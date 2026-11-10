#include <iiGUI/NativeAppCardView.h>
#include <iiGUI/WidgetState.h>
#include <algorithm>
#include <cmath>
#include <memory>
#include <optional>
#include <stdexcept>

namespace {
void require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
template<class T>T take(ii::core::Result<T> value){if(!value)throw std::runtime_error(value.error().message);return std::move(*value);}
void good(ii::core::Result<void> value){if(!value)throw std::runtime_error(value.error().message);}
void retained_content(){
    auto tree=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});
    auto outer=take(tree->content_host({},"outer",{640,480}));ii::gui::AppCardOptions config;
    std::optional<ii::gui::NativeAppCard> card;ii::gui::Widget body;
    good(outer.set_content([&](ii::gui::WidgetBuilder& builder)->ii::core::Result<void>{
        auto created=ii::gui::NativeAppCard::create(builder,{},"card",config);if(!created)return std::unexpected(created.error());
        card=*created;auto child=builder.label(card->content_root(),"body",{.text="Persistent"});if(!child)return std::unexpected(child.error());body=*child;return {};
    }));
    auto before=take(card->root().measure(640));require(before.width>=280,"Card lost its minimum natural width");
    config.title="Title";config.subtitle="Description";config.padding=20;config.spacing=8;
    config.shape=ii::gui::CardShape::cylinder;config.background={.2,.3,.4,1};good(card->set_options(config));
    require(take(body.snapshot()).text=="Persistent"&&take(card->root().measure(640)).height>before.height,
        "Changing header or style replaced the card body");
    auto invalid=config;invalid.padding=-1;require(!card->set_options(invalid)&&take(body.snapshot()).text=="Persistent",
        "Invalid card input changed the live body");
    auto frame=take(tree->drawing_snapshot({640,480}));const auto bounds=take(card->root().snapshot()).bounds;
    const auto contains=[&](const auto& panels){return std::ranges::any_of(panels,[&](const auto& panel){return panel.bounds==bounds&&panel.top.alpha>0;});};
    require(contains(frame.panels)||std::ranges::any_of(frame.overlays,[&](const auto& layer){return contains(layer.panels);}),
        "Card did not submit its own background");
    good(outer.clear_content());require(!body.snapshot()&&!card->set_options(config),"Retired card remained usable");tree->close();
}
void reentry_and_exception(){
    auto tree=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});
    auto outer=take(tree->content_host({},"outer",{640,480}));ii::gui::AppCardOptions config;config.title="Original";
    std::optional<ii::gui::NativeAppCard> card;ii::gui::Widget button;bool throw_cancel=false,reentered=false;
    good(outer.set_content([&](auto& builder)->ii::core::Result<void>{
        auto created=ii::gui::NativeAppCard::create(builder,{},"card",config);if(!created)return std::unexpected(created.error());card=*created;
        auto child=builder.action_surface(card->content_root(),"action",{.text="Action",.minimum_width=100,.minimum_height=36},{});
        if(!child)return std::unexpected(child.error());button=*child;
        return child->set_control_events({.canceled=[&]{if(throw_cancel)throw std::runtime_error("Cancellation callback failed");
            auto latest=config;latest.title="Reentrant";good(card->set_options(latest));reentered=true;}});
    }));
    const auto press=[&]{auto bounds=take(button.snapshot()).bounds;good(tree->input({.kind=ii::gui::WidgetInputKind::pointer_down,.x=bounds.x+4,.y=bounds.y+4}));};
    auto disabled=config;disabled.enabled=false;press();good(card->set_options(disabled));
    require(reentered&&take(card->root().snapshot()).enabled,"A reentrant cancellation update was overwritten");
    throw_cancel=true;press();bool escaped=false;ii::core::Result<void> failed;
    try{failed=card->set_options(disabled);}catch(...){escaped=true;}
    require(!escaped&&!failed&&failed.error().cause,"A cancellation exception escaped the card interface");
    good(card->set_options(config));require(take(card->root().snapshot()).enabled,"Card did not recover after a callback exception");
    good(outer.clear_content());tree->close();
}
}
int main(){retained_content();reentry_and_exception();}
