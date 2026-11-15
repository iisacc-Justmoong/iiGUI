#include "../src/Platform/Win32ParentHook.h"
#include <commctrl.h>
#include <memory>
#include <stdexcept>

namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
HWND make_parent(){
    WNDCLASSEXW type{};type.cbSize=sizeof(type);type.lpszClassName=L"ExternalParentTest";
    type.lpfnWndProc=[](HWND,UINT,WPARAM,LPARAM)->LRESULT{return 0;};
    check(RegisterClassExW(&type),"Cannot register parent fixture");
    auto window=CreateWindowExW(0,L"ExternalParentTest",L"",0,0,0,0,0,HWND_MESSAGE,nullptr,nullptr,nullptr);
    check(window,"Cannot create parent fixture");return window;
}
void cleanup(HWND parent){if(fake_win32::windows.contains(parent))DestroyWindow(parent);UnregisterClassW(L"ExternalParentTest",nullptr);}

void forwards_and_detaches_without_owning_parent(){
    HWND parent=make_parent();int inputs=0;
    {
        ii::gui::detail::Win32ParentHook hook(parent,[&](HWND,UINT message,WPARAM,LPARAM)->std::optional<LRESULT>{
            if(message==WM_APP+1){++inputs;return 37;}return std::nullopt;
        });
        check(fake_subclass::dispatch(parent,WM_APP+1,0,0)==37 && inputs==1,"Handled input was not returned");
        check(fake_subclass::dispatch(parent,WM_APP+2,0,0)==fake_subclass::default_result,"Unrelated input was swallowed");
        check(fake_subclass::count()==1,"Parent hook was not installed");
    }
    check(fake_subclass::count()==0 && fake_win32::windows.contains(parent),"Detachment destroyed or altered the parent");
    cleanup(parent);
}
void parent_destruction_retires_registration_once(){
    HWND parent=make_parent();int notifications=0;
    auto hook=std::make_unique<ii::gui::detail::Win32ParentHook>(parent,[&](HWND,UINT message,WPARAM,LPARAM)->std::optional<LRESULT>{
        if(message==WM_NCDESTROY)++notifications;return std::nullopt;
    });
    check(fake_subclass::dispatch(parent,WM_NCDESTROY,0,0)==fake_subclass::default_result,"Parent destruction did not reach its owner");
    check(notifications==1 && fake_subclass::count()==0 && !hook->attached(),"Parent destruction did not retire the hook");
    hook.reset();cleanup(parent);
}
void callback_can_release_hook(){
    HWND parent=make_parent();std::unique_ptr<ii::gui::detail::Win32ParentHook> hook;
    hook=std::make_unique<ii::gui::detail::Win32ParentHook>(parent,[&](HWND,UINT,WPARAM,LPARAM)->std::optional<LRESULT>{hook.reset();return 51;});
    check(fake_subclass::dispatch(parent,WM_APP+3,0,0)==51 && !hook && fake_subclass::count()==0,"Reentrant removal failed");
    cleanup(parent);
}
void rejects_foreign_thread(){
    HWND parent=make_parent();fake_win32::current_thread(2);bool rejected=false;
    try{ii::gui::detail::Win32ParentHook hook(parent,{});}catch(const std::exception&){rejected=true;}
    fake_win32::current_thread(1);
    check(rejected && fake_subclass::count()==0,"Foreign-thread subclass was accepted");cleanup(parent);
}
void boundary_covers_forwarded_native_callback(){
    HWND parent=make_parent();int depth=0;
    auto enter=[](void* value)noexcept{++*static_cast<int*>(value);};
    auto leave=[](void* value)noexcept{--*static_cast<int*>(value);};
    {
        ii::gui::detail::Win32ParentHook hook(parent,{},&depth,enter,leave);
        fake_subclass::on_default=[&]{check(depth==1,"The native owner ran outside the callback lifetime boundary");};
        check(fake_subclass::dispatch(parent,WM_APP+4,0,0)==fake_subclass::default_result && depth==0,
              "The forwarded native callback did not leave its lifetime boundary");
        fake_subclass::on_default={};
    }
    cleanup(parent);
}
}
int main(){forwards_and_detaches_without_owning_parent();parent_destruction_retires_registration_once();callback_can_release_hook();rejects_foreign_thread();boundary_covers_forwarded_native_callback();}
