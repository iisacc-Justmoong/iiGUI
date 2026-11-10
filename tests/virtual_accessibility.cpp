#include <iiGUI/WidgetState.h>
#include <iiGUI/AccessibilityGrouping.h>
#include <algorithm>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
template<class T>T take(ii::core::Result<T> result){if(!result)throw std::runtime_error(result.error().message);return std::move(*result);}
void good(ii::core::Result<void> result){if(!result)throw std::runtime_error(result.error().message);}
void headers(){
    using namespace ii::gui;
    auto tree=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{200,100}));Widget header;
    good(host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto root=builder.action_surface({},"header",{.width=120,.height=24});if(!root)return std::unexpected(root.error());header=*root;
        return header.set_accessibility_semantics(AccessibilitySemantics{.role=AccessibilityRole::column_header_group,.name="Columns"});
    }));
    (void)take(tree->drawing_snapshot({200,100}));int activated=0,focused=0;
    require(!take(header.snapshot()).accessibility_actionable,"A header group was exposed as an actionable control");
    VirtualAccessibilityAction handler=[&](std::string_view key,WidgetInputKind kind)->ii::core::Result<void>{
        require(key=="a","The wrong header identity was delivered");if(kind==WidgetInputKind::activate)++activated;else if(kind==WidgetInputKind::focus)++focused;return {};
    };
    VirtualAccessibilityNode column{.key="a",.name="Amount",.bounds={0,0,60,24},.role=AccessibilityRole::column_header,.actionable=false};
    good(header.set_virtual_accessibility({column},handler));auto nodes=take(tree->accessibility_snapshots());
    auto child=std::ranges::find_if(nodes,[](const auto& value){return value.native_class=="iiGUI.VirtualAccessibility";});
    require(child!=nodes.end()&&child->accessibility_semantics&&child->accessibility_semantics->role==AccessibilityRole::column_header&&
            !child->accessibility_actionable&&child->enabled,"A read-only header lost its role or action state");
    const auto child_id=child->id;const auto group=ii::gui::detail::accessibility_group_parents(nodes);
    const auto slot=std::size_t(child-nodes.begin());require(group[slot]&&nodes[*group[slot]].id==take(header.snapshot()).id,
            "A virtual column header was not grouped under its semantic owner");
    good(tree->input({.kind=WidgetInputKind::focus,.target=child_id}));
    good(tree->input({.kind=WidgetInputKind::activate,.target=child_id}));
    require(focused==1&&activated==0,"A non-actionable header received activation");
    column.actionable=true;good(header.set_virtual_accessibility({column},handler));
    require(take(tree->accessibility_snapshot(child_id)).accessibility_actionable,"A retained header did not update its action state");
    good(tree->input({.kind=WidgetInputKind::activate,.target=child_id}));require(activated==1,"An actionable header did not receive activation");
    auto bad=column;bad.role=AccessibilityRole::table;
    require(!header.set_virtual_accessibility({bad},handler)&&take(tree->accessibility_snapshot(child_id)).accessibility_actionable,
            "Invalid virtual roles replaced published headers");
    require(!header.set_accessibility_semantics(AccessibilitySemantics{.role=AccessibilityRole::column_header}),
            "A virtual-only header role was accepted for a real container");
    good(host.clear_content());require(!tree->accessibility_snapshot(child_id),"A retired header retained its virtual child");tree->close();
}
}
int main(){
    namespace gui=ii::gui;
    using gui::VirtualAccessibilityNode;
    auto tree=std::make_shared<gui::detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{300,200}));gui::Widget body;
    good(host.set_content([&](gui::WidgetBuilder& builder)->ii::core::Result<void>{
        auto root=builder.absolute_stack({},"root",{300,200});if(!root)return std::unexpected(root.error());
        auto node=builder.absolute_stack(*root,"table",{100,80});if(!node)return std::unexpected(node.error());body=*node;
        return body.set_frame({20,30,100,80});
    }));
    (void)take(tree->drawing_snapshot({300,200}));
    auto physical=take(tree->snapshots());
    int actions=0;std::string key;gui::WidgetInputKind kind{};
    gui::VirtualAccessibilityAction handler=[&](std::string_view value,gui::WidgetInputKind action)->ii::core::Result<void>{++actions;key=value;kind=action;return {};};
    std::vector<VirtualAccessibilityNode> children{
        {.key="row:1/column:a",.name="A, row 1",.value="alpha",.bounds={-10,5,40,20},.selected=true},
        {.key="row:1/column:b",.name="B, row 1",.value="beta",.bounds={30,5,40,20},.enabled=false},
        {.key="offscreen",.name="outside",.bounds={120,5,20,20}}
    };
    good(body.set_virtual_accessibility(children,handler));
    require(take(tree->snapshots()).size()==physical.size(),"Virtual children consumed real widget slots");
    auto nodes=take(tree->accessibility_snapshots());
    const auto find=[&](std::string_view name)->gui::WidgetSnapshot {
        const auto found=std::ranges::find_if(nodes,[&](const auto& item){return item.accessibility_name==name;});
        if(found==nodes.end())throw std::runtime_error("Missing virtual accessibility node");return *found;
    };
    const auto first=find("A, row 1"), disabled=find("B, row 1"), offscreen=find("outside");
    require(first.accessibility_semantics&&first.accessibility_semantics->role==gui::AccessibilityRole::cell&&first.selected&&first.text=="alpha",
            "Virtual cell metadata was not exposed");
    require(first.parent_id==take(body.snapshot()).id&&first.visual_clip_bounds&&first.visual_clip_bounds->x==20&&
            first.visual_bounds.x==10&&first.visual_bounds.y==35,"Virtual cell coordinates or clipping were incorrect");
    require(!offscreen.visible,"An offscreen virtual cell remained visible");
    good(tree->input({.kind=gui::WidgetInputKind::activate,.target=first.id}));
    require(actions==1&&key==children[0].key&&kind==gui::WidgetInputKind::activate,"Virtual activation did not reach its owner");
    good(tree->input({.kind=gui::WidgetInputKind::focus,.target=first.id}));
    require(actions==2&&kind==gui::WidgetInputKind::focus,"Virtual focus did not reach its owner");
    good(tree->input({.kind=gui::WidgetInputKind::activate,.target=disabled.id}));
    good(tree->input({.kind=gui::WidgetInputKind::activate,.target=offscreen.id}));
    require(actions==2,"Disabled or clipped virtual cells received an action");
    children[0].value="updated";children[0].bounds.x=0;good(body.set_virtual_accessibility(children,handler));
    require(take(tree->accessibility_snapshot(first.id)).text=="updated","Retained virtual identity did not expose updated content");
    auto invalid=children;invalid[1].key=invalid[0].key;
    require(!body.set_virtual_accessibility(invalid,handler)&&take(tree->accessibility_snapshot(first.id)).text=="updated",
            "Duplicate virtual keys partially changed the published children");
    invalid=children;invalid[0].bounds.width=std::numeric_limits<float>::quiet_NaN();
    require(!body.set_virtual_accessibility(invalid,handler),"Invalid virtual geometry was accepted");
    invalid=children;invalid[0].name=std::string("bad\0text",8);
    require(!body.set_virtual_accessibility(invalid,handler),"Invalid virtual text was accepted");
    good(body.set_visible(false));good(tree->input({.kind=gui::WidgetInputKind::activate,.target=first.id}));
    require(actions==2,"Hidden parent allowed a virtual action");good(body.set_visible(true));
    children.erase(children.begin());good(body.set_virtual_accessibility(children,handler));
    require(!tree->accessibility_snapshot(first.id)&&!tree->input({.kind=gui::WidgetInputKind::activate,.target=first.id}),
            "Removed virtual identity still addressed a child");
    children.push_back({.key="row:1/column:a",.name="recreated",.bounds={0,0,20,20}});
    good(body.set_virtual_accessibility(children,handler));nodes=take(tree->accessibility_snapshots());
    require(find("recreated").id!=first.id,"A retired virtual identity was reused");
    std::vector<VirtualAccessibilityNode> many;many.reserve(3072);
    for(int i=0;i<3072;++i)many.push_back({.key=std::to_string(i),.name="Cell",.bounds={0,0,1,1}});
    good(body.set_virtual_accessibility(many,handler));
    require(take(tree->accessibility_snapshots()).size()==physical.size()+many.size(),"A large virtual collection was truncated");
    many.push_back({.key="excess",.name="Cell",.bounds={0,0,1,1}});
    require(!body.set_virtual_accessibility(many,handler),"The virtual collection budget was not enforced");
    good(body.set_virtual_accessibility({{.key="one",.name="one",.bounds={0,0,10,10}}},handler));
    nodes=take(tree->accessibility_snapshots());const auto one=find("one");
    good(body.set_enabled(false));good(tree->input({.kind=gui::WidgetInputKind::activate,.target=one.id}));
    require(actions==2,"A disabled virtual owner received an action");good(body.set_enabled(true));
    good(body.set_virtual_accessibility({{.key="one",.name="one",.bounds={0,0,10,10}}},
        [&](std::string_view,gui::WidgetInputKind)->ii::core::Result<void>{good(body.set_virtual_accessibility({},{}));return {};}));
    good(tree->input({.kind=gui::WidgetInputKind::activate,.target=one.id}));
    require(!tree->accessibility_snapshot(one.id),"A reentrant action retained a removed child");
    good(host.clear_content());require(!tree->accessibility_snapshot(first.id),"A retired host retained a virtual child");tree->close();headers();
}
