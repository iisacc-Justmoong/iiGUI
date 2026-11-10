#include "../src/Platform/HostCallback.h"
#include <functional>
#include <memory>
#include <stdexcept>

namespace {
void check(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }

void callback_survives_own_removal() {
    auto value = std::make_shared<int>(42);
    std::weak_ptr<int> weak = value;
    std::function<void()> opened;
    opened = [keep = value, &opened, weak] {
        opened = {};
        check(!weak.expired() && *keep == 42, "The active opened callback lost its capture during close");
    };
    value.reset();
    ii::gui::detail::deliver_host_opened(opened);
    check(weak.expired(), "The opened callback capture outlived its invocation");
    ii::gui::detail::deliver_host_opened(opened);
}

void callback_exception_is_preserved() {
    auto value = std::make_shared<int>(7);
    std::weak_ptr<int> weak = value;
    std::function<void()> opened;
    opened = [keep = value, &opened] { opened = {}; throw std::runtime_error("opened failure"); };
    value.reset();
    bool caught = false;
    try { ii::gui::detail::deliver_host_opened(opened); }
    catch (const std::runtime_error&) { caught = true; }
    check(caught && weak.expired(), "An opened callback exception or capture cleanup was lost");
}

void child_close_is_deferred_and_retains_its_capture() {
    auto owner = std::make_shared<int>(23);
    std::weak_ptr<int> weak = owner;
    std::function<void()> delivered;
    bool invoked = false;
    std::function<void()> callback = [keep = owner, &invoked] { invoked = *keep == 23; };
    owner.reset();
    ii::gui::detail::defer_host_closed(std::move(callback),
        [&](std::function<void()> task) { delivered = std::move(task); });
    check(!invoked && delivered && !weak.expired(), "A child close ran inline or lost its capture");
    delivered();
    delivered = {};
    check(invoked && weak.expired(), "A deferred child close leaked its capture");
}

void primary_close_waits_for_all_native_callbacks() {
    ii::gui::detail::HostCallbackGate gate;
    int calls = 0;
    std::function<void()> immediate = [&] { ++calls; };
    check(!gate.defer(immediate) && bool(immediate), "An idle gate consumed an immediate close");
    gate.enter(); gate.enter();
    auto owner = std::make_shared<int>(31);
    std::weak_ptr<int> weak = owner;
    std::function<void()> close = [keep = owner, &calls] { check(*keep == 31, "Close capture changed"); ++calls; };
    owner.reset();
    check(gate.defer(close) && !close && !weak.expired(), "The gate lost a pending close");
    check(!gate.leave() && calls == 0, "An inner callback released the close early");
    auto ready = gate.leave();
    check(bool(ready) && calls == 0, "The last callback failed to release the close");
    ready(); ready = {};
    check(calls == 1 && weak.expired() && !gate.leave(), "The close was repeated or retained");
}

void rejected_child_close_is_reported_and_releases_its_capture() {
    auto owner = std::make_shared<int>(43);
    std::weak_ptr<int> weak = owner;
    std::function<void()> closed = [keep = owner] { check(*keep == 43, "Close capture changed"); };
    owner.reset();
    bool submitted = false;
    const bool accepted=ii::gui::detail::try_defer_host_closed(std::move(closed), [&](std::function<void()> task) {
        submitted = bool(task);
        return false;
    });
    closed={};
    check(!accepted && submitted, "A rejected child close was hidden");
    check(weak.expired(), "A rejected child close retained its capture");
    check(ii::gui::detail::try_defer_host_closed({}, [&](auto) { throw std::runtime_error("unexpected submission"); return false; }),
          "An absent close callback was treated as a submission failure");
    bool thrown = false;
    try {
        (void)ii::gui::detail::try_defer_host_closed([] {}, [](auto) -> bool { throw std::runtime_error("queue failure"); });
    } catch (const std::runtime_error&) { thrown = true; }
    check(thrown, "A throwing close queue lost its failure");
}
} // namespace

int main() { callback_survives_own_removal(); callback_exception_is_preserved(); child_close_is_deferred_and_retains_its_capture(); primary_close_waits_for_all_native_callbacks(); rejected_child_close_is_reported_and_releases_its_capture(); }
