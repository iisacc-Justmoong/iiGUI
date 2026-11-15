#pragma once
#include <windows.h>
#include <commctrl.h>
#include <functional>
#include <optional>
#include <stdexcept>
#include <utility>

namespace ii::gui::detail {
// Observe a borrowed window on its owning thread through the system subclass
// chain. The owner retains the window; detaching never destroys it.
class Win32ParentHook final {
public:
    using Handler=std::function<std::optional<LRESULT>(HWND,UINT,WPARAM,LPARAM)>;
    using Boundary=void(*)(void*) noexcept;
    Win32ParentHook(HWND parent,Handler handler,void* boundary_context=nullptr,Boundary enter=nullptr,Boundary leave=nullptr)
        :parent_(parent),handler_(std::move(handler)),boundary_context_(boundary_context),enter_(enter),leave_(leave) {
        if(bool(enter_)!=bool(leave_))throw std::invalid_argument("External parent callback boundary is incomplete");
        if(!parent_ || GetWindowThreadProcessId(parent_,nullptr)!=GetCurrentThreadId())
            throw std::invalid_argument("External parent must belong to the owner thread");
        if(!SetWindowSubclass(parent_,procedure,id(),reinterpret_cast<DWORD_PTR>(this))) {
            parent_=nullptr;
            throw std::runtime_error("Cannot observe the external parent window");
        }
    }
    ~Win32ParentHook(){detach();}
    Win32ParentHook(const Win32ParentHook&)=delete;
    Win32ParentHook& operator=(const Win32ParentHook&)=delete;
    [[nodiscard]] bool attached()const noexcept{return parent_!=nullptr;}
    void detach()noexcept{
        auto parent=std::exchange(parent_,nullptr);
        if(parent)RemoveWindowSubclass(parent,procedure,id());
    }
private:
    HWND parent_{};
    Handler handler_;
    void* boundary_context_{};
    Boundary enter_{},leave_{};
    UINT_PTR id()const noexcept{return reinterpret_cast<UINT_PTR>(this);}
    static LRESULT CALLBACK procedure(HWND parent,UINT message,WPARAM w,LPARAM l,
                                      UINT_PTR id,DWORD_PTR context)noexcept {
        auto* self=reinterpret_cast<Win32ParentHook*>(context);
        if(!self)return DefSubclassProc(parent,message,w,l);
        auto* boundary=self->boundary_context_;
        auto enter=self->enter_;auto leave=self->leave_;
        if(enter)enter(boundary);
        struct Exit final {void* context;Boundary leave;~Exit(){if(leave)leave(context);}} exit{boundary,leave};
        if(message==WM_NCDESTROY){
            self->parent_=nullptr;
            RemoveWindowSubclass(parent,procedure,id);
        }
        // Handler code may retire the hook. Do not access self after copying it.
        try {
            auto handler=self->handler_;
            if(handler){
                auto result=handler(parent,message,w,l);
                // The native owner must always finish destroying its window.
                if(result && message!=WM_NCDESTROY)return *result;
            }
        }catch(...){}
        return DefSubclassProc(parent,message,w,l);
    }
};
} // namespace ii::gui::detail
