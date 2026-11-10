#include <iiGUI/Host.h>
#include <iiGUI/NativeWidgets.h>
#include <iiGUI/AtSpiHost.h>
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <vector>

void fake_wayland_clear_wakes();
void fake_wayland_set_scale(double);
void fake_wayland_output_available(bool);
void fake_wayland_reset_resize_events();
int fake_wayland_resize_events();
void fake_wayland_reject_next_open();
void fake_wayland_reject_next_dispatch();
void fake_wayland_reset_waits();
void fake_wayland_on_next_wait(std::function<void()>);
void fake_wayland_complete_dialog();
void fake_wayland_request_all_close();
void fake_wayland_emit_ordered_input();
void fake_wayland_clear_input_log();
std::vector<int> fake_wayland_input_log();
namespace {
using ii::gui::Host;
void check(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
void good(ii::core::Result<void> result){if(!result){if(result.error().cause)std::rethrow_exception(result.error().cause);throw std::runtime_error(result.error().message);}}
struct Cleanup final {~Cleanup(){fake_wayland_clear_wakes();}};

void independent_windows_and_application_lifecycle(){
    Cleanup cleanup;fake_wayland_reset_waits();ii::gui::detail::AtSpiHost::reset();
    Host driver,first,second;good(driver.set_application_driver(true));
    std::vector<int> activity;good(driver.set_application_activity_handler([&](bool value){activity.push_back(value?1:0);}));
    int opened=0,closed=0,decisions=0,driver_closed=0,child_tasks=0;
    bool veto_preserved=false,sibling_preserved=false,task_ordered=false;
    std::exception_ptr child_error;
    auto report=[&](const ii::core::Error& error){
        if(!child_error)child_error=error.cause?error.cause:std::make_exception_ptr(std::runtime_error(error.message));
        (void)driver.request_close();
    };
    good(first.set_failure_handler(report));good(second.set_failure_handler(report));
    ii::gui::HostCallbacks callbacks;
    callbacks.opened=[&]{
        auto hidden=driver.snapshot();check(hidden&&!hidden->visible&&!hidden->closed,"The application driver was visible or closed");
        auto title=driver.set_title("No window");check(!title&&title.error().code==ii::core::ErrorCode::unsupported,"Driver accepted a window-only operation");
        ii::gui::HostCallbacks one;
        one.opened=[&]{++opened;good(first.request_close());auto snapshot=first.snapshot();veto_preserved=snapshot&&!snapshot->closed;good(first.request_close());good(first.request_close());};
        one.close_requested=[&]{return ++decisions>1;};
        one.closed=[&]{++closed;auto snapshot=second.snapshot();sibling_preserved=snapshot&&!snapshot->closed;};
        ii::gui::HostCallbacks two;
        two.opened=[&]{++opened;auto submitted=second.executor().post([&]{++child_tasks;task_ordered=closed==1;good(second.request_close());});check(bool(submitted),"A secondary window could not post to the shared loop");};
        two.closed=[&]{++closed;good(driver.request_close());};
        good(first.show(driver,std::move(one)));good(second.show(driver,std::move(two)));
    };
    callbacks.closed=[&]{++driver_closed;};
    good(driver.run(std::move(callbacks)));
    if(child_error)std::rethrow_exception(child_error);
    check(opened==2&&closed==2&&decisions==2&&driver_closed==1&&child_tasks==1&&veto_preserved&&sibling_preserved&&task_ordered,
          "Independent window, task, or app callbacks were lost or misordered");
    check(activity==std::vector<int>({1,0}),"Application activity was not aggregated across windows");
    check(ii::gui::detail::AtSpiHost::created==1&&ii::gui::detail::AtSpiHost::opened==2&&ii::gui::detail::AtSpiHost::closed==2,
          "The application did not share one accessibility service across its windows");
}

void failed_or_unconfigured_child_does_not_stop_driver(){
    Cleanup cleanup;fake_wayland_reset_waits();
    Host driver,failed,early;good(driver.set_application_driver(true));
    int failed_closed=0,early_opened=0,early_closed=0;
    ii::gui::HostCallbacks callbacks;
    callbacks.opened=[&]{
        fake_wayland_reject_next_open();
        ii::gui::HostCallbacks rejected;rejected.closed=[&]{++failed_closed;};
        auto result=failed.show(driver,std::move(rejected));
        check(!result&&result.error().cause,"A failed Wayland connection was reported as opened");
        ii::gui::HostCallbacks pending;pending.opened=[&]{++early_opened;};
        pending.closed=[&]{++early_closed;good(driver.request_close());};
        good(early.show(driver,std::move(pending)));
        good(early.request_close());
    };
    good(driver.run(std::move(callbacks)));
    check(failed_closed==0&&early_opened==0&&early_closed==1,"Failure rollback or pre-configure close was incorrect");
}

void standalone_window_still_runs(){
    Cleanup cleanup;fake_wayland_reset_waits();Host host;int opened=0,closed=0;
    ii::gui::HostCallbacks callbacks;
    callbacks.opened=[&]{++opened;
        fake_wayland_output_available(false);
        auto pending=host.display_pixels();
        check(!pending && pending.error().code==ii::core::ErrorCode::busy,"Undetermined Wayland display was guessed");
        fake_wayland_output_available(true);
        auto pixels=host.display_pixels();
        check(pixels && *pixels==ii::gui::DisplayPixels{1920,1080},"Wayland display pixels changed with window scale");
        good(host.request_close());};callbacks.closed=[&]{++closed;};
    good(host.run(std::move(callbacks)));
    check(opened==1&&closed==1,"Standalone Wayland lifecycle regressed");
}

void closing_the_driver_stops_later_window_publication(){
    Cleanup cleanup;fake_wayland_reset_waits();Host driver,first,second,nested;good(driver.set_application_driver(true));
    int first_opened=0,second_opened=0;bool nested_rejected=false;
    ii::gui::HostCallbacks callbacks;
    callbacks.opened=[&]{
        ii::gui::HostCallbacks one;one.opened=[&]{
            ++first_opened;good(driver.request_close());
            auto result=nested.show(first);
            nested_rejected=!result&&result.error().code==ii::core::ErrorCode::closed;
        };
        ii::gui::HostCallbacks two;two.opened=[&]{++second_opened;};
        good(first.show(driver,std::move(one)));good(second.show(driver,std::move(two)));
    };
    good(driver.run(std::move(callbacks)));
    check(first_opened==1&&second_opened==0&&nested_rejected,"A later window opened or was admitted after application shutdown was accepted");
}

void child_failure_is_reported_without_hiding_the_cause(){
    Cleanup cleanup;fake_wayland_reset_waits();Host driver,child;good(driver.set_application_driver(true));
    int failures=0,driver_closed=0;bool original_cause=false;
    good(child.set_failure_handler([&](const ii::core::Error& error){
        ++failures;original_cause=bool(error.cause);good(driver.request_close());
    }));
    ii::gui::HostCallbacks callbacks;
    callbacks.opened=[&]{good(child.show(driver));fake_wayland_reject_next_dispatch();};
    callbacks.closed=[&]{++driver_closed;};
    good(driver.run(std::move(callbacks)));
    check(failures==1&&driver_closed==1&&original_cause,"A child dispatch failure or its cause was not delivered to its application");
}

void dialog_result_after_child_close_is_not_delivered(){
    Cleanup cleanup;fake_wayland_reset_waits();Host driver;auto child=std::make_unique<Host>();good(driver.set_application_driver(true));
    int completed=0,closed=0;
    ii::gui::HostCallbacks callbacks;
    callbacks.opened=[&]{
        ii::gui::HostCallbacks window;
        window.opened=[&]{good(child->choose_file({},[&](auto){++completed;}));good(child->request_close());};
        window.closed=[&]{++closed;child.reset();fake_wayland_complete_dialog();good(driver.request_close());};
        good(child->show(driver,std::move(window)));
    };
    good(driver.run(std::move(callbacks)));
    check(closed==1&&completed==0,"A file dialog result was delivered after its window closed");
}

void simultaneous_native_and_programmatic_close_are_coalesced(){
    Cleanup cleanup;fake_wayland_reset_waits();Host driver,child;good(driver.set_application_driver(true));
    int decisions=0,closed=0;
    ii::gui::HostCallbacks callbacks;
    callbacks.opened=[&]{
        ii::gui::HostCallbacks window;
        window.opened=[&]{
            fake_wayland_request_all_close();
            auto submitted=child.executor().post([&]{good(child.request_close());});
            check(bool(submitted),"Cannot queue a programmatic close");
        };
        window.close_requested=[&]{++decisions;return true;};
        window.closed=[&]{++closed;good(driver.request_close());};
        good(child.show(driver,std::move(window)));
    };
    good(driver.run(std::move(callbacks)));
    check(decisions==1&&closed==1,"One native and programmatic close produced duplicate approvals");
}
void input_and_ime_keep_native_order(){
    Cleanup cleanup;fake_wayland_reset_waits();fake_wayland_clear_input_log();ii::gui::detail::AtSpiHost::reset();Host host;
    ii::gui::HostCallbacks callbacks;
    callbacks.opened=[&]{
        auto mounted=host.mount_widgets([](ii::gui::WidgetBuilder& builder)->ii::core::Result<void>{
            auto field=builder.input_field({},"field");if(!field)return std::unexpected(field.error());return {};
        });
        check(bool(mounted),"Wayland mock could not mount a field");fake_wayland_emit_ordered_input();
        auto submitted=host.executor().post([&]{good(host.request_close());});check(bool(submitted),"Cannot queue host close");
    };
    good(host.run(std::move(callbacks)));
    check(fake_wayland_input_log()==std::vector<int>({1,2,3,4}),"Wayland host reordered IME and keyboard events");
    check(ii::gui::detail::AtSpiHost::created==1&&ii::gui::detail::AtSpiHost::opened==1&&ii::gui::detail::AtSpiHost::published==1&&ii::gui::detail::AtSpiHost::closed==1,
          "A committed Wayland mount was not published and retired with its window");
}
void accessibility_mount_is_transactional(){
    Cleanup cleanup;fake_wayland_reset_waits();ii::gui::detail::AtSpiHost::reset();Host host;
    ii::gui::HostCallbacks callbacks;callbacks.opened=[&]{
        auto build=[](ii::gui::WidgetBuilder& builder)->ii::core::Result<void>{auto item=builder.button({},"button",{.text="Save"});if(!item)return std::unexpected(item.error());return {};};
        check(bool(host.mount_widgets(build)),"Cannot mount first accessible view");
        auto rejected=host.mount_widgets([](ii::gui::WidgetBuilder&)->ii::core::Result<void>{return ii::core::fail(ii::core::ErrorCode::failed,"Expected failure");});
        check(!rejected&&ii::gui::detail::AtSpiHost::published==1&&ii::gui::detail::AtSpiHost::cleared==0,"A rejected mount retired the visible accessible content");
        check(bool(host.mount_widgets(build)),"Cannot replace accessible view");
        check(ii::gui::detail::AtSpiHost::published==2&&ii::gui::detail::AtSpiHost::cleared==1,"A replacement did not retire its predecessor");
        good(host.unmount_widgets());check(ii::gui::detail::AtSpiHost::cleared==2,"Unmount did not retire accessible content");good(host.request_close());
    };
    good(host.run(std::move(callbacks)));
}
void display_scale_changes_are_event_driven(){
    Cleanup cleanup;fake_wayland_reset_waits();fake_wayland_reset_resize_events();Host host;
    int notifications=0;bool applied=false,duplicate_ignored=false;
    ii::gui::HostCallbacks callbacks;
    callbacks.resized=[&](auto){++notifications;auto snapshot=host.snapshot();applied=snapshot&&snapshot->backing_scale==1.25;};
    callbacks.opened=[&]{
        auto mounted=host.mount_widgets([](ii::gui::WidgetBuilder& builder)->ii::core::Result<void>{
            auto label=builder.label({},"scale-label",{.text="Scale"});if(!label)return std::unexpected(label.error());return {};
        });check(bool(mounted),"Cannot mount display scale fixture");
        fake_wayland_on_next_wait([&]{
            fake_wayland_set_scale(1.25);
            fake_wayland_on_next_wait([&]{
                const auto before=fake_wayland_resize_events();
                fake_wayland_set_scale(1.25);
                fake_wayland_on_next_wait([&,before]{duplicate_ignored=fake_wayland_resize_events()==before;good(host.request_close());});
            });
        });
    };
    good(host.run(std::move(callbacks)));
    check(applied&&notifications==1&&duplicate_ignored,"Scale transition was lost or repeated without a change");
}
void accessibility_value_refresh_is_deferred(){
    Cleanup cleanup;fake_wayland_reset_waits();ii::gui::detail::AtSpiHost::reset();Host host;
    ii::gui::Widget slider;int before=0;bool refreshed=false;
    ii::gui::HostCallbacks callbacks;callbacks.opened=[&]{
        auto mounted=host.mount_widgets([&](ii::gui::WidgetBuilder& builder)->ii::core::Result<void>{
            auto item=builder.slider({},"slider",{});if(!item)return std::unexpected(item.error());slider=*item;return {};
        });check(bool(mounted),"Cannot mount slider for value refresh");
        fake_wayland_on_next_wait([&]{before=ii::gui::detail::AtSpiHost::updated;good(slider.set_value(.6));good(slider.set_value(.7));
            fake_wayland_on_next_wait([&]{refreshed=ii::gui::detail::AtSpiHost::updated==before+1;good(host.request_close());});});
    };
    good(host.run(std::move(callbacks)));check(refreshed,"Value updates did not schedule one deferred accessibility refresh");
}
} // namespace
int main(){
    try{independent_windows_and_application_lifecycle();failed_or_unconfigured_child_does_not_stop_driver();standalone_window_still_runs();closing_the_driver_stops_later_window_publication();child_failure_is_reported_without_hiding_the_cause();dialog_result_after_child_close_is_not_delivered();simultaneous_native_and_programmatic_close_are_coalesced();input_and_ime_keep_native_order();accessibility_mount_is_transactional();accessibility_value_refresh_is_deferred();display_scale_changes_are_event_driven();}
    catch(const std::exception& error){std::fprintf(stderr,"%s\n",error.what());return 1;}
}
