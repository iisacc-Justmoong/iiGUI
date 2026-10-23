#include <iiGUI/Bindings.h>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace ii;
namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(core::Result<T> value){if(!value){if(value.error().cause)std::rethrow_exception(value.error().cause);throw std::runtime_error(value.error().message);}return std::move(*value);}
void good(core::Result<void> value){if(!value){if(value.error().cause)std::rethrow_exception(value.error().cause);throw std::runtime_error(value.error().message);}}
}
int main(int argc,char** argv){try{
    const bool vulkan=argc==2&&std::string_view(argv[1])=="vulkan";
    gui::Host host({.renderer=vulkan?gui::Renderer::vulkan:gui::Renderer::automatic});core::Observable<std::string> status{"Ready"};core::Observable<bool> enabled{true};
    gui::Widget retired;int ink=0,canceled=0;bool text_active=false;std::string edited;
    good(host.run({.opened=[&]{
        auto mount=take(host.mount_widgets([&](gui::WidgetBuilder& b)->core::Result<void>{
            auto root=take(b.vstack({},"root"));
            auto label=take(b.button(root,"status",{.text=""}));retired=label;
            auto bindings=std::make_shared<gui::BindingScope>();good(gui::bind_text(*bindings,label,status));good(gui::bind_enabled(*bindings,label,enabled));good(b.retain(bindings));
            auto field=take(b.input_field(root,"editor",{.text="A🙂한B",.multiline=true},[&](std::string value){edited=std::move(value);}));
            take(b.input_field(root,"secret",{.text="private-key",.password=true}));
            auto canvas=take(b.absolute_stack(root,"canvas",{500,200}));
            good(root.set_input_handler([&](const gui::WidgetInput& event)->core::Result<bool>{if(event.kind==gui::WidgetInputKind::key_down)text_active=event.text_input_active;return false;}));
            good(canvas.set_input_handler([&](const gui::WidgetInput& event)->core::Result<bool>{
                if(event.kind==gui::WidgetInputKind::focus_lost){++canceled;return false;}
                if(event.kind==gui::WidgetInputKind::pointer_down||event.kind==gui::WidgetInputKind::pointer_move||event.kind==gui::WidgetInputKind::pointer_up){
                    check(event.device==gui::PointerDevice::pen&&event.device_id==42,"Pen identity was lost");
                    check(event.pressure==.37&&event.tilt_x==-20&&event.tilt_y==31&&event.rotation==12,"Pen geometry was lost");
                    check(event.control&&event.alt&&event.meta&&event.shift&&event.buttons==1,"Pointer modifiers were lost");++ink;return true;
                }
                return false;
            }));return {};
        }));
        check(take(retired.snapshot()).text=="Ready","Binding did not set its initial value");status.set("Edited");enabled.set(false);
        check(take(retired.snapshot()).text=="Edited"&&!take(retired.snapshot()).enabled,"Property changes did not reach widgets");
        auto field=take(mount.find("editor"));good(field.focus());
        good(mount.dispatch_input({.kind=gui::WidgetInputKind::text,.text="국",.replacement_range=gui::TextRange{3,1}}));
        check(edited=="A🙂국B","UTF-16 replacement selected the wrong scalar");
        const auto before=take(field.snapshot());
        check(!mount.dispatch_input({.kind=gui::WidgetInputKind::text,.text="bad",.replacement_range=gui::TextRange{2,1}}),"A surrogate was split");
        check(!mount.dispatch_input({.kind=gui::WidgetInputKind::text,.text="bad",.replacement_range=gui::TextRange{std::numeric_limits<std::size_t>::max(),1}}),"An overflowing native range was accepted");
        check(take(field.snapshot()).text==before.text,"Rejected replacement mutated text");
        good(mount.dispatch_input({.kind=gui::WidgetInputKind::composition,.text="ㅎ",.replacement_range=gui::TextRange{3,1},.composition_selection=gui::TextRange{1,0}}));
        check(take(field.snapshot()).text==edited&&take(field.snapshot()).marked_range==gui::TextRange{3,1},"Preedit changed committed data or lost its range");
        good(mount.dispatch_input({.kind=gui::WidgetInputKind::text,.text="한"}));check(edited=="A🙂한B","IME commit did not replace the preedit selection");
        good(mount.dispatch_input({.kind=gui::WidgetInputKind::key_down,.key=gui::WidgetKey::enter}));check(edited=="A🙂한\nB"&&text_active,"Multiline entry or shortcut protection failed");
        good(mount.dispatch_input({.kind=gui::WidgetInputKind::key_down,.key=gui::WidgetKey::home}));
        good(mount.dispatch_input({.kind=gui::WidgetInputKind::key_down,.key=gui::WidgetKey::right,.shift=true}));
        good(mount.dispatch_input({.kind=gui::WidgetInputKind::key_down,.key=gui::WidgetKey::cut}));check(edited=="🙂한\nB","Cut did not honor scalar selection");
        auto secret=take(mount.find("secret"));good(secret.focus());check(take(secret.snapshot()).password,"Secure field lost its semantic flag");
        good(mount.dispatch_input({.kind=gui::WidgetInputKind::key_down,.key=gui::WidgetKey::select_all}));
        good(mount.dispatch_input({.kind=gui::WidgetInputKind::key_down,.key=gui::WidgetKey::cut}));check(take(secret.snapshot()).text=="private-key","Secure field exported a cut operation");
        auto canvas=take(mount.find("canvas"));auto bounds=take(canvas.snapshot()).bounds;
        gui::WidgetInput point{.kind=gui::WidgetInputKind::pointer_down,.x=bounds.x+10,.y=bounds.y+10,.shift=true,.control=true,.alt=true,.meta=true,.device=gui::PointerDevice::pen,.pressure=.37,.tilt_x=-20,.tilt_y=31,.rotation=12,.timestamp=1,.device_id=42,.buttons=1};
        good(mount.dispatch_input(point));point.kind=gui::WidgetInputKind::pointer_move;point.x=-200;good(mount.dispatch_input(point));
        good(mount.dispatch_input({.kind=gui::WidgetInputKind::focus_lost}));check(ink==2&&canceled==1,"Captured stroke did not cancel on focus loss");
        point.pressure=2;check(!mount.dispatch_input(point),"Invalid pressure was accepted");
        check(take(host.capture_canvas()).width>0,"Native editor surface did not render");if(vulkan)check(take(host.graphics_info()).api==gui::GraphicsApi::vulkan,"Forced Vulkan was not selected");
        good(host.unmount_widgets());status.set("After detach");check(!retired.snapshot(),"Detached binding retained a widget generation");
        good(host.request_close());
    }}));status.set("After close");
    std::cout<<"EDITOR_INPUT_OK pen=true capture=true ime=true multiline=true bindings=true\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
