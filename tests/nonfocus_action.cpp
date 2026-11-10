#include "../src/Widgets/WidgetState.h"
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(ii::core::Result<T> value){if(!value)throw std::runtime_error(value.error().message);return std::move(*value);}
void good(ii::core::Result<void> value){if(!value)throw std::runtime_error(value.error().message);}
void nested_escape(){
    using namespace ii::gui;
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    auto outer=take(tree->content_host({},"outer",{320,100}));Widget inner,button,parent;
    std::vector<std::string> events;
    good(outer.set_content([&](auto& builder)->ii::core::Result<void>{
        parent=take(builder.absolute_stack({},"parent",{320,100}));
        inner=take(builder.content_host(parent,"inner",{320,100}));
        return parent.set_escape_action([&]{events.emplace_back("parent escape");});
    }));
    good(inner.set_content([&](auto& builder)->ii::core::Result<void>{
        auto root=take(builder.absolute_stack({},"root",{320,100}));
        button=take(builder.action_surface(root,"button",{.accessibility_name="Button",.width=60,.height=30},
            [&]{events.emplace_back("clicked");}));
        good(button.set_frame({20,20,60,30}));
        return button.set_control_events({.pressed=[&]{events.emplace_back("pressed");},
            .released=[&]{events.emplace_back("released");},.canceled=[&]{events.emplace_back("canceled");}});
    }));
    (void)take(tree->drawing_snapshot({320,100}));good(button.focus());
    good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::space}));
    good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::escape}));
    good(tree->input({.kind=WidgetInputKind::key_up,.key=WidgetKey::space}));
    require(events==std::vector<std::string>{"pressed","canceled"},"Escape failed to cancel a nested press before the parent handler");
    good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::escape}));
    require(events==std::vector<std::string>{"pressed","canceled","parent escape"},"Unclaimed Escape failed to reach the parent handler");
    events.clear();good(tree->input({.kind=WidgetInputKind::pointer_down,.x=30,.y=30}));
    good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::escape}));
    good(tree->input({.kind=WidgetInputKind::pointer_up,.x=30,.y=30}));
    require(events==std::vector<std::string>{"pressed","canceled"},"Escape failed to cancel a nested pointer press");
    tree->close();
}
void popup_escape(){
    using namespace ii::gui;
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"popup-host",{320,200}));Widget popup,button;
    std::vector<std::string> events;
    good(host.set_content([&](auto& builder)->ii::core::Result<void>{
        auto root=take(builder.absolute_stack({},"root",{320,200}));
        popup=take(builder.popover(root,"popup",{.width=150,.height=100}));
        return {};
    }));
    good(popup.set_popup_content([&](auto& builder)->ii::core::Result<void>{
        auto root=take(builder.vstack({},"body"));
        button=take(builder.action_surface(root,"button",{.accessibility_name="Button",.width=60,.height=30},
            [&]{events.emplace_back("clicked");}));
        return button.set_control_events({.pressed=[&]{events.emplace_back("pressed");},
            .released=[&]{events.emplace_back("released");},.canceled=[&]{events.emplace_back("canceled");}});
    }));
    good(popup.open_at(30,30));(void)take(tree->drawing_snapshot({320,200}));good(button.focus());
    good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::space}));
    good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::escape}));
    good(tree->input({.kind=WidgetInputKind::key_up,.key=WidgetKey::space}));
    require(events==std::vector<std::string>{"pressed","canceled"}&&take(popup.snapshot()).opened,
        "Escape did not cancel the popup control before dismissal");
    good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::escape}));
    require(!take(popup.snapshot()).opened,"Unclaimed Escape did not dismiss the popup");
    tree->close();
}
void enter_policy(){
    using namespace ii::gui;
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"enter-host",{200,100}));Widget button;
    std::vector<std::string> events;
    good(host.set_content([&](auto& builder)->ii::core::Result<void>{
        auto root=take(builder.absolute_stack({},"root",{200,100}));
        button=take(builder.action_surface(root,"button",{.accessibility_name="Button",.width=60,.height=30},
            [&]{events.emplace_back("clicked");}));
        good(button.set_frame({20,20,60,30}));
        return button.set_control_events({.pressed=[&]{events.emplace_back("pressed");},
            .released=[&]{events.emplace_back("released");},.canceled=[&]{events.emplace_back("canceled");}});
    }));
    (void)take(tree->drawing_snapshot({200,100}));good(button.focus());
    const auto key=[&](WidgetInputKind kind,WidgetKey value){good(tree->input({.kind=kind,.key=value}));};
    key(WidgetInputKind::key_down,WidgetKey::enter);key(WidgetInputKind::key_down,WidgetKey::enter);
    key(WidgetInputKind::key_up,WidgetKey::space);
    require(events==std::vector<std::string>{"pressed"}&&take(button.snapshot()).pressed,
            "A different key or repeated press released an Enter gesture");
    key(WidgetInputKind::key_up,WidgetKey::enter);
    require(events==std::vector<std::string>{"pressed","released","clicked"},"Enter release did not activate once");
    events.clear();auto options=take(button.action_options());options.enter_key_activation=false;good(button.set_action_options(options));
    key(WidgetInputKind::key_down,WidgetKey::enter);key(WidgetInputKind::key_up,WidgetKey::enter);
    require(events.empty()&&!take(button.snapshot()).pressed,"Disabled Enter activated the action");
    key(WidgetInputKind::key_down,WidgetKey::space);key(WidgetInputKind::key_up,WidgetKey::space);
    require(events==std::vector<std::string>{"pressed","released","clicked"},"Enter policy disabled Space");
    events.clear();good(button.activate());require(events==std::vector<std::string>{"clicked"},"Enter policy disabled programmatic activation");
    options.enter_key_activation=true;good(button.set_action_options(options));events.clear();
    key(WidgetInputKind::key_down,WidgetKey::enter);options.enter_key_activation=false;good(button.set_action_options(options));
    key(WidgetInputKind::key_up,WidgetKey::enter);
    require(events==std::vector<std::string>{"pressed","canceled"}&&!take(button.snapshot()).pressed,
            "Disabling Enter mid-press did not cancel it");
    events.clear();good(button.set_control_events({.pressed=[&]{events.emplace_back("pressed");},
        .released=[&]{events.emplace_back("released");},.canceled=[&]{
            events.emplace_back("canceled");auto reentrant=take(button.action_options());reentrant.enter_key_activation=true;
            good(button.set_action_options(reentrant));}}));
    options.enter_key_activation=true;good(button.set_action_options(options));
    key(WidgetInputKind::key_down,WidgetKey::enter);options.enter_key_activation=false;good(button.set_action_options(options));
    key(WidgetInputKind::key_up,WidgetKey::enter);
    require(events==std::vector<std::string>{"pressed","canceled"}&&take(button.action_options()).enter_key_activation,
            "Reentrant cancellation lost the latest policy or delivered a stale click");
    events.clear();good(tree->input({.kind=WidgetInputKind::pointer_down,.x=30,.y=30}));
    good(tree->input({.kind=WidgetInputKind::pointer_up,.x=30,.y=30}));
    require(events==std::vector<std::string>{"pressed","released","clicked"},"Enter policy disabled pointer activation");
    good(host.clear_content());require(!button.set_action_options(options),"Retired action accepted Enter policy");tree->close();
}
}
int main(){
    nested_escape();
    popup_escape();
    enter_policy();
    using namespace ii::gui;
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{320,100}));
    Widget input,other,action;unsigned activations=0,presses=0,releases=0;
    good(host.set_content([&](auto& builder)->ii::core::Result<void>{
        auto root=builder.absolute_stack({},"root",{320,100});if(!root)return std::unexpected(root.error());
        auto first=builder.input_field(*root,"input",{.text="query"});if(!first)return std::unexpected(first.error());input=*first;
        auto second=builder.action_surface(*root,"other",{.accessibility_name="Other",.width=40,.height=22});
        if(!second)return std::unexpected(second.error());other=*second;
        auto button=builder.action_surface(*root,"clear",{.accessibility_name="Clear",.width=22,.height=22,.pointer_focusable=false},[&]{++activations;});
        if(!button)return std::unexpected(button.error());action=*button;
        if(auto r=input.set_frame({0,0,120,22});!r)return r;
        if(auto r=action.set_frame({140,0,22,22});!r)return r;
        if(auto r=other.set_frame({180,0,40,22});!r)return r;
        return action.set_control_events({.pressed=[&]{++presses;},.released=[&]{++releases;}});
    }));
    (void)take(tree->drawing_snapshot({320,100}));
    const auto click=[&](WidgetInputKind kind,double x=150,double y=10){good(tree->input({.kind=kind,.x=x,.y=y,.buttons=kind==WidgetInputKind::pointer_down?1u:0u}));};
    click(WidgetInputKind::pointer_down);click(WidgetInputKind::pointer_up);
    require(activations==1&&presses==1&&releases==1&&!take(action.snapshot()).focused,
            "A nonfocusable action did not activate with no focused widget");
    good(input.focus());click(WidgetInputKind::pointer_down);click(WidgetInputKind::pointer_up);
    require(activations==2&&presses==2&&releases==2&&take(input.snapshot()).focused&&!take(action.snapshot()).focused,
            "A nonfocusable action stole focus or failed to activate");
    click(WidgetInputKind::pointer_down);click(WidgetInputKind::pointer_up,280,70);
    require(activations==2&&take(input.snapshot()).focused,"Release outside activated the action");
    click(WidgetInputKind::pointer_down);good(other.focus());click(WidgetInputKind::pointer_up);
    require(activations==2&&take(other.snapshot()).focused,"Focus changed during a press but the old action activated");
    good(input.focus());click(WidgetInputKind::pointer_down);good(other.focus());good(input.focus());click(WidgetInputKind::pointer_up);
    require(activations==2&&take(input.snapshot()).focused,"Changing and restoring focus reactivated an old press");
    click(WidgetInputKind::pointer_down);good(action.focus());click(WidgetInputKind::pointer_up);
    require(activations==2&&take(action.snapshot()).focused,"Focusing the action during a press reactivated it");
    good(input.focus());
    good(action.set_control_events({.released=[&]{good(other.focus());}}));
    click(WidgetInputKind::pointer_down);click(WidgetInputKind::pointer_up);
    require(activations==2&&take(other.snapshot()).focused,"A release callback changed focus but the action still activated");
    good(action.set_enabled(false));click(WidgetInputKind::pointer_down);click(WidgetInputKind::pointer_up);
    require(activations==2,"A disabled action was activated");
    tree->close();
}
