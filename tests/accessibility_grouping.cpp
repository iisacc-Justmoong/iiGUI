#include <iiGUI/AccessibilityGrouping.h>
#include <iiGUI/WidgetState.h>

#include <algorithm>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(ii::core::Result<T> result){if(!result)throw std::runtime_error(result.error().message);return std::move(*result);}
void good(ii::core::Result<void> result){if(!result)throw std::runtime_error(result.error().message);}
std::size_t index(const std::vector<ii::gui::WidgetSnapshot>& values,std::string_view suffix){
    const auto found=std::ranges::find_if(values,[&](const auto& value){return value.id.ends_with(suffix);});
    if(found==values.end())throw std::runtime_error("Missing accessibility snapshot: "+std::string(suffix));return std::size_t(found-values.begin());
}
void synthetic(){
    using namespace ii::gui;
    auto node=[](std::string id,std::string parent,WidgetKind kind){WidgetSnapshot value{};value.id=std::move(id);value.parent_id=std::move(parent);value.kind=kind;value.attached=true;return value;};
    auto tabs=node("tabs","",WidgetKind::horizontal_stack);tabs.accessibility_semantics=AccessibilitySemantics{.role=AccessibilityRole::tab_group};
    auto section=node("section","tabs",WidgetKind::content_host),wrap=node("wrap","section",WidgetKind::horizontal_stack);
    require(!detail::accessibility_structural_only(section)&&detail::accessibility_structural_only(wrap)&&
            !detail::accessibility_structural_only(tabs),"The native host's existing structural filter changed");
    auto text_wrapper=section;text_wrapper.text_input=true;
    require(!detail::accessibility_structural_only(text_wrapper),"A wrapper's custom text provider was suppressed");
    auto first=node("first","tabs",WidgetKind::action_surface);first.action_role=ActionRole::tab;
    auto nested=node("nested","wrap",WidgetKind::action_surface);nested.action_role=ActionRole::tab;nested.selected=true;
    auto menu=node("menu","wrap",WidgetKind::vertical_stack);menu.accessibility_semantics=AccessibilitySemantics{.role=AccessibilityRole::menu};
    auto menu_wrap=node("menu-wrap","menu",WidgetKind::content_host);
    auto item=node("item","menu-wrap",WidgetKind::action_surface);item.action_role=ActionRole::menu_item;
    auto wrong=node("wrong","menu-wrap",WidgetKind::action_surface);wrong.action_role=ActionRole::tab;
    auto barrier=node("barrier","tabs",WidgetKind::action_surface);barrier.action_role=ActionRole::button;
    auto blocked=node("blocked","barrier",WidgetKind::action_surface);blocked.action_role=ActionRole::tab;
    auto loop_a=node("loop-a","loop-b",WidgetKind::content_host),loop_b=node("loop-b","loop-a",WidgetKind::horizontal_stack);
    auto cycle=node("cycle","loop-a",WidgetKind::action_surface);cycle.action_role=ActionRole::tab;
    auto hidden_wrap=node("hidden-wrap","tabs",WidgetKind::content_host);hidden_wrap.visible=false;
    auto hidden_child=node("hidden-child","hidden-wrap",WidgetKind::action_surface);hidden_child.action_role=ActionRole::tab;
    auto inner=node("inner-tabs","wrap",WidgetKind::horizontal_stack);inner.accessibility_semantics=AccessibilitySemantics{.role=AccessibilityRole::tab_group};
    auto inner_tab=node("inner-tab","inner-tabs",WidgetKind::action_surface);inner_tab.action_role=ActionRole::tab;
    auto popup=node("popup","tabs",WidgetKind::popover);
    auto popup_tab=node("popup-tab","popup",WidgetKind::action_surface);popup_tab.action_role=ActionRole::tab;
    auto disabled_wrap=node("blocked-wrap","tabs",WidgetKind::content_host);disabled_wrap.input_blocked=true;
    auto disabled_child=node("blocked-child","blocked-wrap",WidgetKind::action_surface);disabled_child.action_role=ActionRole::tab;
    std::vector<WidgetSnapshot> values{nested,item,wrong,first,tabs,wrap,section,menu,menu_wrap,barrier,blocked,loop_a,loop_b,cycle,hidden_wrap,hidden_child,inner,inner_tab,popup,popup_tab,disabled_wrap,disabled_child};
    auto groups=detail::accessibility_group_parents(values);
    require(groups.size()==values.size()&&groups[0]==4&&groups[1]==7&&groups[3]==4,
        "Nested or out-of-order accessibility group membership was lost");
    require(groups[17]==16&&!groups[2]&&!groups[10]&&!groups[13]&&!groups[15]&&!groups[19]&&!groups[21],
        "A closer group was missed or a semantic, interactive, popup, hidden, blocked or cyclic boundary was crossed");
    auto duplicate=tabs;values.push_back(duplicate);groups=detail::accessibility_group_parents(values);
    require(!groups[0]&&!groups[3]&&groups[1]==7,"An ambiguous parent id was attached to an arbitrary group");
    auto table=node("table","",WidgetKind::absolute_stack);table.accessibility_semantics=AccessibilitySemantics{.role=AccessibilityRole::table};
    auto cell=node("cell","table",WidgetKind::action_surface);cell.accessibility_semantics=AccessibilitySemantics{.role=AccessibilityRole::cell};
    std::vector<WidgetSnapshot> grid{cell,table};auto cell_groups=detail::accessibility_group_parents(grid);
    require(cell_groups[0]==1&&!cell_groups[1],"A virtual table cell was not grouped under its table");
    auto scroll=node("scroll","table",WidgetKind::scroll_view);
    auto body=node("body","scroll",WidgetKind::absolute_stack);cell.parent_id="body";
    auto header=node("header","table",WidgetKind::absolute_stack);
    auto column=node("column","header",WidgetKind::action_surface);column.accessibility_semantics=AccessibilitySemantics{.role=AccessibilityRole::column_header};
    auto standalone=node("standalone","",WidgetKind::action_surface);standalone.accessibility_semantics=AccessibilitySemantics{.role=AccessibilityRole::column_header_group};
    auto separate=column;separate.id="separate";separate.parent_id="standalone";
    std::vector<WidgetSnapshot> hierarchical{cell,body,scroll,table,column,header,standalone,separate};
    auto hierarchy=detail::accessibility_group_parents(hierarchical);
    require(hierarchy[0]==3&&hierarchy[4]==3&&hierarchy[7]==6,
            "Table headers and scrolled cells did not attach to their nearest semantic owner");
    scroll.visible=false;hierarchical[2]=scroll;hierarchy=detail::accessibility_group_parents(hierarchical);
    require(!hierarchy[0]&&hierarchy[4]==3,"A hidden table viewport leaked cells or hid its header");
}
void deep(){
    using namespace ii::gui;std::vector<WidgetSnapshot> values;values.reserve(4097);
    WidgetSnapshot group{};group.id="root";group.kind=WidgetKind::horizontal_stack;group.accessibility_semantics=AccessibilitySemantics{.role=AccessibilityRole::tab_group};values.push_back(group);
    std::string parent="root";
    for(int n=0;n<3000;++n){WidgetSnapshot wrapper{};wrapper.id="wrapper-"+std::to_string(n);wrapper.parent_id=parent;wrapper.kind=WidgetKind::content_host;parent=wrapper.id;values.push_back(std::move(wrapper));}
    for(int n=0;n<1000;++n){WidgetSnapshot tab{};tab.id="tab-"+std::to_string(n);tab.parent_id=parent;tab.kind=WidgetKind::action_surface;tab.action_role=ActionRole::tab;values.push_back(std::move(tab));}
    const auto groups=detail::accessibility_group_parents(values);
    require(std::ranges::all_of(groups.begin()+3001,groups.end(),[](const auto& value){return value==0;}),
        "A deep shared wrapper chain lost tab membership");
}
void mounted(){
    using namespace ii::gui;
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{400,100}));Widget tabs,section,tab;
    good(host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto root=builder.hstack({},"tabs",{});if(!root)return std::unexpected(root.error());tabs=*root;
        if(auto semantic=root->set_accessibility_semantics(AccessibilitySemantics{.role=AccessibilityRole::tab_group,.name="Sections"});!semantic)return semantic;
        auto slot=builder.content_host(*root,"dynamic");if(!slot)return std::unexpected(slot.error());section=*slot;return {};
    }));
    good(section.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto wrapper=builder.hstack({},"wrapper",{});if(!wrapper)return std::unexpected(wrapper.error());
        auto child=builder.action_surface(*wrapper,"dynamic-tab",{.text="Dynamic",.role=ActionRole::tab,.checked=true});
        if(!child)return std::unexpected(child.error());tab=*child;return {};
    }));
    auto values=take(tree->snapshots());auto groups=detail::accessibility_group_parents(values);
    auto tab_index=index(values,"/dynamic-tab"),group_index=index(values,"/tabs");
    require(groups[tab_index]==group_index&&values[tab_index].selected,
        "A dynamic keyed generation was not grouped under its native tab group");
    good(section.clear_content());values=take(tree->snapshots());groups=detail::accessibility_group_parents(values);
    require(std::ranges::none_of(values,[](const auto& value){return value.id.ends_with("/dynamic-tab");})&&groups.size()==values.size()&&!tab.snapshot(),
        "A retired dynamic tab survived in the accessibility grouping");
    tree->close();
}
}
int main(){synthetic();deep();mounted();}
