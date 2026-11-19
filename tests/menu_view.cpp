#include <iiGUI/NativeMenuView.h>
#include <iiGUI/WidgetState.h>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <vector>

namespace {
using namespace ii::gui;using ii::core::Result;
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(Result<T> value){if(!value)throw std::runtime_error(value.error().message);return std::move(*value);}
void good(Result<void> value){if(!value)throw std::runtime_error(value.error().message);}
struct Input final {MenuConfiguration configuration;int selected_index=-1;friend bool operator==(const Input&,const Input&)=default;};
struct Callbacks final {std::function<void(int)> triggered;std::function<void()> opened,closed,canceled;};
struct Presentation final {std::optional<bool> selected,compact,show_icon_slot;std::optional<double> width;
    friend bool operator==(const Presentation&,const Presentation&)=default;};
using Activation=std::function<void(std::function<void()>)>;
using CloseAction=std::function<Result<void>()>;using ClosePlan=std::function<Result<CloseAction>()>;
struct PopupPort final {std::function<Result<WidgetSnapshot>()> snapshot;
    std::function<Result<void>(Widget,MenuDirection,CloseAction)> open;std::function<Result<void>()> close;};
struct ItemPort final {
    std::function<Result<void>(Presentation)> present;std::function<Result<void>(Activation)> activated;
    std::function<Result<void>(std::function<Result<void>()>)> changed;std::function<Result<MenuEntry>()> entry;
    std::function<Result<WidgetSnapshot>()> snapshot;std::function<Result<void>()> focus;
    std::function<Result<bool>()> open_submenu;std::function<Result<void>(ClosePlan)> parent_close;
    std::function<bool()> retired;std::function<Result<bool>()> submenu_open;std::function<Result<void>()> close_submenu;
    std::function<Result<Widget>()> widget;std::vector<Widget> layout_groups;
};
using SectionPort=std::function<Result<void>(std::vector<ItemPort>)>;
using RegistrationPort=std::function<Result<void>(ItemPort)>;
struct SectionAccess final {std::function<Result<SectionPort>(Widget)> register_section;std::function<Result<Input>()> options;};
struct Policy final {
    using Model=MenuModel;using Input=::Input;using Callbacks=::Callbacks;using ItemPort=::ItemPort;using Presentation=::Presentation;
    using Activation=::Activation;using CloseAction=::CloseAction;using ClosePlan=::ClosePlan;using Entry=MenuEntry;
    using EntryKind=MenuEntryKind;using EntryState=MenuItemState;using Configuration=MenuConfiguration;using Layout=MenuLayout;
    using Color=WidgetColor;using Error=MenuError;using ErrorCode=MenuErrorCode;using Navigation=MenuNavigation;
    using Direction=MenuDirection;using PopupPort=::PopupPort;using SectionPort=::SectionPort;using SectionAccess=::SectionAccess;
    using RegistrationPort=::RegistrationPort;
};

// A context menu keeps hit testing and keyboard focus without publishing
// pointer or key feedback to the item or its presentation.
void context_menu_uses_stable_item_appearance() {
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    const auto host=take(tree->content_host({},"host",{320,180}));
    Widget menu,first,second;
    int first_clicks=0,second_clicks=0,presses=0,releases=0;
    good(host.set_content([&](WidgetBuilder& builder)->Result<void>{
        menu=take(builder.menu({},"context",{.style=MenuStyle::context,.auto_close_on_trigger=false}));
        first=take(builder.menu_item(menu,"first",{.label="First",.has_child_items=false},[&]{++first_clicks;}));
        second=take(builder.menu_item(menu,"second",{.label="Second",.has_child_items=false},[&]{++second_clicks;}));
        return second.set_control_events({.pressed=[&]{++presses;},.released=[&]{++releases;}});
    }));
    good(menu.open_at(16,16));
    const auto idle=take(tree->drawing_snapshot({320,180}));
    const auto second_bounds=take(second.snapshot()).visual_bounds;
    const auto x=second_bounds.x+second_bounds.width/2;
    const auto y=second_bounds.y+second_bounds.height/2;
    good(tree->input({.kind=WidgetInputKind::pointer_move,.x=x,.y=y}));
    require(!take(second.snapshot()).hovered&&take(first.snapshot()).focused,"Context menu hover changed the item state");
    good(tree->input({.kind=WidgetInputKind::pointer_down,.x=x,.y=y}));
    require(!take(second.snapshot()).pressed&&presses==0,"Context menu published a transient press");
    const auto pressed=take(tree->drawing_snapshot({320,180}));
    require(idle.overlays.size()==pressed.overlays.size(),"Context menu press changed the drawing layers");
    for(std::size_t i=0;i<idle.overlays.size();++i)
        require(idle.overlays[i].panels==pressed.overlays[i].panels,"Context menu press changed the rendered panels");
    good(tree->input({.kind=WidgetInputKind::pointer_up,.x=x,.y=y}));
    require(second_clicks==1&&releases==0,"Context menu pointer activation did not complete exactly once");
    good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::up}));
    good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::enter}));
    good(tree->input({.kind=WidgetInputKind::key_up,.key=WidgetKey::enter}));
    require(first_clicks==1,"Context menu keyboard activation was lost");
    tree->close();
}

// New content generations inherit the policy even though their widget indices
// are local to a separate state tree.
void popup_policy_reaches_embedded_content() {
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    const auto host=take(tree->content_host({},"host",{320,180}));
    Widget popup,section,action;
    good(host.set_content([&](WidgetBuilder& builder)->Result<void>{
        popup=take(builder.popover({},"context",{.width=160,.height=64,.padding=0}));
        good(popup.suppress_popup_transient_feedback());
        section=take(builder.content_host(popup,"section",{160,64}));
        return {};
    }));
    int clicks=0,hover_events=0;
    std::optional<WidgetPaintState> painted;
    good(section.set_content([&](WidgetBuilder& builder)->Result<void>{
        action=take(builder.action_surface({},"action",{.text="Run",.width=140,.height=22},[&]{++clicks;}));
        good(action.set_input_handler([&](const auto& event)->Result<bool>{
            if(event.kind==WidgetInputKind::pointer_move)++hover_events;
            return false;
        }));
        return action.set_paint_handler([&](const WidgetPaintState& state)->Result<std::shared_ptr<const WidgetDrawing>>{
            painted=state;
            return std::make_shared<const WidgetDrawing>();
        });
    }));
    good(popup.open_at(24,24));
    (void)take(tree->drawing_snapshot({320,180}));
    const auto bounds=take(action.snapshot()).visual_bounds;
    const auto x=bounds.x+bounds.width/2,y=bounds.y+bounds.height/2;
    good(tree->input({.kind=WidgetInputKind::pointer_move,.x=x,.y=y}));
    good(tree->input({.kind=WidgetInputKind::pointer_down,.x=x,.y=y}));
    (void)take(tree->drawing_snapshot({320,180}));
    require(painted&&!painted->hovered&&!painted->pressed&&!painted->focused&&hover_events==0,
            "An embedded popup control received transient feedback");
    good(tree->input({.kind=WidgetInputKind::pointer_up,.x=x,.y=y}));
    require(clicks==1,"The embedded popup action lost its click");
    tree->close();
}
}
int main(){
    context_menu_uses_stable_item_appearance();
    popup_policy_reaches_embedded_content();
    using Menu=ii::gui::BasicNativeMenu<Policy>;
    auto tree=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{320,100}));std::optional<Menu> menu;
    auto payload=std::make_shared<int>(7);std::weak_ptr<int> weak=payload;int opened=0;
    good(host.set_content([&](auto& builder)->Result<void>{
        auto created=Menu::create(builder,{},"menu",{},Callbacks{.opened=[payload,&opened]{++opened;}});
        if(!created)return std::unexpected(created.error());menu=*created;return menu->finalize();
    }));
    payload.reset();require(!weak.expired(),"Live menu lost its callback");
    good(menu->root().open_at(0,0));require(opened==1,"Menu open was not delivered");
    auto bad=take(menu->options());bad.configuration.item_width=0;
    require(!menu->set_options(bad),"Invalid menu input was accepted");
    good(host.clear_content());require(weak.expired(),"Retired menu retained callbacks");
    require(!menu->set_options(bad),"Retired menu accepted input");tree->close();
}
