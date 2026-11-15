#include <iiGUI/Host.h>
#include <iiGUI/NativeWidgets.h>
#include <windows.h>
#include <memory>
#include <string>

namespace {
LRESULT CALLBACK parent_procedure(HWND window,UINT message,WPARAM w,LPARAM l){return DefWindowProcW(window,message,w,l);}
}

// Hardware integration test: a hidden foreign HWND owns its message loop and
// never displays a user-facing window. A Windows runner is required to run it.
int main(int argc,char**){
    const auto instance=GetModuleHandleW(nullptr);
    WNDCLASSW type{};type.hInstance=instance;type.lpfnWndProc=parent_procedure;type.lpszClassName=L"iiGUI.EmbeddedTest";
    if(!RegisterClassW(&type))return 1;
    HWND parent=CreateWindowExW(0,type.lpszClassName,L"External owner",WS_POPUP,0,0,400,240,nullptr,nullptr,instance,nullptr);
    if(!parent)return 2;
    const auto original_procedure=GetWindowLongPtrW(parent,GWLP_WNDPROC);
    auto host=std::make_unique<ii::gui::Host>();
    std::string failure;bool opened=false,closed=false;int resizes=0,activations=0;
    const bool close_parent=argc>1;
    auto attached=host->attach_external(parent,{
        .opened=[&]{
            opened=true;
            auto mounted=host->mount_widgets([&](ii::gui::WidgetBuilder& builder)->ii::core::Result<void>{
                auto root=builder.vstack({},"root",{});if(!root)return std::unexpected(root.error());
                auto button=builder.button(*root,"replace",{.text="Replace"},[&]{
                    ++activations;
                    if(!host->unmount_widgets()){failure="Cannot unmount during input";return;}
                    auto replacement=host->mount_widgets([](ii::gui::WidgetBuilder& next)->ii::core::Result<void>{
                        auto label=next.label({},"new",{.text="Replacement"});
                        if(!label)return std::unexpected(label.error());return {};
                    });
                    if(!replacement)failure="Cannot remount during input";
                });
                if(!button)return std::unexpected(button.error());
                return {};
            });
            if(!mounted)failure="Cannot mount transparent native widgets";
            if(mounted){
                auto image=host->capture_canvas();
                if(!image || image->rgba.size()<4 || image->rgba[3]!=0)failure="Embedded canvas did not retain transparent background";
                auto button=mounted->find("replace");
                auto state=button?button->snapshot():ii::core::Result<ii::gui::WidgetSnapshot>{std::unexpected(button.error())};
                if(!state)failure="Embedded button was not mounted";
                else {
                    const double scale=double(GetDpiForWindow(parent))/96.0;
                    const auto x=static_cast<short>((state->bounds.x+state->bounds.width/2)*scale);
                    const auto y=static_cast<short>((state->bounds.y+state->bounds.height/2)*scale);
                    SendMessageW(parent,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(x,y));
                    SendMessageW(parent,WM_LBUTTONUP,0,MAKELPARAM(x,y));
                    if(activations!=1)failure="Borrowed parent input did not reach the view";
                }
            }
            if(!SetWindowPos(parent,nullptr,0,0,280,160,SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE))failure="Cannot resize parent";
            auto resized=host->external_parent_resized();auto snapshot=host->snapshot();
            RECT client{};GetClientRect(parent,&client);const double scale=double(GetDpiForWindow(parent))/96.0;
            if(!resized||!snapshot||snapshot->size.width!=(client.right-client.left)/scale||snapshot->size.height!=(client.bottom-client.top)/scale||resizes!=1)
                failure="Parent geometry was not observed exactly once";
            const int delivered=resizes;
            if(!host->external_parent_resized()||resizes!=delivered)failure="Unchanged geometry caused a duplicate notification";
            if(host->set_title("Owned by external application"))failure="Embedded host changed the foreign window";
            if(close_parent)DestroyWindow(parent);
            else if(!host->request_close())failure="Embedded host could not close";
        },
        .resized=[&](ii::gui::Size){++resizes;},
        .closed=[&]{
            closed=true;
            if(!close_parent && (!IsWindow(parent)||GetWindowLongPtrW(parent,GWLP_WNDPROC)!=original_procedure))
                failure="The foreign window was destroyed or its procedure replaced";
            host.reset();PostQuitMessage(0);
        }
    });
    if(!attached)return 3;
    MSG message{};int status=0;
    while((status=GetMessageW(&message,nullptr,0,0))>0){TranslateMessage(&message);DispatchMessageW(&message);}
    if(IsWindow(parent))DestroyWindow(parent);
    UnregisterClassW(type.lpszClassName,instance);
    return status<0 || !failure.empty() || !opened || !closed || host?4:0;
}
