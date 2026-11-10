#include "../src/Widgets/AtSpiApplication.h"
#include <memory>
#include <stdexcept>
#include <thread>

using namespace ii::gui;
using namespace ii::gui::detail;
namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
WidgetSnapshot item(std::string id,std::string parent,WidgetKind kind){
    WidgetSnapshot value{};value.id=std::move(id);value.parent_id=std::move(parent);value.kind=kind;
    value.attached=true;value.bounds=value.visual_bounds={5,8,60,20};return value;
}
void windows_and_generations(){
    AtSpiApplication app("Creative App");auto first=app.open_window("Main",{640,480});auto second=app.open_window("Palette",{320,240});
    check(first&&second&&*first!=*second,"Window identities were reused");
    const auto root=app.object(AtSpiApplication::root_path());check(root&&root->role==AtSpiRole::application&&root->children.size()==2,"The application root did not own both windows");
    const auto a=app.window_path(*first),b=app.window_path(*second);check(root->children[0]==a&&root->children[1]==b,"Window order was lost");
    auto window=app.object(a);check(window&&window->role==AtSpiRole::frame&&window->name=="Main"&&window->children.empty()&&!window->screen_origin,"Window state was not exposed accurately");
    auto layout=item("layout","",WidgetKind::vertical_stack);auto button=item("save","layout",WidgetKind::button);button.text="Save";button.focused=true;
    auto old=app.stage(*first,{layout,button});check(old&&app.object(a)->children.empty(),"An uncommitted surface became visible");
    check(app.commit(*old),"The initial surface could not commit");window=app.object(a);check(window&&window->children.size()==1,"Committed content was not published");
    const auto old_path=window->children[0];auto save=app.object(old_path);
    check(save&&save->name=="Save"&&save->role==AtSpiRole::button&&save->parent==a&&save->focused,"A widget lost its identity, parent, role or focus");
    auto action=app.action(old_path,WidgetInputKind::activate);check(action&&action->target=="save","A live widget action was not mapped");
    button.text="Save As";check(app.update(*old,{layout,button})&&app.object(old_path)->name=="Save As","A retained widget changed path on update");
    auto next=app.stage(*first,{layout,button});check(next&&app.object(old_path).has_value(),"Staging replaced the visible generation prematurely");
    check(app.commit(*next)&&!app.object(old_path)&&!app.action(old_path,WidgetInputKind::activate),"A retired surface remained addressable");
    const auto new_path=app.object(a)->children.at(0);check(new_path!=old_path,"A replacement reused an accessibility object path");
    check(!app.update(*old,{layout,button})&&!app.retire(*old)&&app.object(new_path).has_value(),"A stale surface changed its successor");
    check(app.close_window(*first)&&!app.object(new_path)&&app.object(b),"Closing one window retired another");
    check(!app.close_window(*first),"A closed window was closed twice");
}
void dynamic_nodes_and_privacy(){
    AtSpiApplication app("Editor");auto window=app.open_window("Document",{200,100},{WidgetPoint{100,200}});check(window.has_value(),"Cannot open a window");
    auto root=item("root","",WidgetKind::absolute_stack);auto secret=item("password","root",WidgetKind::input_field);
    secret.text_input=true;secret.password=true;secret.text="secret";secret.composition_text="private";secret.placeholder="Password";
    auto cell=item("@iiscope:ax:0:1","root",WidgetKind::action_surface);cell.accessibility_semantics=AccessibilitySemantics{.role=AccessibilityRole::cell};cell.accessibility_name="Cell";
    auto surface=app.stage(*window,{root,secret,cell});check(surface&&app.commit(*surface),"Cannot publish accessibility surface");
    auto children=app.object(app.window_path(*window))->children;check(children.size()==2,"Wrong accessible children");
    auto password=app.object(children[0]);check(password&&password->role==AtSpiRole::password&&password->name=="Password"&&!password->value,"Password content escaped");
    check(password->screen_origin&&password->screen_origin->x==100&&password->bounds.x==5,"Known screen and local geometry were conflated");
    const auto old_cell=children[1];check(app.update(*surface,{root,secret})&&!app.object(old_cell),"Removed virtual node was retained");
    check(app.update(*surface,{root,secret,cell}),"Cannot restore virtual node");children=app.object(app.window_path(*window))->children;
    check(children[1]!=old_cell&&!app.action(old_cell,WidgetInputKind::activate),"A virtual node reused a retired path");
    check(app.retire(*surface)&&app.object(app.window_path(*window))->children.empty(),"Retired surface remained published");
}
void rejection_and_thread_ownership(){
    AtSpiApplication app("Editor");
    check(!app.open_window("Bad",{0,10})&&!app.open_window(std::string("Bad\0title",9),{10,10}),"An invalid window was accepted");
    auto window=app.open_window("Main",{200,100});check(window.has_value(),"Cannot create a valid window");
    auto root=item("root","",WidgetKind::absolute_stack);
    auto button=item("button","root",WidgetKind::button);button.text="Apply";
    auto disabled=item("disabled","root",WidgetKind::button);disabled.enabled=false;
    auto hidden=item("hidden","root",WidgetKind::button);hidden.visible=false;
    auto passive=item("passive","root",WidgetKind::button);passive.accessibility_actionable=false;
    auto surface=app.stage(*window,{root,button,disabled,hidden,passive});
    check(surface&&app.commit(*surface),"Cannot publish the valid surface");
    const auto path=app.window_path(*window);const auto children=app.object(path)->children;
    check(children.size()==3,"The hidden control was exposed");
    check(!app.action(children[1],WidgetInputKind::activate)&&!app.action(children[2],WidgetInputKind::activate),"A disabled or passive control accepted activation");
    check(app.action(children[0],WidgetInputKind::focus).has_value()&&!app.action(children[0],WidgetInputKind::text),"An unsupported action was accepted");
    auto candidate=app.stage(*window,{root,button});check(candidate.has_value(),"Cannot stage replacement");
    check(!app.stage(*window,{root,button})&&app.retire(*candidate)&&app.object(children[0]).has_value(),"A cancelled replacement affected the published surface");
    check(!app.commit(*candidate)&&!app.object(path+"0")&&!app.object(children[0]+"_1")&&!app.object("/org/a11y/atspi/accessible/w01"),"An invalid object path or generation was accepted");
    bool read=false,write=false;
    std::thread other([&]{read=app.object(path).has_value();write=app.close_window(*window);});other.join();
    check(!read&&!write&&app.object(path).has_value(),"A foreign thread accessed owner-thread state");
}
}
int main(){windows_and_generations();dynamic_nodes_and_privacy();rejection_and_thread_ownership();}
