#pragma once

#include <windows.h>
#include <cstdint>
#include <functional>
#include <stdexcept>
#include <string>
#include <utility>

namespace ii::gui::detail {

// A message-only window survives destruction of the presented window. The
// owner keeps this endpoint alive until the close observer has been delivered.
// No timer is active in the normal path; the one-shot timer is a recovery for
// a full posted-message queue, not a recurring lifecycle check.
class Win32DeferredClose final {
public:
    Win32DeferredClose()
        : instance_(GetModuleHandleW(nullptr)),
          class_name_(L"iiGUI.ExternalClose."+std::to_wstring(reinterpret_cast<std::uintptr_t>(this))) {
        WNDCLASSEXW type{};
        type.cbSize=sizeof(type); type.lpfnWndProc=procedure;
        type.hInstance=instance_; type.lpszClassName=class_name_.c_str();
        if(!RegisterClassExW(&type)) throw std::runtime_error("Cannot register external-close endpoint");
        registered_=true;
        window_=CreateWindowExW(0,class_name_.c_str(),L"",0,0,0,0,0,
                                HWND_MESSAGE,nullptr,instance_,this);
        if(!window_) {
            UnregisterClassW(class_name_.c_str(),instance_); registered_=false;
            throw std::runtime_error("Cannot create external-close endpoint");
        }
    }
    ~Win32DeferredClose() {
        if(window_) { KillTimer(window_,timer_id); DestroyWindow(window_); }
        if(registered_) UnregisterClassW(class_name_.c_str(),instance_);
    }
    Win32DeferredClose(const Win32DeferredClose&)=delete;
    Win32DeferredClose& operator=(const Win32DeferredClose&)=delete;

    [[nodiscard]] bool schedule(std::function<void()> callback) {
        if(!window_ || callback_ || !callback) return false;
        callback_=std::move(callback);
        if(PostMessageW(window_,close_message,0,0)) return true;
        if(SetTimer(window_,timer_id,1,nullptr)) return true;
        callback_={};
        return false;
    }

private:
    static constexpr UINT close_message=WM_APP+0x14a;
    static constexpr UINT_PTR timer_id=0x4949434c;
    HINSTANCE instance_{};
    std::wstring class_name_;
    HWND window_{};
    bool registered_=false;
    std::function<void()> callback_;

    static LRESULT CALLBACK procedure(HWND window,UINT message,WPARAM w,LPARAM l) noexcept {
        auto* self=reinterpret_cast<Win32DeferredClose*>(GetWindowLongPtrW(window,GWLP_USERDATA));
        if(message==WM_NCCREATE) {
            self=static_cast<Win32DeferredClose*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);
            SetWindowLongPtrW(window,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(self));
        }
        if(!self) return DefWindowProcW(window,message,w,l);
        if(message==close_message || (message==WM_TIMER && w==timer_id)) {
            if(message==WM_TIMER) KillTimer(window,timer_id);
            auto callback=std::exchange(self->callback_,{});
            // The callback may destroy self and this window. Do not touch self
            // after invoking it, including on the exceptional path.
            try { if(callback) callback(); } catch(...) {}
            return 0;
        }
        if(message==WM_NCDESTROY) {
            SetWindowLongPtrW(window,GWLP_USERDATA,0);
            self->window_=nullptr;
        }
        return DefWindowProcW(window,message,w,l);
    }
};
} // namespace ii::gui::detail
