#pragma once
#include <iiGUI/Image.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <exception>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace ii::gui {
// Rasterization and cache ownership are independent of the authored material policy.
template<class Policy>
class BasicMaterialGradient final {
    using Material=typename Policy::Material;
    struct Key final {
        double width,height,scale,padding_x,padding_y,intense,faint,accent_alpha;
        std::uint64_t source_revision;
        friend bool operator==(const Key&,const Key&)=default;
    };
    struct ColorKey final {
        Key geometry;
        double red,green,blue;
        friend bool operator==(const ColorKey&,const ColorKey&)=default;
    };
    struct BasisPixel final {std::uint16_t primary,edge,alpha;};
    std::optional<Key> key_;
    std::shared_ptr<const ii::gui::RasterImage> image_;
    std::optional<Key> basis_key_;
    std::optional<ColorKey> color_key_;
    std::vector<BasisPixel> basis_;
    std::shared_ptr<const ii::gui::RasterImage> color_image_;
public:
    [[nodiscard]] static double raster_scale(double width,double height,double backing_scale)noexcept {
        return (std::min)({(std::max)(backing_scale,0.125),2048./width,2048./height,std::sqrt(1048576./(width*height))});
    }
    [[nodiscard]] static std::pair<double,double> blur_padding(double width,double height,double blur)noexcept {
        if(!std::isfinite(width)||!std::isfinite(height)||!std::isfinite(blur)||width<=0||height<=0||width>8192||height>8192||blur<=0)
            return {0,0};
        const auto requested=(std::min)(64.,(std::max)(2.,std::ceil(blur)));
        return {(std::min)(requested,(8192.-width)/2),(std::min)(requested,(8192.-height)/2)};
    }
    [[nodiscard]] ii::core::Result<std::shared_ptr<const ii::gui::RasterImage>> render(
            const Material& material,double width,double height,double backing_scale,double padding_x=0,double padding_y=0) {
        if(!std::isfinite(width)||!std::isfinite(height)||width<=0||height<=0||width>8192||height>8192||
           !std::isfinite(backing_scale)||backing_scale<=0||!std::isfinite(padding_x)||!std::isfinite(padding_y)||
           padding_x<0||padding_y<0||padding_x>64||padding_y>64||width+2*padding_x>8192||height+2*padding_y>8192)
            return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid material gradient extent or scale");
        try {
            const auto style=material.resolved();
            const auto alpha=material.displayed_primary().alpha;
            if((style.intense_opacity==0&&style.faint_opacity==0)||alpha==0)
                return std::shared_ptr<const ii::gui::RasterImage>{};
            const auto full_width=width+2*padding_x,full_height=height+2*padding_y;
            const auto scale=raster_scale(full_width,full_height,backing_scale);
            const Key key{width,height,scale,padding_x,padding_y,style.intense_opacity,style.faint_opacity,alpha,Policy::geometry_revision(material)};
            if(key_&&*key_==key)return image_;
            const auto w=(std::max)(1u,static_cast<unsigned>(std::ceil(full_width*scale)));
            const auto h=(std::max)(1u,static_cast<unsigned>(std::ceil(full_height*scale)));
            std::vector<std::uint8_t> pixels(std::size_t(w)*h*4);
            for(unsigned y=0;y<h;++y)for(unsigned x=0;x<w;++x){
                const auto offset=(std::size_t(y)*w+x)*4;
                pixels[offset]=pixels[offset+1]=pixels[offset+2]=255;
                pixels[offset+3]=static_cast<std::uint8_t>(std::lround(255*material.gradient_alpha(
                    (x+.5)*full_width/w-padding_x,(y+.5)*full_height/h-padding_y,width,height)));
            }
            auto image=ii::gui::rgba_image(w,h,std::move(pixels));if(!image)return std::unexpected(image.error());
            key_=key;image_=*image;return image_;
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Cannot rasterize material gradient",std::current_exception());}
    }
    // Preserve render()'s monochrome-mask contract for existing consumers.
    // Colored output caches the geometry separately so an accent RGB change
    // only recolors the bounded raster rather than recalculating eight fields.
    [[nodiscard]] ii::core::Result<std::shared_ptr<const ii::gui::RasterImage>> render_colored(
            const Material& material,double width,double height,double backing_scale,double padding_x=0,double padding_y=0) {
        if(!std::isfinite(width)||!std::isfinite(height)||width<=0||height<=0||width>8192||height>8192||
           !std::isfinite(backing_scale)||backing_scale<=0||!std::isfinite(padding_x)||!std::isfinite(padding_y)||
           padding_x<0||padding_y<0||padding_x>64||padding_y>64||width+2*padding_x>8192||height+2*padding_y>8192)
            return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid material gradient extent or scale");
        try {
            const auto style=material.resolved();const auto accent=material.displayed_primary();
            if((style.intense_opacity==0&&style.faint_opacity==0)||accent.alpha==0)
                return std::shared_ptr<const ii::gui::RasterImage>{};
            const auto full_width=width+2*padding_x,full_height=height+2*padding_y;
            const auto scale=raster_scale(full_width,full_height,backing_scale);
            const Key geometry{width,height,scale,padding_x,padding_y,style.intense_opacity,style.faint_opacity,accent.alpha,Policy::geometry_revision(material)};
            const ColorKey color{geometry,accent.red,accent.green,accent.blue};
            if(color_key_&&*color_key_==color)return color_image_;
            const auto w=(std::max)(1u,static_cast<unsigned>(std::ceil(full_width*scale)));
            const auto h=(std::max)(1u,static_cast<unsigned>(std::ceil(full_height*scale)));
            if(!basis_key_||*basis_key_!=geometry){
                std::vector<BasisPixel> next(std::size_t(w)*h);
                const auto quantize=[](double v){return static_cast<std::uint16_t>(std::lround(65535*std::clamp(v,0.,1.)));};
                for(unsigned y=0;y<h;++y)for(unsigned x=0;x<w;++x){
                    const auto weights=material.gradient_weights((x+.5)*full_width/w-padding_x,
                        (y+.5)*full_height/h-padding_y,width,height);
                    next[std::size_t(y)*w+x]={quantize(weights.primary),quantize(weights.edge),quantize(weights.alpha)};
                }
                basis_=std::move(next);basis_key_=geometry;
            }
            std::vector<std::uint8_t> pixels(std::size_t(w)*h*4);
            const std::array<double,3> primary{accent.red,accent.green,accent.blue};
            for(std::size_t i=0;i<basis_.size();++i){
                const auto b=basis_[i];if(!b.alpha)continue;
                for(std::size_t channel=0;channel<3;++channel){
                    const auto encoded=(primary[channel]*b.primary+Policy::edge_rgb[channel]*b.edge)/b.alpha;
                    pixels[i*4+channel]=static_cast<std::uint8_t>(std::lround(255*std::clamp(encoded,0.,1.)));
                }
                pixels[i*4+3]=static_cast<std::uint8_t>(std::lround(255.*b.alpha/65535.));
            }
            auto image=ii::gui::rgba_image(w,h,std::move(pixels));if(!image)return std::unexpected(image.error());
            color_key_=color;color_image_=*image;return color_image_;
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Cannot rasterize colored material gradient",std::current_exception());}
    }
};
} // namespace ii::gui
