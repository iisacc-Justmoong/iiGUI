#include <iiGUI/NativeFocusRingView.h>
#include "../src/Widgets/WidgetState.h"
#include <cmath>
#include <memory>
#include <optional>
#include <stdexcept>

namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(ii::core::Result<T> value){if(!value)throw std::runtime_error(value.error().message);return std::move(*value);}
void good(ii::core::Result<void> value){if(!value)throw std::runtime_error(value.error().message);}
}
int main(){
    auto tree=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{200,100}));
    std::optional<ii::gui::NativeFocusRing> ring;
    ii::gui::FocusRingOptions options{.width=100,.height=40,.radius=6,.stroke=2,.color={1,0,0,1},.active=true,.motion_enabled=false};
    good(host.set_content([&](auto& builder)->ii::core::Result<void>{
        auto result=ii::gui::NativeFocusRing::create(builder,{},"ring",options);
        if(!result)return std::unexpected(result.error());ring=std::move(*result);return {};
    }));
    auto widget=ring->root();auto drawing=take(tree->drawing_snapshot({200,100}));
    require(!drawing.panels.empty()||!drawing.overlays.empty(),"Active focus ring did not produce retained drawing");
    const auto before=take(widget.snapshot());
    auto invalid=options;invalid.stroke=-1;
    require(!ring->set_options(invalid)&&take(widget.snapshot()).size==before.size,"Invalid focus-ring update changed its widget");
    options.active=false;good(ring->set_options(options));
    auto hidden=take(tree->drawing_snapshot({200,100}));
    require(hidden.panels.empty()&&hidden.overlays.empty(),"Inactive focus ring retained visual ink");
    good(host.clear_content());
    require(!ring->set_options(options),"Retired focus ring accepted a late update");
    tree->close();
}
