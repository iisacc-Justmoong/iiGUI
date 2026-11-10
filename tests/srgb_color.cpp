#include <iiGUI/SrgbColor.h>
#include <cmath>
#include <stdexcept>
struct Encoded {double red,green,blue,alpha;};
int main(){
    const auto color=ii::gui::srgb_to_linear(Encoded{.04045,.5,1,.4});
    if(std::abs(color.red-.04045/12.92)>1e-6||std::abs(color.green-.21404114)>1e-6||
       std::abs(color.blue-1)>1e-6||std::abs(color.alpha-.4)>1e-6)
        throw std::runtime_error("sRGB conversion changed transfer function or alpha");
    const auto converted=ii::gui::SrgbColorConverter::convert(Encoded{.5,.5,.5,1});
    if(std::abs(converted.red-.21404114)>1e-6)throw std::runtime_error("generic converter");
    const auto black=ii::gui::srgb_to_linear(Encoded{0,0,0,0});
    if(black.red!=0||black.green!=0||black.blue!=0||black.alpha!=0)throw std::runtime_error("black conversion");
}
