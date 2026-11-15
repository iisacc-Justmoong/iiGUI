#include "../src/Platform/Win32DeferredClose.h"
#include "../src/Platform/ExternalCloseBarrier.h"
#include "../src/Platform/Win32Wake.h"
#include <windows.h>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
void check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }

void deferred_delivery_can_destroy_owner() {
    using ii::gui::detail::Win32DeferredClose;
    std::vector<std::string> events;
    auto endpoint=std::make_unique<Win32DeferredClose>();
    check(endpoint->schedule([&]{ events.push_back("closed"); endpoint.reset(); }), "close was not scheduled");
    events.push_back("native callback returned");
    check(events.size()==1, "close ran inside the native callback");
    fake_win32::dispatch_all();
    check(events==std::vector<std::string>({"native callback returned","closed"}), "close delivery order is incorrect");
    check(!endpoint && fake_win32::live_windows()==0, "close observer did not release the endpoint");
}

void fallback_and_exactly_once() {
    using ii::gui::detail::Win32DeferredClose;
    Win32DeferredClose endpoint;
    int calls=0;
    fake_win32::fail_posts(true);
    check(endpoint.schedule([&]{++calls;}), "timer fallback was not scheduled");
    check(!endpoint.schedule([&]{calls+=100;}), "a second close replaced the pending one");
    fake_win32::fail_posts(false);
    fake_win32::dispatch_all();
    fake_win32::dispatch_all();
    check(calls==1 && fake_win32::active_timers()==0, "fallback did not deliver exactly once");
}

void admission_failure_and_destruction() {
    using ii::gui::detail::Win32DeferredClose;
    int calls=0;
    {
        Win32DeferredClose endpoint;
        fake_win32::fail_posts(true);
        fake_win32::fail_timers(true);
        check(!endpoint.schedule([&]{++calls;}), "failed admission was reported as success");
        fake_win32::fail_posts(false);
        fake_win32::fail_timers(false);
        check(endpoint.schedule([&]{++calls;}), "a failed admission left the endpoint occupied");
    }
    fake_win32::dispatch_all();
    check(calls==0 && fake_win32::live_windows()==0, "destroyed endpoint delivered a stale callback");
}

void nested_native_dispatch_waits_for_outer_callback() {
    using ii::gui::detail::Win32DeferredClose;
    ii::gui::detail::ExternalCloseBarrier barrier;
    Win32DeferredClose endpoint;
    int calls=0;
    const auto deliver=[&]{if(barrier.admit()) ++calls;};
    barrier.enter(); barrier.enter();
    check(endpoint.schedule(deliver), "nested close was not scheduled");
    fake_win32::dispatch_all();
    check(calls==0 && fake_win32::active_timers()==0, "nested loop destroyed an active owner");
    check(!barrier.leave(), "inner callback rearmed close too early");
    check(barrier.leave(), "outer callback lost deferred close");
    check(endpoint.schedule(deliver), "deferred close was not rearmed");
    fake_win32::dispatch_all();
    check(calls==1 && !barrier.leave(), "deferred close was not delivered exactly once");
}

void full_queue_wakeup_fallbacks() {
    int received=0;
    WNDCLASSEXW type{}; type.cbSize=sizeof(type); type.lpszClassName=L"WakeTest";
    type.lpfnWndProc=[](HWND window,UINT code,WPARAM w,LPARAM l)->LRESULT {
        auto* counter=reinterpret_cast<int*>(GetWindowLongPtrW(window,GWLP_USERDATA));
        if(code==WM_NCCREATE) {
            counter=static_cast<int*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);
            SetWindowLongPtrW(window,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(counter));
        } else if(counter && (code==WM_APP+1 || (code==WM_TIMER && w==31))) {
            if(code==WM_TIMER) KillTimer(window,31);
            ++*counter;
        }
        return 0;
    };
    check(RegisterClassExW(&type), "cannot create wake test class");
    HWND window=CreateWindowExW(0,L"WakeTest",L"",0,0,0,0,0,HWND_MESSAGE,nullptr,nullptr,&received);
    check(window, "cannot create wake test window");
    fake_win32::fail_posts(true);
    fake_win32::current_thread(2);
    check(ii::gui::detail::post_win32_wake(window,WM_APP+1,31), "worker fallback failed");
    fake_win32::dispatch_all();
    check(received==1 && fake_win32::active_timers()==0, "worker fallback did not deliver a message");
    fake_win32::current_thread(1);
    check(ii::gui::detail::post_win32_wake(window,WM_APP+1,31), "owner fallback failed");
    fake_win32::dispatch_all();
    check(received==2 && fake_win32::active_timers()==0, "owner fallback was not one-shot");
    fake_win32::fail_timers(true);
    check(!ii::gui::detail::post_win32_wake(window,WM_APP+1,31), "failed wake admission was reported as success");
    fake_win32::fail_posts(false); fake_win32::fail_timers(false);
    DestroyWindow(window); UnregisterClassW(L"WakeTest",nullptr);
}
}

int main() {
    deferred_delivery_can_destroy_owner();
    fallback_and_exactly_once();
    admission_failure_and_destruction();
    nested_native_dispatch_waits_for_outer_callback();
    full_queue_wakeup_fallbacks();
}
