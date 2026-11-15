#pragma once
#include <algorithm>
#include <cstdint>
#include <deque>
#include <map>
#include <memory>
#include <string>

#define CALLBACK
using BOOL=int;
using UINT=unsigned int;
using DWORD=unsigned long;
using UINT_PTR=std::uintptr_t;
using WPARAM=std::uintptr_t;
using LPARAM=std::intptr_t;
using LRESULT=std::intptr_t;
using LONG_PTR=std::intptr_t;
using HINSTANCE=void*;
struct FakeWindow;
using HWND=FakeWindow*;
using WNDPROC=LRESULT (*)(HWND,UINT,WPARAM,LPARAM);
using ATOM=unsigned short;
struct CREATESTRUCTW { void* lpCreateParams; };
struct WNDCLASSEXW {
    UINT cbSize{}; UINT style{}; WNDPROC lpfnWndProc{}; int cbClsExtra{}; int cbWndExtra{};
    HINSTANCE hInstance{}; void* hIcon{}; void* hCursor{}; void* hbrBackground{};
    const wchar_t* lpszMenuName{}; const wchar_t* lpszClassName{}; void* hIconSm{};
};
constexpr UINT WM_NCCREATE=0x0081, WM_NCDESTROY=0x0082, WM_TIMER=0x0113, WM_APP=0x8000;
constexpr int GWLP_USERDATA=-21;
constexpr BOOL FALSE=0, TRUE=1;
inline HWND const HWND_MESSAGE=reinterpret_cast<HWND>(static_cast<std::intptr_t>(-3));
struct FakeWindow { WNDPROC procedure{}; LONG_PTR data{}; std::wstring name; };

namespace fake_win32 {
struct Message { HWND window; UINT code; WPARAM w; LPARAM l; };
inline std::map<std::wstring,WNDPROC> classes;
inline std::map<HWND,std::unique_ptr<FakeWindow>> windows;
inline std::map<HWND,UINT_PTR> timers;
inline std::deque<Message> messages;
inline bool post_failure=false, timer_failure=false;
inline DWORD thread_id=1;
inline void current_thread(DWORD value) { thread_id=value; }
inline void fail_posts(bool value) { post_failure=value; }
inline void fail_timers(bool value) { timer_failure=value; }
inline std::size_t live_windows() { return windows.size(); }
inline std::size_t active_timers() { return timers.size(); }
inline void dispatch_all() {
    auto ready=std::move(messages); messages.clear();
    for(const auto& [window,id]:timers) ready.push_back({window,WM_TIMER,id,0});
    while(!ready.empty()) {
        const auto item=ready.front(); ready.pop_front();
        auto it=windows.find(item.window);
        if(it!=windows.end()) it->second->procedure(item.window,item.code,item.w,item.l);
    }
}
}
inline HINSTANCE GetModuleHandleW(const wchar_t*) { return nullptr; }
inline ATOM RegisterClassExW(const WNDCLASSEXW* value) {
    return fake_win32::classes.emplace(value->lpszClassName,value->lpfnWndProc).second?1:0;
}
inline BOOL UnregisterClassW(const wchar_t* name,HINSTANCE) { return fake_win32::classes.erase(name)?TRUE:FALSE; }
inline LONG_PTR GetWindowLongPtrW(HWND window,int) { return window->data; }
inline LONG_PTR SetWindowLongPtrW(HWND window,int,LONG_PTR value) { auto old=window->data;window->data=value;return old; }
inline LRESULT DefWindowProcW(HWND,UINT,WPARAM,LPARAM) { return 0; }
inline HWND CreateWindowExW(unsigned long,const wchar_t* name,const wchar_t*,unsigned long,
                            int,int,int,int,HWND,void*,HINSTANCE,void* parameter) {
    const auto cls=fake_win32::classes.find(name);
    if(cls==fake_win32::classes.end()) return nullptr;
    auto window=std::make_unique<FakeWindow>(); window->procedure=cls->second; window->name=name;
    auto* raw=window.get(); fake_win32::windows.emplace(raw,std::move(window));
    CREATESTRUCTW create{parameter}; raw->procedure(raw,WM_NCCREATE,0,reinterpret_cast<LPARAM>(&create));
    return raw;
}
inline BOOL DestroyWindow(HWND window) {
    auto it=fake_win32::windows.find(window); if(it==fake_win32::windows.end()) return FALSE;
    window->procedure(window,WM_NCDESTROY,0,0);
    fake_win32::timers.erase(window); fake_win32::windows.erase(it); return TRUE;
}
inline BOOL PostMessageW(HWND window,UINT code,WPARAM w,LPARAM l) {
    if(fake_win32::post_failure || !fake_win32::windows.contains(window)) return FALSE;
    fake_win32::messages.push_back({window,code,w,l}); return TRUE;
}
inline DWORD GetWindowThreadProcessId(HWND window,DWORD*) { return fake_win32::windows.contains(window)?1:0; }
inline DWORD GetCurrentThreadId() { return fake_win32::thread_id; }
inline BOOL SendNotifyMessageW(HWND window,UINT code,WPARAM w,LPARAM l) {
    if(!fake_win32::windows.contains(window)) return FALSE;
    fake_win32::messages.push_back({window,code,w,l}); return TRUE;
}
inline UINT_PTR SetTimer(HWND window,UINT_PTR id,UINT,void*) {
    if(fake_win32::timer_failure || !fake_win32::windows.contains(window)) return 0;
    fake_win32::timers[window]=id; return id;
}
inline BOOL KillTimer(HWND window,UINT_PTR) { return fake_win32::timers.erase(window)?TRUE:FALSE; }
