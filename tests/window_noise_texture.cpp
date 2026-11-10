#include <iiGUI/WindowNoiseTexture.h>
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace {
void require(bool value,const char* why){if(!value)throw std::runtime_error(why);}
template<class T>T take(ii::core::Result<T> value){if(!value)throw std::runtime_error(value.error().message);return std::move(*value);}
}
int main(){
    using namespace ii::gui;
    WindowNoiseOptions options{.color={.2,.4,.6,1},.primary_color={.8,.2,.1,1},.noise_strength=0,.blur_radius=0,.grain_strength=0};
    auto flat=take(window_noise_texture(options));
    require(flat->width()==256&&flat->height()==256&&flat->rgba()[0]==51&&flat->rgba()[1]==102&&flat->rgba()[2]==153&&flat->rgba()[3]==255,
            "Zero noise did not produce the specified opaque base color");
    options.noise_strength=.32;options.blur_radius=1.5;options.grain_strength=.05;
    auto first=take(window_noise_texture(options)),second=take(window_noise_texture(options));
    require(std::ranges::equal(first->rgba(),second->rgba())&&!std::ranges::equal(first->rgba(),flat->rgba()),
            "Noise was unstable or ignored its authored strength");
    auto plain=options;plain.blur_radius=0;plain.grain_strength=0;
    auto soft=plain;soft.blur_radius=3;
    auto unblurred=take(window_noise_texture(plain)),blurred=take(window_noise_texture(soft));
    const auto variation=[](const auto& image){double sum=0,sum2=0;std::size_t count=0;
        for(std::size_t n=0;n<image->rgba().size();n+=4){const auto value=double(image->rgba()[n]);sum+=value;sum2+=value*value;++count;}
        return sum2/count-(sum/count)*(sum/count);};
    require(variation(blurred)<variation(unblurred),"Blur did not reduce spatial noise variation");
    auto invalid=options;invalid.blur_radius=std::numeric_limits<double>::quiet_NaN();
    require(!window_noise_texture(invalid),"Invalid noise metrics were accepted");
    invalid=options;invalid.grain_strength=1.01;require(!window_noise_texture(invalid),"Out-of-range grain was accepted");
}
