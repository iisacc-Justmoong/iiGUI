#include <iiGUI/iiGUI.h>
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace ii;
namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(core::Result<T> result){if(!result)throw std::runtime_error(result.error().message);return std::move(*result);}
void good(core::Result<void> result){if(!result)throw std::runtime_error(result.error().message);}
void run(const std::string& name){
    gui::Host host({.size={500,320}});gui::WidgetMount mount;gui::Widget viewport,body,root,child,inner;
    int delivered=0,pressed=0,moved=0,released=0,canceled=0,keys=0,clicked=0,left=0;double offset=-1;
    good(host.run({.opened=[&]{
        mount=take(host.mount_widgets([&](gui::WidgetBuilder& builder)->core::Result<void>{
            root=take(builder.absolute_stack({},"root",{460,280}));
            viewport=take(builder.scroll_view(root,"view",{.width=400,.height=180,.virtual_content_height=2400000,
                .virtual_content_width=name=="horizontal"?std::optional<double>{9000000000000.}:std::nullopt}));
            good(viewport.set_frame({20,20,400,180}));
            body=take(builder.absolute_stack(viewport,"body",{400,180}));
            child=take(builder.button(body,"child",{.text="Native child"},[&]{++clicked;}));
            good(child.set_frame({20,20,120,30}));
            good(viewport.set_scroll_handler([&](const gui::ScrollViewport& state){
                ++delivered;offset=state.offset;
                check(state.content_height==2400000||name=="geometry"||name=="horizontal"||name=="horizontal_natural","Logical content height lost");
                auto recursive=viewport.set_scroll_offset(state.offset+1);
                check(!recursive&&recursive.error().code==core::ErrorCode::busy,"Recursive scroll mutation accepted");
                if(name=="horizontal"){
                    auto recursive_x=viewport.set_horizontal_scroll_offset(state.horizontal_offset+1);
                    check(!recursive_x&&recursive_x.error().code==core::ErrorCode::busy,"Recursive horizontal mutation accepted");
                }
            }));
            if(name=="nested_axes"){
                inner=take(builder.scroll_view(body,"inner",{.width=200,.height=100,.virtual_content_height=100,.virtual_content_width=1000}));
                good(inner.set_frame({30,30,200,100}));take(builder.absolute_stack(inner,"inner-body",{200,100}));
            }
            if(name=="input"||name=="lifetime"||name=="hover")good(body.set_input_handler([&](const gui::WidgetInput& event)->core::Result<bool>{
                if(event.kind==gui::WidgetInputKind::pointer_down){++pressed;good(body.focus());return true;}
                if(event.kind==gui::WidgetInputKind::pointer_move){++moved;return name!="hover";}
                if(event.kind==gui::WidgetInputKind::pointer_leave){++left;return false;}
                if(event.kind==gui::WidgetInputKind::pointer_up){++released;return true;}
                if(event.kind==gui::WidgetInputKind::focus_lost){++canceled;return true;}
                if(event.kind==gui::WidgetInputKind::key_down){++keys;return true;}
                return false;
            }));
            return {};
        }));
        auto initial=take(viewport.snapshot());auto body_initial=take(body.snapshot()).bounds;
        check(delivered==1&&offset==0&&initial.content_height==2400000,"Initial virtual viewport not delivered once");
        if(name=="geometry"){
            good(viewport.set_scroll_offset(1200000));
            check(take(viewport.snapshot()).scroll_offset==1200000&&offset==1200000,"Large logical offset was clamped to widget coordinates");
            check(take(body.snapshot()).bounds==body_initial,"Virtual body translated to an enormous physical coordinate");
            const auto count=delivered;take(viewport.snapshot());check(delivered==count,"Unchanged layout repeated viewport callback");
            good(viewport.set_scroll_options({.width=400,.height=180,.virtual_content_height=600}));
            check(take(viewport.snapshot()).scroll_offset==420,"Content shrink did not clamp logical offset");
            check(!viewport.set_scroll_options({.virtual_content_height=std::nan("")}),"Invalid logical extent accepted");
        }else if(name=="horizontal"){
            const auto revision=delivered;
            good(viewport.set_scroll_position(900000000000.,1200000));
            auto state=take(viewport.snapshot());
            check(state.horizontal_scroll_offset==900000000000.&&state.scroll_offset==1200000&&state.content_width==9000000000000.,"Logical horizontal extent or position lost");
            check(take(body.snapshot()).bounds==body_initial&&delivered==revision+1,"Atomic two-axis scroll translated content or delivered twice");
            check(!viewport.set_scroll_position(std::nan(""),0)&&take(viewport.snapshot()).scroll_offset==1200000,"Invalid X partially changed Y");
            good(viewport.set_scroll_offset(1200020));good(viewport.set_horizontal_scroll_offset(900000000030.));
            state=take(viewport.snapshot());check(state.horizontal_scroll_offset==900000000030.&&state.scroll_offset==1200020,"Single-axis setter reset the other axis");
            good(mount.dispatch_input({.kind=gui::WidgetInputKind::scroll,.x=40,.y=40,.scroll_y=17,.scroll_x=23}));
            state=take(viewport.snapshot());check(state.horizontal_scroll_offset==900000000053.&&state.scroll_offset==1200037,"Diagonal wheel dropped an axis");
            good(mount.dispatch_input({.kind=gui::WidgetInputKind::scroll,.x=40,.y=40,.shift=true,.scroll_y=11}));
            state=take(viewport.snapshot());check(state.horizontal_scroll_offset==900000000064.&&state.scroll_offset==1200037,"Shift-wheel did not move horizontally");
            check(!viewport.set_scroll_options({.virtual_content_width=std::nan("")}),"Invalid horizontal extent accepted");
            good(viewport.set_scroll_options({.width=400,.height=180,.virtual_content_height=600,.virtual_content_width=1000}));
            state=take(viewport.snapshot());check(state.horizontal_scroll_offset==600&&state.scroll_offset==420,"Content shrink did not clamp both axes");
            good(viewport.set_scroll_options({.width=400,.height=180,.enabled=false,.virtual_content_height=600,.virtual_content_width=1000}));
            state=take(viewport.snapshot());check(state.horizontal_scroll_offset==0&&state.scroll_offset==0,"Disabled viewport retained a scroll offset");
        }else if(name=="horizontal_natural"){
            good(body.set_extent({1200,900}));good(child.set_frame({1050,750,120,30}));
            good(viewport.set_scroll_options({.width=400,.height=180,.horizontal_enabled=true}));
            auto state=take(viewport.snapshot());check(state.content_width==1200&&state.content_height==900,"Natural horizontal content was squeezed into viewport");
            good(child.focus());state=take(viewport.snapshot());
            check(state.horizontal_scroll_offset==770&&state.scroll_offset==600,"Focus reveal did not reach both natural axes");
            check(take(child.snapshot()).bounds.x>=state.bounds.x&&take(child.snapshot()).bounds.y>=state.bounds.y,"Revealed child remains outside viewport");
        }else if(name=="nested_axes"){
            good(mount.dispatch_input({.kind=gui::WidgetInputKind::scroll,.x=70,.y=70,.scroll_y=40,.scroll_x=30}));
            check(take(inner.snapshot()).horizontal_scroll_offset==30&&take(viewport.snapshot()).scroll_offset==40,"Nested diagonal wheel did not bubble only the blocked axis");
            check(take(inner.snapshot()).scroll_offset==0,"Blocked inner Y axis moved");
        }else if(name=="drawing"){
            gui::WidgetDrawing drawing;
            drawing.panels.push_back({{0,0,400,180},{1,0,0,1},{1,0,0,1}});
            drawing.texts.push_back({.bounds={5,5,150,20},.text="Native drawing",.color={1,1,1,1}});
            good(body.set_drawing(drawing));
            auto before=take(host.capture_canvas());drawing.panels[0].top=drawing.panels[0].bottom={0,0,1,1};good(body.set_drawing(drawing));auto after=take(host.capture_canvas());
            check(before.rgba!=after.rgba&&take(body.snapshot()).drawing_texts==1,"Retained drawing failed to reach GPU");
            drawing.panels[0].bounds.width=0;check(!body.set_drawing(drawing)&&take(body.snapshot()).drawing_panels==1,"Invalid drawing replaced retained content");
            check(take(mount.snapshot()).size()==4,"Drawing created a node per primitive");
        }else if(name=="hover"){
            good(mount.dispatch_input({.kind=gui::WidgetInputKind::pointer_move,.x=50,.y=50}));
            good(mount.dispatch_input({.kind=gui::WidgetInputKind::pointer_move,.x=470,.y=300}));
            good(mount.dispatch_input({.kind=gui::WidgetInputKind::pointer_move,.x=480,.y=310}));
            check(moved==1&&left==1,"Ancestor hover exit was not delivered once");
        }else if(name=="input"||name=="lifetime"){
            good(mount.dispatch_input({.kind=gui::WidgetInputKind::pointer_down,.x=50,.y=50}));
            if(name=="lifetime"){
                good(body.set_visible(false));check(canceled==1,"Hidden input region did not cancel capture");
                good(body.set_visible(true));good(mount.dispatch_input({.kind=gui::WidgetInputKind::pointer_down,.x=50,.y=50}));
                good(host.unmount_widgets());check(canceled==2,"Unmount did not cancel input capture");
            }else{
                good(mount.dispatch_input({.kind=gui::WidgetInputKind::pointer_move,.x=480,.y=300}));
                good(mount.dispatch_input({.kind=gui::WidgetInputKind::pointer_up,.x=480,.y=300}));
                good(mount.dispatch_input({.kind=gui::WidgetInputKind::key_down,.key=gui::WidgetKey::right}));
                check(pressed==1&&moved==1&&released==1&&keys==1&&clicked==0,"Captured input or ancestor interception was lost");
                good(body.set_input_handler({}));good(mount.dispatch_input({.kind=gui::WidgetInputKind::pointer_down,.x=50,.y=50}));good(mount.dispatch_input({.kind=gui::WidgetInputKind::pointer_up,.x=50,.y=50}));
                check(clicked==1,"Handler removal suppressed ordinary native child input");
            }
        }else throw std::runtime_error("Unknown virtual viewport case");
        good(host.request_close());
    }}));
}
}
int main(int argc,char** argv){try{run(argc>1?argv[1]:"geometry");std::cout<<"VIRTUAL_VIEWPORT_OK\n";}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
