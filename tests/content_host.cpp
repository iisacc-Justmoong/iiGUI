#include <iiGUI/iiGUI.h>
#include <iostream>
#include <stdexcept>
using namespace ii;
namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(core::Result<T> value){if(!value){if(value.error().cause)std::rethrow_exception(value.error().cause);throw std::runtime_error(value.error().message);}return std::move(*value);}
void good(core::Result<void> value){if(!value){if(value.error().cause)std::rethrow_exception(value.error().cause);throw std::runtime_error(value.error().message);}}
void run(std::string name){
    gui::Host host({.size={540,400}});gui::WidgetMount mount;gui::Widget slot,old,button,after;
    int created=0,destroyed=0,clicked=0,committed=0;
    struct Owner final{int& count;explicit Owner(int& value):count(value){}~Owner(){++count;}};
    auto factory=[&](gui::WidgetBuilder& b)->core::Result<void>{++created;good(b.retain(std::make_shared<Owner>(destroyed)));auto root=take(b.vstack({},"page",{.spacing=4,.alignment=gui::StackAlignment::stretch}));button=take(b.button(root,"action",{.text="Native page"},[&]{++clicked;}));return {};};
    good(host.run({.opened=[&]{
        mount=take(host.mount_widgets([&](gui::WidgetBuilder& b)->core::Result<void>{auto root=take(b.vstack({},"root",{.spacing=8,.alignment=gui::StackAlignment::leading}));slot=take(b.content_host(root,"slot",{360,220}));after=take(b.button(root,"after",{.text="After"}));return {};}));
        good(slot.set_content(factory));old=button;check(take(slot.snapshot()).has_content,"Host did not publish a page");const auto generation=take(slot.snapshot()).content_generation;
        if(name=="lifetime"){
            good(slot.set_content(factory));check(!old.snapshot()&&take(slot.snapshot()).content_generation>generation,"Replacement reused old generation handles");
            good(slot.clear_content());check(!button.snapshot()&&!take(slot.snapshot()).has_content,"Clear retained a page");
            good(host.unmount_widgets());check(created==2&&destroyed==2,"Page owners were not retired once");
        }else if(name=="atomic"){
            auto failed=slot.set_content([](gui::WidgetBuilder&)->core::Result<void>{return core::fail(core::ErrorCode::failed,"factory failure");});
            check(!failed&&old.snapshot()&&take(slot.snapshot()).content_generation==generation,"Factory failure replaced the current page");
            failed=slot.set_content(factory,[&]()->core::Result<void>{++committed;return core::fail(core::ErrorCode::busy,"commit failure");});
            check(!failed&&committed==1&&old.snapshot()&&take(slot.snapshot()).content_generation==generation,"Commit rejection replaced the current page");
            failed=slot.set_content([](gui::WidgetBuilder&)->core::Result<void>{throw std::runtime_error("factory exception");});check(!failed&&failed.error().cause&&old.snapshot(),"Factory exception escaped or retired the old page");
        }else if(name=="input"){
            const auto r=take(button.snapshot()).bounds;good(mount.dispatch_input({.kind=gui::WidgetInputKind::pointer_down,.x=r.x+10,.y=r.y+10}));good(mount.dispatch_input({.kind=gui::WidgetInputKind::pointer_up,.x=r.x+10,.y=r.y+10}));check(clicked==1,"Host did not route native pointer input");
            good(button.focus());good(mount.dispatch_input({.kind=gui::WidgetInputKind::key_down,.key=gui::WidgetKey::enter}));check(clicked==1,"Keyboard press activated before release");good(mount.dispatch_input({.kind=gui::WidgetInputKind::key_up,.key=gui::WidgetKey::enter}));check(clicked==2,"Host did not route keyboard input");
            auto before=take(host.capture_canvas());good(slot.set_enabled(false));auto disabled=take(host.capture_canvas());check(before.rgba!=disabled.rgba,"Disabled host did not update inherited control visuals");check(!take(button.snapshot()).enabled,"Disabled host exposed enabled descendants");good(button.activate());check(clicked==2,"Disabled host admitted activation");
        }else if(name=="nested"){
            good(slot.set_content([&](gui::WidgetBuilder& b)->core::Result<void>{auto root=take(b.vstack({},"page"));auto inner=take(b.content_host(root,"inner",{280,160}));return inner.set_content([&](gui::WidgetBuilder& nested)->core::Result<void>{auto root=take(nested.vstack({},"page"));button=take(nested.button(root,"action",{.text="Nested"},[&]{++clicked;}));return {};});}));
            check(take(mount.snapshot()).size()==7&&take(button.snapshot()).popup_id.empty(),"Nested activation was lost or became a popup");
            good(button.focus());good(mount.dispatch_input({.kind=gui::WidgetInputKind::key_down,.key=gui::WidgetKey::enter}));check(clicked==0,"Nested keyboard press activated before release");good(mount.dispatch_input({.kind=gui::WidgetInputKind::key_up,.key=gui::WidgetKey::enter}));check(clicked==1,"Nested host lost keyboard focus routing");
            good(slot.clear_content());check(!button.snapshot()&&take(mount.snapshot()).size()==3,"Nested generation leaked after clear");
        }else if(name=="capture"){
            int moves=0,ups=0;
            good(slot.set_content([&](gui::WidgetBuilder& b)->core::Result<void>{auto root=take(b.vstack({},"page"));return root.set_input_handler([&](const auto& input)->core::Result<bool>{if(input.kind==gui::WidgetInputKind::pointer_down)return true;if(input.kind==gui::WidgetInputKind::pointer_move){++moves;return true;}if(input.kind==gui::WidgetInputKind::pointer_up){++ups;good(slot.set_content(factory));return true;}return false;});}));
            const auto r=take(slot.snapshot()).bounds;good(mount.dispatch_input({.kind=gui::WidgetInputKind::pointer_down,.x=r.x+10,.y=r.y+10}));
            good(mount.dispatch_input({.kind=gui::WidgetInputKind::pointer_move,.x=520,.y=380}));good(mount.dispatch_input({.kind=gui::WidgetInputKind::pointer_up,.x=520,.y=380}));
            good(mount.dispatch_input({.kind=gui::WidgetInputKind::pointer_up,.x=r.x+10,.y=r.y+10}));check(moves==1&&ups==1&&clicked==0,"Captured input escaped a host or activated its successor");
        }else if(name=="popup"){
            gui::Widget popup;
            good(slot.set_content([&](gui::WidgetBuilder& b)->core::Result<void>{auto root=take(b.vstack({},"page"));take(b.label(root,"title",{.text="Page"}));popup=take(b.popover(root,"dialog",{.width=120,.height=80,.modal=true,.dim=true}));take(b.label(popup,"message",{.text="Portal"}));return {};}));
            good(popup.open_at(380,250));check(take(popup.snapshot()).bounds.x>=take(slot.snapshot()).bounds.width,"Popup was clamped to the page viewport");
            check(take(after.snapshot()).input_blocked&&!after.activate(),"Page modal did not block the outer tree");take(host.capture_canvas());
            good(mount.dispatch_input({.kind=gui::WidgetInputKind::key_down,.key=gui::WidgetKey::escape}));check(!take(popup.snapshot()).opened&&!take(after.snapshot()).input_blocked,"Escape did not close the nested modal");
        }else if(name=="quota"){
            const auto dense=[](gui::WidgetBuilder& b)->core::Result<void>{auto root=b.vstack({},"page");if(!root)return std::unexpected(root.error());for(unsigned i=0;i<4092;++i){auto label=b.label(*root,"item"+std::to_string(i),{.text="Row"});if(!label)return std::unexpected(label.error());}return {};};
            good(slot.set_content(dense));const auto first=take(slot.content_item());good(slot.set_content(dense));check(!first.snapshot()&&take(mount.snapshot()).size()==4096,"Replacement did not release the old generation's domain budget");
            auto failed=slot.set_content([](gui::WidgetBuilder& b)->core::Result<void>{auto root=b.vstack({},"page");if(!root)return std::unexpected(root.error());for(unsigned i=0;i<4093;++i){auto label=b.label(*root,"item"+std::to_string(i),{.text="Row"});if(!label)return std::unexpected(label.error());}return {};});
            check(!failed&&take(mount.snapshot()).size()==4096,"Over-budget construction changed the active tree");good(slot.clear_content());check(take(mount.snapshot()).size()==3,"Node budget leaked after rejection and clear");
        }else if(name=="geometry"){
            const auto place=take(slot.placement()),next=take(after.placement());check(!place.scope.expired()&&!place.scope.owner_before(next.scope)&&!next.scope.owner_before(place.scope)&&place.parent==next.parent&&next.sibling==place.sibling+1,"Sibling placement lost scope or tree order");
            auto root=take(slot.content_item()).snapshot();check(root&&root->parent_id=="slot"&&root->popup_id.empty(),"Page acquired popup identity or wrong parent");
            const auto r=take(slot.snapshot()).bounds;check(root->bounds==r,"Page root did not fill its native viewport");
            auto snapshots=take(mount.snapshot());check(snapshots.size()==5&&take(take(mount.find(root->id)).snapshot()).id==root->id,"Embedded page was absent from inspection/lookup");
            take(host.capture_canvas());good(slot.set_extent({400,260}));root=take(slot.content_item()).snapshot();check(root->bounds.width==400&&root->bounds.height==260,"Page did not follow host resize");
        }else if(name=="retirement"){
            auto failed=slot.set_content([&](gui::WidgetBuilder& b)->core::Result<void>{take(b.label({},"new",{.text="new"}));good(host.unmount_widgets());return {};});
            check(!failed&&failed.error().code==core::ErrorCode::closed&&!old.snapshot(),"Owner retirement published an orphan page");
        }else throw std::runtime_error("Unknown content host case");
        good(host.request_close());
    }}));
}
}
int main(int argc,char** argv){try{run(argc>1?argv[1]:"lifetime");std::cout<<"CONTENT_HOST_OK\n";}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
