#pragma once
#include <iiGUI/Image.h>
#include <iiGUI/Appearance.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <exception>
#include <memory>
#include <vector>

namespace ii::gui {
struct WindowNoiseOptions final {
    WidgetColor color{0,0,0,1},primary_color{0,0,0,1};
    double noise_strength=0,blur_radius=0,grain_strength=0;
    friend bool operator==(const WindowNoiseOptions&,const WindowNoiseOptions&)=default;
};

// Build a bounded, deterministic tile only when its authored colors or
// strengths change. Display and compositing belong to the iiGUI renderer.
[[nodiscard]] inline core::Result<std::shared_ptr<const RasterImage>> window_noise_texture(const WindowNoiseOptions& options){
    const auto bounded=[](double value,double high){return std::isfinite(value)&&value>=0&&value<=high;};
    for(auto c:{options.color.red,options.color.green,options.color.blue,options.color.alpha,
                options.primary_color.red,options.primary_color.green,options.primary_color.blue,options.primary_color.alpha})
        if(!bounded(c,1))return core::fail(core::ErrorCode::invalid_argument,"Invalid window-noise color");
    if(!bounded(options.noise_strength,1)||!bounded(options.blur_radius,50)||!bounded(options.grain_strength,1))
        return core::fail(core::ErrorCode::invalid_argument,"Invalid window-noise strength or blur");
    try{
        constexpr int side=256;using Pixel=std::array<float,3>;
        std::vector<Pixel> pixels(side*side),scratch(side*side);
        std::uint32_t state=0x6d2b79f5u;
        const auto random=[&]{state^=state<<13;state^=state>>17;state^=state<<5;return double(state&0xffffu)/65535.;};
        const std::array<double,3> base{options.color.red,options.color.green,options.color.blue};
        const std::array<double,3> accent{options.primary_color.red,options.primary_color.green,options.primary_color.blue};
        for(auto& pixel:pixels){const auto amount=options.noise_strength>0?options.noise_strength*(.1+.9*random()):0.;
            for(int c=0;c<3;++c)pixel[c]=float(base[c]+(accent[c]-base[c])*amount);}
        // Three periodic box passes approximate a Gaussian while each pass is
        // linear in tile size, independent of the requested blur radius.
        const int radius=std::clamp(int(std::lround(options.blur_radius)),0,50);
        if(radius&&options.noise_strength>0){
            const auto blur=[&](bool horizontal){
                const auto at=[&](int line,int offset)->std::size_t{
                    const int position=(offset+side)%side;return horizontal?std::size_t(line*side+position):std::size_t(position*side+line);
                };
                const auto count=double(2*radius+1);
                for(int line=0;line<side;++line){
                    std::array<double,3> sum{};
                    for(int k=-radius;k<=radius;++k)for(int c=0;c<3;++c)sum[c]+=pixels[at(line,k)][c];
                    for(int position=0;position<side;++position){
                        auto& out=scratch[at(line,position)];for(int c=0;c<3;++c)out[c]=float(sum[c]/count);
                        for(int c=0;c<3;++c)sum[c]+=pixels[at(line,position+radius+1)][c]-pixels[at(line,position-radius)][c];
                    }
                }
                pixels.swap(scratch);
            };
            for(int pass=0;pass<3;++pass){blur(true);blur(false);}
        }
        state=0x9e3779b9u;std::vector<std::uint8_t> rgba(side*side*4);
        for(std::size_t n=0;n<pixels.size();++n){
            const auto grain=options.grain_strength>0?(2*random()-1)*options.grain_strength:0.;
            for(int c=0;c<3;++c)rgba[n*4+c]=std::uint8_t(std::lround(std::clamp(double(pixels[n][c])+grain,0.,1.)*255));
            rgba[n*4+3]=255;
        }
        return rgba_image(side,side,std::move(rgba));
    }catch(...){return core::fail(core::ErrorCode::failed,"Window-noise preparation failed",std::current_exception());}
}
} // namespace ii::gui
