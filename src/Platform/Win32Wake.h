#pragma once
#include <windows.h>

namespace ii::gui::detail {
// The message-only retry paths are used only when PostMessage cannot enqueue.
// SetTimer must run on the window owner; SendNotifyMessage is asynchronous
// only from a different thread, so never use it on the owner thread.
[[nodiscard]] inline bool post_win32_wake(HWND window,UINT message,UINT_PTR retry_timer) noexcept {
    if(PostMessageW(window,message,0,0)) return true;
    const auto owner=GetWindowThreadProcessId(window,nullptr);
    if(!owner) return false;
    if(owner!=GetCurrentThreadId()) return SendNotifyMessageW(window,message,0,0)!=FALSE;
    return SetTimer(window,retry_timer,1,nullptr)!=0;
}
} // namespace ii::gui::detail
