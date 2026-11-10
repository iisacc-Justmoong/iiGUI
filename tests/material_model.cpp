#include <iiGUI/MaterialModel.h>
#include <chrono>
#include <cmath>
#include <limits>
#include <stdexcept>
namespace {void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}}
int main(){
    using namespace std::chrono_literals;ii::gui::MaterialModel material;ii::gui::MaterialConfiguration config;
    config.kind=ii::gui::MaterialKind::panel;config.motion={.duration=200ms,.curve=ii::gui::WidgetMotionCurve::linear};
    config.primary_color=ii::gui::WidgetColor{0,0,0,1};config.color=ii::gui::WidgetColor{0,0,0,1};config.tint_opacity=1;
    const auto start=ii::gui::MaterialModel::TimePoint{};require(material.set_configuration(config,start),"Initial material rejected");
    auto resolved=material.resolved();require(resolved.width==380&&resolved.height==204&&resolved.blur_radius==16,"Panel defaults changed");
    config.primary_color=ii::gui::WidgetColor{1,0,0,1};config.color=ii::gui::WidgetColor{1,1,1,1};
    require(material.set_configuration(config,start)&&material.animating(),"Material transition failed");
    require(material.advance(start+100ms)&&std::abs(material.displayed_primary().red-.5)<1e-6,"Material frame changed");
    require(!material.advance(start+200ms)&&material.displayed_primary().red==1,"Material did not settle");
    const auto before=material.configuration();auto bad=before;bad.blur_radius=65;
    require(!material.set_configuration(bad,start)&&material.configuration()==before,"Invalid material replaced state");
    bad=before;bad.outline_path="M0 0 X1 2";require(!material.set_configuration(bad,start),"Unsafe outline accepted");
    bad=before;bad.tint_opacity=std::numeric_limits<double>::quiet_NaN();require(!material.set_configuration(bad,start),"NaN accepted");
    require(material.gradient_alpha(48,32,380,204)>=0&&material.gradient_alpha(48,32,380,204)<=1,"Gradient coverage invalid");
}
