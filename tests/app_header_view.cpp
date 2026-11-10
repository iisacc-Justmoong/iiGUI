#include <iiGUI/NativeAppHeaderView.h>
#include <iiGUI/WidgetState.h>
#include <algorithm>
#include <cmath>
#include <memory>
#include <optional>
#include <stdexcept>

namespace {
void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
template<class T>T take(ii::core::Result<T> value){if(!value)throw std::runtime_error(value.error().message);return std::move(*value);}
void good(ii::core::Result<void> value){if(!value)throw std::runtime_error(value.error().message);}
void retained_actions(){
    auto tree=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});
    auto outer=take(tree->content_host({},"outer",{640,180}));ii::gui::AppHeaderOptions config;
    config.title="Project";config.subtitle="Editor";config.menu_visible=true;config.menu_focus={.25,.5,.75,1};int menus=0;
    std::optional<ii::gui::NativeAppHeader> header;ii::gui::Widget action;
    good(outer.set_content([&](auto& builder)->ii::core::Result<void>{
        auto created=ii::gui::NativeAppHeader::create(builder,{},"header",config,[&]{++menus;});
        if(!created)return std::unexpected(created.error());header=*created;
        auto child=builder.action_surface(header->actions_root(),"action",{.text="Export",.minimum_height=28},{});
        if(!child)return std::unexpected(child.error());action=*child;return {};
    }));
    good(header->menu().activate());require(menus==1&&take(action.snapshot()).text=="Export","Header menu or action slot failed");
    good(header->menu().focus());auto focused=take(tree->drawing_snapshot({640,180}));const auto bounds=take(header->menu().snapshot()).bounds;
    const auto focus_ring=[&](const auto& panels){return std::ranges::any_of(panels,[&](const auto& panel){return panel.bounds==bounds&&panel.stroke_width>0;});};
    require(focus_ring(focused.panels)||std::ranges::any_of(focused.overlays,[&](const auto& layer){return focus_ring(layer.panels);}),
        "Focused menu lost its visible focus ring");
    const auto expected=[](double c){return float(c<=.04045?c/12.92:std::pow((c+.055)/1.055,2.4));};
    const auto supplied_focus=[&](const auto& panels){return std::ranges::any_of(panels,[&](const auto& p){
        return p.bounds==bounds&&p.stroke_width>0&&std::abs(p.top.red-expected(.25))<1e-5f&&
            std::abs(p.top.green-expected(.5))<1e-5f&&std::abs(p.top.blue-expected(.75))<1e-5f;});};
    require(supplied_focus(focused.panels)||std::ranges::any_of(focused.overlays,[&](const auto& layer){return supplied_focus(layer.panels);}),
        "Header did not use its supplied menu focus color");
    int replaced=0;good(header->set_menu_clicked([&]{++replaced;}));good(header->menu().activate());
    require(replaced==1&&menus==1,"Replacing the menu callback changed its action slot");
    config.compact=true;config.title="Updated";config.subtitle="";config.action_spacing=12;good(header->set_options(config));
    require(take(action.snapshot()).text=="Export"&&take(header->root().measure(640)).height>=56,"Header updates replaced its action or broke minimum height");
    auto frame=take(tree->drawing_snapshot({640,180}));const auto contains=[](const auto& runs){return std::ranges::any_of(runs,[](const auto& item){return item.text=="Updated";});};
    require(contains(frame.texts)||std::ranges::any_of(frame.overlays,[&](const auto& layer){return contains(layer.texts);}),"Header title did not update");
    auto invalid=config;invalid.row_spacing=-1;require(!header->set_options(invalid)&&action.snapshot(),"Invalid header input changed the live action");
    good(outer.clear_content());require(!action.snapshot()&&!header->set_options(config),"Retired header remained usable");tree->close();
}
void reentry_and_exception(){
    auto tree=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});
    auto outer=take(tree->content_host({},"outer",{640,180}));ii::gui::AppHeaderOptions config;config.title="Original";
    std::optional<ii::gui::NativeAppHeader> header;ii::gui::Widget action;bool throw_cancel=false,reentered=false;
    good(outer.set_content([&](auto& builder)->ii::core::Result<void>{
        auto created=ii::gui::NativeAppHeader::create(builder,{},"header",config);if(!created)return std::unexpected(created.error());header=*created;
        auto child=builder.action_surface(header->actions_root(),"action",{.text="Action",.minimum_width=100,.minimum_height=32},{});
        if(!child)return std::unexpected(child.error());action=*child;
        return child->set_control_events({.canceled=[&]{if(throw_cancel)throw std::runtime_error("Cancellation callback failed");
            auto latest=config;latest.title="Reentrant";good(header->set_options(latest));reentered=true;}});
    }));
    const auto press=[&]{auto bounds=take(action.snapshot()).bounds;good(tree->input({.kind=ii::gui::WidgetInputKind::pointer_down,.x=bounds.x+4,.y=bounds.y+4}));};
    auto disabled=config;disabled.enabled=false;press();good(header->set_options(disabled));
    require(reentered&&take(header->root().snapshot()).enabled,"A reentrant header update was overwritten");
    throw_cancel=true;press();bool escaped=false;ii::core::Result<void> failed;
    try{failed=header->set_options(disabled);}catch(...){escaped=true;}
    require(!escaped&&!failed&&failed.error().cause,"A cancellation exception escaped the header interface");
    good(header->set_options(config));require(take(header->root().snapshot()).enabled,"Header did not recover after a callback exception");
    good(outer.clear_content());tree->close();
}
void partial_updates(){
    unsigned accessibility_changes=0;
    ii::gui::detail::WidgetFrameHooks hooks;
    hooks.accessibility_changed=[&]{++accessibility_changes;};
    auto tree=std::make_shared<ii::gui::detail::WidgetState>(
        [](ii::runtime::Task task){task();},ii::gui::WidgetAppearance{},std::move(hooks));
    auto outer=take(tree->content_host({},"outer",{640,180}));
    ii::gui::AppHeaderOptions config;config.title="Title";config.menu_visible=true;
    std::optional<ii::gui::NativeAppHeader> header;
    good(outer.set_content([&](auto& builder)->ii::core::Result<void>{
        auto candidate=ii::gui::NativeAppHeader::create(builder,{},"header",config);
        if(!candidate)return std::unexpected(candidate.error());header=*candidate;return {};
    }));
    accessibility_changes=0;good(header->set_options(config));
    require(accessibility_changes==0,"Unchanged header input republished accessibility");
    config.menu_focus={.1,.4,.8,1};good(header->set_options(config));
    require(accessibility_changes==0,"A focus-color update republished unrelated accessibility");
    good(outer.clear_content());tree->close();
}
}
int main(){retained_actions();reentry_and_exception();partial_updates();}
