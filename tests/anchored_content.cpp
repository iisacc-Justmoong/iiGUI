#include <iiGUI/iiGUI.h>
#include <chrono>
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace ii;
using namespace std::chrono_literals;
namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>void good(const T& r){if(!r){if constexpr(requires{r.error().cause;}){if(r.error().cause)std::rethrow_exception(r.error().cause);throw std::runtime_error(r.error().message);}else throw std::runtime_error("Task admission failed");}}
template<class T>T take(core::Result<T> r){good(r);return std::move(*r);}
struct Lifetime final {int& retired;explicit Lifetime(int& n):retired(n){}~Lifetime(){++retired;}};
struct Tree final {
    gui::Widget root,popup,anchor,title,apply,outside;gui::WidgetMount mount;
    std::shared_ptr<gui::MotionPreferences> preferences=std::make_shared<gui::MotionPreferences>();
    int made=0,retired=0,clicked=0,edited=0,closed=0;bool add_menu=false,retire_on_blur=false,label_only=false,blur_armed=false,preserve_focus=false;
    gui::PopoverOptions options(bool motion) {
        gui::PopoverOptions o{.width=400,.padding=20,.margins=24,.corner_radius=12,.surface_opacity=1,.surface_color=gui::WidgetColor{1,1,1,1},.modal=true,.dim=true,.placement=gui::PopoverPlacement::centered};
        o.focus_on_open=!preserve_focus;
        if(motion)o.transition=gui::PopoverTransition{.enter_duration=420ms,.exit_duration=150ms,.enter_easing=animation::Easing::out_back,.exit_easing=animation::Easing::in_cubic,.overshoot=1.45,.collapsed_scale=.92,.preferences=preferences};
        return o;
    }
    void attach(gui::Host& host,bool motion=false){
        mount=take(host.mount_widgets([&](gui::WidgetBuilder& b)->core::Result<void>{
            root=take(b.vstack({},"root"));outside=take(b.button(root,"outside",{.text="Outside"}));
            popup=take(b.popover(root,"popup",options(motion),{.closed=[&]{check(retired==made,"Close notified before body retirement");++closed;}}));
            auto chrome=take(b.vstack(popup,"chrome",{.spacing=20,.alignment=gui::StackAlignment::stretch}));
            title=take(b.label(chrome,"title",{.text="Static chrome",.color=gui::WidgetColor{1,0,0,1}}));
            if(retire_on_blur)take(b.input_field(chrome,"chromeField",{.text="Chrome",.commit_on_focus_loss=true},{},[&]{if(blur_armed)good(host.unmount_widgets());}));
            anchor=take(b.vstack(chrome,"body",{.spacing=0,.alignment=gui::StackAlignment::stretch}));
            apply=take(b.button(chrome,"apply",{.text="Apply"},[&]{++clicked;}));
            return popup.set_popup_content_at(anchor,[&](gui::WidgetBuilder& child)->core::Result<void>{
                ++made;good(child.retain(std::make_shared<Lifetime>(retired)));auto content=take(child.vstack({},"bodyRoot",{.spacing=8,.alignment=gui::StackAlignment::stretch}));
                if(label_only){take(child.label(content,"description",{.text="No input here"}));return {};}
                take(child.input_field(content,"editor",{.text="Fresh",.width=280},[&](std::string){++edited;}));
                take(child.slider(content,"slider",{.width=280}));
                take(child.label(content,"description",{.text="Short body text"}));
                if(add_menu){auto menu=take(child.menu(content,"menu"));take(child.menu_item(menu,"item",{.label="Choose",.has_child_items=false},[&]{++clicked;}));}
                return {};
            });
        }));
    }
};
void deterministic(const std::string& name){
    runtime::EventLoop events;runtime::FrameLoop frames(events);gui::Host host({.size={800,600},.event_loop=&events,.frame_loop=&frames});Tree tree;
    good(host.run({.opened=[&]{
        tree.add_menu=name=="nesting";tree.retire_on_blur=name=="reentrant_focus";tree.label_only=name=="label_focus";tree.preserve_focus=name=="no_focus";
        tree.attach(host,name=="motion"||name=="input"||name=="exit_label");auto static_title=tree.title;auto static_action=tree.apply;good(tree.outside.focus());good(tree.popup.open_at(0,0));
        if(name=="label_focus"){check(take(tree.apply.snapshot()).focused,"Noninteractive body added a spurious Tab stop");good(host.request_close());return;}
        if(name=="no_focus"){check(take(tree.outside.snapshot()).focused&&!take(take(tree.popup.popup_content("editor")).snapshot()).focused,"Explicit focus_on_open false was ignored by body autofocus");good(host.request_close());return;}
        auto editor=take(tree.popup.popup_content("editor"));auto first=take(editor.snapshot()).id;
        const auto now=runtime::Clock::now();
        if(name=="lifetime"){
            for(int i=0;i<20;++i){check(tree.made==i+1&&tree.retired==i,"Open did not create one body");good(tree.popup.close_popup());check(!editor.snapshot()&&bool(static_title.snapshot())&&bool(static_action.snapshot()),"Closing retired static chrome or kept the old body");good(tree.popup.open_at(0,0));editor=take(tree.popup.popup_content("editor"));check(take(editor.snapshot()).id!=first,"Body generation was reused");}
            good(tree.popup.close_popup());check(tree.made==21&&tree.retired==21&&tree.closed==21,"Repeated lifecycle did not balance");
        }else if(name=="layout"){
            auto body=take(tree.anchor.snapshot()).bounds,field=take(editor.snapshot()).bounds,action=take(tree.apply.snapshot()).bounds,title=take(tree.title.snapshot()).bounds;
            check(field.x>=body.x&&field.y>=body.y&&field.y>title.y+title.height&&action.y>=body.y+body.height+19,"Body did not contribute height between static chrome");
            auto count=tree.made;good(editor.set_text("Changed"));good(host.set_content_size({640,550}));check(tree.made==count&&take(editor.snapshot()).text=="Changed","Resize rebuilt active body");
            auto description=take(tree.popup.popup_content("description"));const auto previous_height=take(tree.popup.snapshot()).bounds.height;good(description.set_text(std::string(500,'W')));check(take(tree.popup.snapshot()).bounds.height>previous_height,"Body intrinsic size mutation did not invalidate the retained chrome layout");
            good(tree.popup.close_popup());check(!take(tree.anchor.snapshot()).visible&&take(tree.popup.snapshot()).implicit_size.height<200,"Inactive body kept a layout gap");
        }else if(name=="motion"){
            (void)frames.step(now+210ms);auto entered=take(tree.popup.snapshot());check(entered.reveal_progress>1,"Anchored body did not join frame motion");
            good(tree.popup.close_popup());auto exit=take(tree.popup.snapshot());check(exit.exiting&&exit.visible&&!editor.snapshot()&&tree.retired==1&&take(tree.title.snapshot()).visible,"Exit did not keep chrome while immediately retiring body");
            check(!tree.apply.activate(),"Exiting chrome accepted input");good(tree.popup.open_at(0,0));check(tree.made==2&&tree.retired==1&&take(tree.popup.popup_content("editor")).snapshot()->text=="Fresh"&&bool(static_action.snapshot()),"Reversal failed to replace only the body");
            good(tree.preferences->set_reduced_motion(true));check(!take(tree.popup.snapshot()).transitioning,"Reduced motion did not settle anchored content");
        }else if(name=="input"){
            (void)frames.step(now+70ms);auto field=take(editor.snapshot());check(field.visual_bounds.width<field.bounds.width,"Embedded accessibility frame did not inherit card scale");
            auto p=field.visual_bounds;good(tree.mount.dispatch_input({.kind=gui::WidgetInputKind::pointer_down,.x=p.x+p.width/2,.y=p.y+p.height/2}));good(tree.mount.dispatch_input({.kind=gui::WidgetInputKind::pointer_up,.x=p.x+p.width/2,.y=p.y+p.height/2}));
            good(tree.mount.dispatch_input({.kind=gui::WidgetInputKind::key_down,.key=gui::WidgetKey::select_all}));good(tree.mount.dispatch_input({.kind=gui::WidgetInputKind::text,.text="Edited"}));check(take(editor.snapshot()).text=="Edited"&&tree.edited==1,"Transformed embedded field input missed its body");
        }else if(name=="focus"){
            good(editor.focus());good(tree.mount.dispatch_input({.kind=gui::WidgetInputKind::key_down,.key=gui::WidgetKey::tab}));check(take(take(tree.popup.popup_content("slider")).snapshot()).focused,"Tab did not visit the next body field");
            good(tree.mount.dispatch_input({.kind=gui::WidgetInputKind::key_down,.key=gui::WidgetKey::tab}));check(take(tree.apply.snapshot()).focused,"Tab did not leave body for static action");
            good(tree.mount.dispatch_input({.kind=gui::WidgetInputKind::key_down,.key=gui::WidgetKey::tab,.shift=true}));check(take(take(tree.popup.popup_content("slider")).snapshot()).focused,"Reverse Tab did not enter the last body field");
            good(tree.popup.close_popup());check(take(tree.outside.snapshot()).focused,"Close lost the original outer focus");
        }else if(name=="rendering"){
            auto image=take(host.capture_canvas());auto before=take(host.graphics_info());good(tree.apply.set_text("New chrome"));auto after=take(host.capture_canvas());check(image.rgba!=after.rgba&&before.api==gui::GraphicsApi::metal,"Static chrome and body were not jointly rendered");
            good(tree.popup.close_popup());check(take(host.capture_canvas()).rgba!=after.rgba,"Closed anchored content remained on GPU");
        }else if(name=="teardown"){
            good(host.unmount_widgets());check(tree.retired==1&&!editor.snapshot()&&!static_action.snapshot(),"Mount teardown kept scoped body or chrome");
        }else if(name=="routing"){
            const auto action=take(tree.apply.snapshot()).bounds,field=take(editor.snapshot()).bounds;
            auto move=[&](gui::Rect r){good(tree.mount.dispatch_input({.kind=gui::WidgetInputKind::pointer_move,.x=r.x+r.width/2,.y=r.y+r.height/2}));};
            move(action);check(take(tree.apply.snapshot()).hovered,"Static action did not hover");move(field);check(!take(tree.apply.snapshot()).hovered,"Body routing left stale chrome hover");
            good(tree.mount.dispatch_input({.kind=gui::WidgetInputKind::pointer_down,.x=action.x+2,.y=action.y+2}));move(field);good(tree.mount.dispatch_input({.kind=gui::WidgetInputKind::pointer_up,.x=field.x+2,.y=field.y+2}));check(!take(tree.apply.snapshot()).pressed&&tree.clicked==0,"Body stole chrome capture/release");
            auto slider=take(tree.popup.popup_content("slider"));auto r=take(slider.snapshot()).bounds;
            good(tree.mount.dispatch_input({.kind=gui::WidgetInputKind::pointer_down,.x=r.x+r.width/2,.y=r.y+r.height/2}));good(tree.mount.dispatch_input({.kind=gui::WidgetInputKind::pointer_move,.x=1000,.y=1000}));good(tree.mount.dispatch_input({.kind=gui::WidgetInputKind::pointer_up,.x=1000,.y=1000}));check(take(slider.snapshot()).value==1,"Body slider lost capture outside the frame");
        }else if(name=="nesting"){
            auto menu=take(tree.popup.popup_content("menu"));good(menu.open_for(editor,40,30));check(take(tree.apply.snapshot()).input_blocked,"Nested modal menu left static chrome active");
            auto r=take(tree.apply.snapshot()).bounds;auto click=[&]{good(tree.mount.dispatch_input({.kind=gui::WidgetInputKind::pointer_down,.x=r.x+2,.y=r.y+2}));good(tree.mount.dispatch_input({.kind=gui::WidgetInputKind::pointer_up,.x=r.x+2,.y=r.y+2}));};
            click();check(!take(menu.snapshot()).opened&&tree.clicked==0,"Nested outside dismissal leaked into chrome");click();check(tree.clicked==1,"Chrome did not resume after nested dismissal");
        }else if(name=="reentrant_focus"){
            auto chrome=take(tree.mount.find("chromeField"));check(take(chrome.snapshot()).focused,"Body autofocus skipped the first chrome field");tree.blur_armed=true;good(tree.mount.dispatch_input({.kind=gui::WidgetInputKind::key_down,.key=gui::WidgetKey::tab}));check(!tree.popup.snapshot()&&tree.retired==1,"Blur-time unmount admitted pending body focus");
        }else if(name=="exit_label"){
            (void)frames.step(now+500ms);good(tree.popup.close_popup());auto r=take(tree.title.snapshot()).visual_bounds;auto pixels=take(host.capture_canvas());auto scale=take(host.snapshot()).backing_scale;bool red=false;
            for(unsigned y=unsigned(r.y*scale);y<unsigned((r.y+r.height)*scale)&&y<pixels.height;++y)for(unsigned x=unsigned(r.x*scale);x<unsigned((r.x+r.width)*scale)&&x<pixels.width;++x){const auto* p=&pixels.rgba[(std::size_t(y)*pixels.width+x)*4];red|=p[0]>220&&p[1]<20&&p[2]<20;}
            check(red,"Exit changed explicit label color when only input enablement changed");
        }else throw std::runtime_error("Unknown anchored content test");
        good(host.request_close());
    }}));
}
void contracts(){
    gui::Host host;Tree tree;
    good(host.run({.opened=[&]{tree.attach(host);check(!tree.popup.set_popup_content_at(tree.root,{}),"Anchor outside popup accepted");check(!tree.popup.set_popup_content_at(tree.apply,{}),"Interactive anchor accepted");good(tree.popup.open_at(0,0));check(!tree.popup.set_popup_content_at(tree.anchor,{}),"Factory changed during open");good(tree.popup.close_popup());good(host.request_close());}}));
}
}
int main(int argc,char**argv){try{std::string name=argc>1?argv[1]:"lifetime";if(name=="contracts")contracts();else deterministic(name);std::cout<<"ANCHORED_CONTENT_OK "<<name<<'\n';}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
