#include "../src/Core/DeferredDriverStop.h"
#include <iiSDKCore/Runtime/Application.h>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using ii::gui::detail::DeferredDriverStop;
void check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
void good(ii::core::Result<void> result) { if (!result) throw std::runtime_error(result.error().message); }

void application_requests_worker_before_host() {
    ii::runtime::Application app;
    check(app.workers().stats().workers==0 && app.workers().allocated_slots()==0,
          "Default application unexpectedly owns worker resources");
    check(app.post([]{}).error==ii::runtime::SubmitError::overloaded,
          "Unconfigured application accepted background work");
    check(bool(app.workers().resize_workers(1)),"Application could not request its worker");
    int completed=0;
    check(bool(app.post([&]{++completed;})) && bool(app.workers().wait_idle()) && completed==1,
          "Requested worker did not execute the application task");
    good(app.set_driver({[](const auto& start) { return start(); },[]() -> ii::core::Result<void> { return {}; }}));
    check(bool(app.exec()) && app.workers().stats().state==ii::runtime::RuntimeState::stopped,
          "Application did not retire its explicitly requested worker");
}

void closed_host_retires_after_native_callback_returns() {
    ii::runtime::Application app({}, {.workers={.workers=1}});
    std::string order;
    good(app.attach_before_driver({"app",[&](auto&) -> ii::core::Result<void> { order+='A'; return {}; },
                                   [&]() -> ii::core::Result<void> { order+='a'; return {}; }}));
    good(app.attach({"view",[&](auto&) -> ii::core::Result<void> { order+='V'; return {}; },
                     [&]() -> ii::core::Result<void> { order+='v'; return {}; }}));
    good(app.set_driver({[&](const auto& start) -> ii::core::Result<void> {
        good(start());
        auto stop=std::make_shared<DeferredDriverStop>([&] { good(app.driver_stopped()); });
        auto delivery=stop->delivery();
        order+='C';
        check(app.state()==ii::runtime::ApplicationState::running && order=="AVC",
              "Application owners retired inside the native callback");
        delivery();
        check(app.state()==ii::runtime::ApplicationState::stopped && order=="AVCva",
              "Deferred host completion did not retire application owners");
        delivery(); stop->cancel();
        check(order=="AVCva", "Deferred completion ran more than once");
        return {};
    },[]() -> ii::core::Result<void> { return {}; }}));
    check(bool(app.exec()) && order=="AVCva", "Core repeated teardown after the driver returned");
}

void cancelled_delivery_releases_its_capture() {
    int calls=0;
    auto capture=std::make_shared<int>(42);
    std::weak_ptr<int> weak=capture;
    auto stop=std::make_shared<DeferredDriverStop>([keep=capture,&calls] { calls+=*keep; });
    capture.reset();
    auto delivery=stop->delivery();
    stop->cancel();
    check(weak.expired(), "Cancelling a returned driver retained application state");
    stop.reset(); delivery();
    check(calls==0, "A late native delivery reached a cancelled application");
}

void completion_retains_itself_and_rejects_reentry() {
    int calls=0;
    std::shared_ptr<DeferredDriverStop> stop;
    std::function<void()> delivery;
    stop=std::make_shared<DeferredDriverStop>([&] { ++calls; stop.reset(); delivery(); });
    delivery=stop->delivery();
    delivery(); delivery();
    check(calls==1, "Reentrant delivery repeated or destroyed the active completion");
}

void closed_observer_finishes_before_completion_is_queued() {
    int completions=0;
    auto stop=std::make_shared<DeferredDriverStop>([&] { ++completions; });
    std::vector<DeferredDriverStop::Task> queue;
    ii::gui::detail::notify_closed_then_schedule([&] {
        // Simulate a nested event loop draining every task already queued.
        for (auto& task : queue) task();
        check(queue.empty() && completions==0, "Driver completion was queued during the closed observer");
    }, [&] { queue.push_back(stop->delivery()); });
    check(queue.size()==1 && completions==0, "Driver completion was not deferred after the closed observer");
    queue.front()();
    check(completions==1, "Deferred driver completion was not delivered");
}

void throwing_closed_observer_still_schedules_completion() {
    int completions=0;
    auto stop=std::make_shared<DeferredDriverStop>([&] { ++completions; });
    DeferredDriverStop::Task delivery;
    bool caught=false;
    try {
        ii::gui::detail::notify_closed_then_schedule(
            [] { throw std::runtime_error("closed observer failure"); },
            [&] { delivery=stop->delivery(); });
    } catch (const std::runtime_error& error) {
        caught=std::string(error.what())=="closed observer failure";
    }
    check(caught && bool(delivery) && completions==0, "Observer failure lost completion or the original exception");
    delivery();
    check(completions==1, "Completion was not delivered after an observer failure");
}
}

int main() {
    application_requests_worker_before_host();
    closed_host_retires_after_native_callback_returns();
    cancelled_delivery_releases_its_capture();
    completion_retains_itself_and_rejects_reentry();
    closed_observer_finishes_before_completion_is_queued();
    throwing_closed_observer_still_schedules_completion();
}
