#include <iiGUI/WidgetState.h>
#include <iiGUI/SrgbColor.h>
#include <memory>
#include <cmath>
#include <stdexcept>
#include <string>
#include <vector>
namespace {
template<class T>T take(ii::core::Result<T> value){if(!value)throw std::runtime_error(value.error().message);return std::move(*value);}
void good(ii::core::Result<void> value){if(!value)throw std::runtime_error(value.error().message);}
void check(bool vertical,int count){
    namespace gui=ii::gui;
    auto tree=std::make_shared<gui::detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{900,500}));gui::Widget group;std::vector<gui::Widget> buttons;
    gui::SegmentedOptions options;options.vertical=vertical;
    if(options.corner_radius!=8||options.corner_radius!=gui::WidgetAppearance{}.button_radius)
        throw std::runtime_error("Default segment and button radii differ");
    good(host.set_content([&](gui::WidgetBuilder& builder)->ii::core::Result<void>{
        auto root=builder.vstack({},"root");if(!root)return std::unexpected(root.error());
        auto result=builder.segmented_control(*root,"group",options);if(!result)return std::unexpected(result.error());group=*result;
        for(int i=0;i<count;++i){auto b=builder.action_surface(group,"button"+std::to_string(i),{.width=22,.height=22});if(!b)return std::unexpected(b.error());buttons.push_back(*b);}return {};
    }));
    const auto frame=take(tree->drawing_snapshot({900,500}));auto bounds=take(group.snapshot()).bounds;const auto length=count*22+(count-1)*2;
    if(bounds.width!=(vertical?22:length)||bounds.height!=(vertical?length:22))throw std::runtime_error("Invalid segmented extent: "+std::to_string(bounds.width)+"x"+std::to_string(bounds.height)+" count="+std::to_string(count)+" vertical="+std::to_string(vertical));
    for(int i=0;i<count;++i){auto b=take(buttons[i].snapshot()).bounds;
        if(b.x!=bounds.x+(vertical?0:i*24)||b.y!=bounds.y+(vertical?i*24:0))throw std::runtime_error("Invalid segment position");}
    const auto border=gui::srgb_to_linear(options.border),background=gui::srgb_to_linear(options.background);bool border_drawn=false,surface_drawn=false;
    const auto inspect=[&](const auto& panels){for(const auto& p:panels){if(p.bounds==bounds&&p.top==border)border_drawn=true;
        if(p.bounds==bounds&&p.top==background&&p.radius==8)surface_drawn=true;}};
    inspect(frame.panels);for(const auto& layer:frame.overlays)inspect(layer.panels);
    if(border_drawn)throw std::runtime_error("A border panel was drawn for a borderless segment");
    if(!surface_drawn)throw std::runtime_error("Default segment surface does not use the button radius");
    tree->close();
}
void selected_button(){
    namespace gui=ii::gui;auto appearance=gui::lvrs_appearance();appearance.primary={1,.2,.1,1};
    auto tree=std::make_shared<gui::detail::WidgetState>([](ii::runtime::Task task){task();},appearance);
    auto host=take(tree->content_host({},"host",{200,80}));gui::Widget button;unsigned clicks=0;
    good(host.set_content([&](gui::WidgetBuilder& builder)->ii::core::Result<void>{
        auto group=builder.segmented_control({},"segments");if(!group)return std::unexpected(group.error());
        auto child=builder.button(*group,"button",{.text="One",.tone=gui::ButtonTone::primary,.checkable=true},[&]{++clicks;});
        if(!child)return std::unexpected(child.error());button=*child;return {};
    }));
    (void)take(tree->drawing_snapshot({200,80}));auto before=take(button.snapshot());
    if(before.tone!=gui::ButtonTone::primary || before.checked)throw std::runtime_error("Segment changed the authored button tone");
    good(button.activate());auto after=take(button.snapshot());
    if(!after.checked || after.tone!=before.tone || clicks!=1)throw std::runtime_error("Segment did not retain its tone while toggling");
    auto frame=take(tree->drawing_snapshot({200,80}));bool found=false;
    const auto match=[&](const auto& panels){for(const auto& panel:panels)if(panel.bounds==after.bounds && panel.stroke_width==0){
        const auto channel=[](double c){return c<=.04045?c/12.92:std::pow((c+.055)/1.055,2.4);};
        const auto red=.1411764770746231*.75+appearance.primary.red*.25;
        if(std::abs(panel.top.red-channel(red))<1e-5)found=true;
    }};match(frame.panels);for(const auto& overlay:frame.overlays)match(overlay.panels);
    if(!found)throw std::runtime_error("Segment selected fill did not use the application accent");
    good(button.activate());if(take(button.snapshot()).checked)throw std::runtime_error("Segment did not toggle off");
    tree->close();
}
void explicit_padding_and_border(){
    namespace gui=ii::gui;
    auto tree=std::make_shared<gui::detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{200,80}));gui::Widget group,child;
    gui::SegmentedOptions options;options.horizontal_padding=4;options.vertical_padding=4;options.border_width=2;
    good(host.set_content([&](gui::WidgetBuilder& builder)->ii::core::Result<void>{
        auto root=builder.vstack({},"root");if(!root)return std::unexpected(root.error());
        auto value=builder.segmented_control(*root,"custom",options);if(!value)return std::unexpected(value.error());group=*value;
        auto button=builder.action_surface(group,"button",{.width=22,.height=22});if(!button)return std::unexpected(button.error());child=*button;return {};
    }));
    auto frame=take(tree->drawing_snapshot({200,80}));auto outer=take(group.snapshot()).bounds,inner=take(child.snapshot()).bounds;
    if(outer.width!=30||outer.height!=30||inner.x!=outer.x+4||inner.y!=outer.y+4)
        throw std::runtime_error("Explicit segment padding was not retained");
    const auto border=gui::srgb_to_linear(options.border);bool found=false;
    const auto inspect=[&](const auto& panels){for(const auto& p:panels)if(p.bounds==outer&&p.top==border)found=true;};
    inspect(frame.panels);for(const auto& layer:frame.overlays)inspect(layer.panels);
    if(!found)throw std::runtime_error("Explicit segment border was not retained");
    tree->close();
}
}
int main(){for(int n=2;n<=15;++n){check(false,n);check(true,n);}selected_button();explicit_padding_and_border();}
