#include "../src/Widgets/WindowsAccessibilityTree.h"
#include "../src/Widgets/WindowsAccessibilityQueue.h"
#include "../src/Widgets/WindowsAccessibilityObjectIds.h"
#include <iiGUI/WidgetState.h>
#include <atomic>
#include <stdexcept>
#include <thread>
#include <vector>

using namespace ii::gui;
using namespace ii::gui::detail;

namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
WidgetSnapshot node(std::string id,std::string parent,WidgetKind kind,Rect bounds={0,0,80,30}){
    WidgetSnapshot n{};n.id=std::move(id);n.parent_id=std::move(parent);n.kind=kind;n.attached=true;
    n.bounds=n.visual_bounds=bounds;return n;
}
void hierarchy_and_privacy(){
    auto root=node("layout","",WidgetKind::vertical_stack);
    auto group=node("tabs","layout",WidgetKind::horizontal_stack);group.accessibility_semantics=AccessibilitySemantics{.role=AccessibilityRole::tab_group};
    auto wrapper=node("wrapper","tabs",WidgetKind::vertical_stack);
    auto tab=node("tab","wrapper",WidgetKind::action_surface);tab.action_role=ActionRole::tab;tab.text="Layers";tab.focused=true;tab.selected=true;
    auto secret=node("password","layout",WidgetKind::input_field);secret.text_input=true;secret.password=true;secret.text="secret";secret.composition_text="private";secret.placeholder="Password";
    auto hidden=node("hidden","layout",WidgetKind::button);hidden.visible=false;
    auto blocked=node("blocked","layout",WidgetKind::button);blocked.input_blocked=true;
    Win32AccessibilityTree tree({root,group,wrapper,tab,secret,hidden,blocked});
    const auto tabs=tree.find("tabs"),item=tree.find("tab"),field=tree.find("password");
    check(tabs&&item&&field&&!tree.find("layout")&&!tree.find("wrapper")&&!tree.find("hidden")&&!tree.find("blocked"),"Wrong exposed nodes");
    check(tree.node(*item).parent==tabs&&tree.node(*tabs).children==std::vector<std::size_t>{*item},"Tab group lost its child");
    check(tree.focus()==item&&tree.node(*item).role==Win32AccessibilityRole::tab&&tree.node(*item).selected,"Focused tab state was lost");
    check(tree.node(*field).name=="Password"&&tree.node(*field).snapshot.text.empty()&&tree.node(*field).snapshot.composition_text.empty()&&!tree.node(*field).value,"Password content escaped");
}
void invalid_and_retired(){
    auto a=node("duplicate","",WidgetKind::button),b=a;
    auto orphan=node("orphan","duplicate",WidgetKind::button);
    auto left=node("left","right",WidgetKind::vertical_stack),right=node("right","left",WidgetKind::vertical_stack);
    auto cycle=node("cycle","left",WidgetKind::button);
    auto parent=node("hidden-parent","",WidgetKind::vertical_stack);parent.visible=false;
    auto child=node("hidden-child","hidden-parent",WidgetKind::button);
    auto old=node("@iiscope:ax:1:4","",WidgetKind::action_surface);
    Win32AccessibilityTree first({a,b,orphan,left,right,cycle,parent,child,old});
    check(!first.find("duplicate")&&!first.find("orphan")&&!first.find("cycle")&&!first.find("hidden-child"),"Invalid ancestry leaked into the accessibility tree");
    auto fresh=node("@iiscope:ax:1:5","",WidgetKind::action_surface);
    Win32AccessibilityTree next({fresh});
    check(first.find(old.id)&&!next.find(old.id)&&next.find(fresh.id),"A retired virtual identity was reused");
}
void geometry_and_actions(){
    auto outer=node("panel","",WidgetKind::popover,{0,0,100,100});
    auto button=node("button","panel",WidgetKind::button,{10,10,50,50});button.visual_clip_bounds=Rect{20,20,20,20};
    auto inner=node("inner","button",WidgetKind::action_surface,{25,25,10,10});
    auto disabled=node("disabled","panel",WidgetKind::button,{60,0,20,20});disabled.enabled=false;
    auto passive=node("cell","panel",WidgetKind::action_surface);passive.accessibility_semantics=AccessibilitySemantics{.role=AccessibilityRole::cell};passive.accessibility_actionable=false;
    Win32AccessibilityTree tree({outer,button,inner,disabled,passive});
    const auto button_id=tree.find("button"),inner_id=tree.find("inner"),disabled_id=tree.find("disabled"),passive_id=tree.find("cell");
    check(button_id&&inner_id&&disabled_id&&passive_id,"Missing test nodes");
    check(tree.node(*button_id).bounds.x==20&&tree.node(*button_id).bounds.width==20,"Clipping was not applied");
    check(tree.hit(26,26)==inner_id&&tree.hit(15,15)!=button_id,"Hit test ignored depth or clipping");
    check(tree.node(*button_id).actionable&&!tree.node(*disabled_id).actionable&&!tree.node(*passive_id).actionable,"Action policy ignored disabled or read-only nodes");
}
void action_queue(){
    Win32AccessibilityQueue queue;
    check(!queue.admit({"field",WidgetInputKind::focus,0}),"An action without an object generation was accepted");
    auto first=queue.admit({"@iiscope:ax:1:4",WidgetInputKind::activate,1});
    auto delivered=first?queue.take(*first):std::nullopt;
    check(delivered&&delivered->target=="@iiscope:ax:1:4"&&delivered->object_id==1&&!queue.take(*first),"Action did not retain its generation exactly once");
    auto pending=queue.admit({"field",WidgetInputKind::focus,2});
    check(pending&&*pending!=*first,"Action identifiers were reused");
    queue.close();check(!queue.take(*pending)&&!queue.admit({"field",WidgetInputKind::focus,2}),"Retired bridge admitted an action");
    Win32AccessibilityQueue concurrent;std::atomic<unsigned> completed=0;std::vector<std::thread> workers;
    for(unsigned i=0;i<4;++i)workers.emplace_back([&]{for(unsigned n=0;n<64;++n){
        auto id=concurrent.admit({"field",WidgetInputKind::focus,2});if(id&&concurrent.take(*id))++completed;
    }});
    for(auto& worker:workers)worker.join();check(completed==256,"Concurrent accessibility actions were lost");
}
void event_identities(){
    Win32AccessibilityObjectIds ids;
    auto first=ids.update({"field","@iiscope:ax:1:4"});
    const auto field=first.at("field"),old=first.at("@iiscope:ax:1:4");
    auto second=ids.update({"field","@iiscope:ax:1:5"});
    check(second.at("field")==field&&second.at("@iiscope:ax:1:5")!=old,"Accessibility event reused a retired object identity");
    auto third=ids.update({"@iiscope:ax:1:5"});(void)third;
    auto again=ids.update({"field"});check(again.at("field")!=field,"A reintroduced object retained a retired event ID");
}
void virtual_generation(){
    auto owner=std::make_shared<WidgetState>([](ii::runtime::Task task){task();});
    auto table=owner->absolute_stack({},"table",{120,80});check(bool(table),"Cannot create a table");
    check(bool(table->set_accessibility_semantics(AccessibilitySemantics{.role=AccessibilityRole::table})),"Cannot set table semantics");
    check(bool(owner->drawing_snapshot({120,80})),"Cannot lay out the table");unsigned activated=0;
    VirtualAccessibilityAction handler=[&](std::string_view,WidgetInputKind kind)->ii::core::Result<void>{if(kind==WidgetInputKind::activate)++activated;return {};};
    VirtualAccessibilityNode cell{.key="cell",.name="Cell",.bounds={0,0,30,20}};
    check(bool(table->set_virtual_accessibility({cell},handler)),"Cannot publish a cell");
    auto snapshots=owner->accessibility_snapshots();check(bool(snapshots),"Cannot read accessibility snapshots");
    for(auto& value:*snapshots)value.attached=true;
    auto previous=std::ranges::find_if(*snapshots,[](const auto& n){return n.native_class=="iiGUI.VirtualAccessibility";});
    check(previous!=snapshots->end(),"Virtual cell is missing");const auto old=previous->id;
    Win32AccessibilityTree first(*snapshots);check(first.find(old).has_value(),"Projection omitted a published virtual cell");
    Win32AccessibilityQueue pending;auto token=pending.admit({old,WidgetInputKind::activate,1});check(token.has_value(),"Cannot queue a virtual action");
    check(bool(table->set_virtual_accessibility({},handler))&&bool(table->set_virtual_accessibility({cell},handler)),"Cannot replace a virtual cell");
    snapshots=owner->accessibility_snapshots();check(bool(snapshots),"Cannot read replacement snapshots");for(auto& value:*snapshots)value.attached=true;
    Win32AccessibilityTree replacement(*snapshots);auto request=pending.take(*token);
    if(request&&replacement.find(request->target))check(bool(owner->input({.kind=request->kind,.target=request->target})),"Cannot deliver a live action");
    check(!replacement.find(old)&&activated==0,"A queued action reached a replacement cell");owner->close();
}
}
int main(){hierarchy_and_privacy();invalid_and_retired();geometry_and_actions();action_queue();event_identities();virtual_generation();}
