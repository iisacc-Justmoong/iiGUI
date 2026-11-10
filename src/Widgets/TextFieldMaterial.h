#pragma once
#include <iiGUI/Image.h>
#include <iiGUI/NativeWidgets.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <exception>
#include <memory>
#include <optional>
#include <vector>
namespace ii::gui {
template<class Policy>
class BasicTextFieldMaterial final {
    using StyleColor=typename Policy::Color;using Variant=typename Policy::Variant;
    struct Key final {
        double width,height,scale,radius;
        bool enabled;
        Variant variant;
        friend bool operator==(const Key&,const Key&)=default;
    };
    struct Pixel final { double r=0,g=0,b=0,a=0; };
    std::optional<Key> key_;
    std::shared_ptr<const ii::gui::RasterImage> image_;

    // The supplied paint and shadow layers are combined in encoded
    // sRGB. iiGUI receives the resulting straight-alpha image and handles its
    // later composition with the captured backdrop in linear light.
    static void over(Pixel& dst,StyleColor src,double coverage=1)noexcept{
        const auto a=std::clamp(src.alpha*coverage,0.,1.),remaining=1-a;
        dst={src.red*a+dst.r*remaining,src.green*a+dst.g*remaining,
             src.blue*a+dst.b*remaining,a+dst.a*remaining};
    }
    template<std::size_t N,class Stop>static StyleColor gradient(const Stop (&stops)[N],double t)noexcept{
        t=std::clamp(t,0.,1.);std::size_t next=1;
        while(next+1<N&&stops[next].position<t)++next;
        const auto& a=stops[next-1];const auto& b=stops[next];
        const auto p=std::clamp((t-a.position)/(b.position-a.position),0.,1.);
        const auto mix=[p](double x,double y){return x+(y-x)*p;};
        return {mix(a.color.red,b.color.red),mix(a.color.green,b.color.green),
                mix(a.color.blue,b.color.blue),mix(a.color.alpha,b.color.alpha)};
    }
    static double distance(double x,double y,double w,double h,double radius)noexcept{
        const auto qx=std::abs(x-w/2)-(w/2-radius),qy=std::abs(y-h/2)-(h/2-radius);
        return std::hypot((std::max)(qx,0.),(std::max)(qy,0.))+(std::min)((std::max)(qx,qy),0.)-radius;
    }
    [[nodiscard]] ii::core::Result<std::shared_ptr<const ii::gui::RasterImage>> raster(const Key& key){
        if(key_&&*key_==key)return image_;
        const auto w=(std::max)(1u,static_cast<unsigned>(std::ceil(key.width*key.scale)));
        const auto h=(std::max)(1u,static_cast<unsigned>(std::ceil(key.height*key.scale)));
        const auto radius=(std::min)({key.radius,key.width/2,key.height/2});
        const auto border=(std::min)({Policy::border_width,key.width/2,key.height/2});
        std::vector<std::uint8_t> rgba(std::size_t(w)*h*4);
        for(unsigned y=0;y<h;++y)for(unsigned x=0;x<w;++x){
            const auto px=(x+.5)*key.width/w,py=(y+.5)*key.height/h;
            const auto outside=distance(px,py,key.width,key.height,radius);
            const auto coverage=std::clamp(.5-outside*key.scale,0.,1.);
            if(coverage<=0)continue;
            Pixel pixel;
            if(key.variant==Variant::inline_)over(pixel,gradient(Policy::inline_fill_gradient,py/key.height));
            else if(key.enabled)over(pixel,gradient(Policy::fill_gradient,py/key.height));
            else over(pixel,gradient(Policy::disabled_fill_gradient,py/key.height));
            over(pixel,Policy::reflection);
            if(key.variant==Variant::inline_)
                over(pixel,key.enabled?Policy::inline_background:Policy::inline_disabled_background);
            else over(pixel,key.enabled?Policy::background:Policy::disabled_background);
            if(key.enabled){
                const auto apply_shadows=[&](const auto& shadows){for(const auto& shadow:shadows){
                        const auto edge=distance(px-shadow.offset_x,py-shadow.offset_y,key.width,key.height,radius);
                        const auto sigma=shadow.radius/3;
                        over(pixel,shadow.color,.5*std::erfc(-edge/(sigma*std::sqrt(2.))));
                    }};
                if(key.variant==Variant::inline_)apply_shadows(Policy::inline_inner_shadows);
                else{
                    apply_shadows(Policy::inner_shadows);
                    const auto inside=std::clamp(.5-distance(px-border,py-border,key.width-2*border,key.height-2*border,
                        (std::max)(0.,radius-border))*key.scale,0.,1.);
                    const auto ring=std::clamp((coverage-inside)/coverage,0.,1.);
                    over(pixel,gradient(Policy::stroke_gradient,py/key.height),ring);
                }
            }
            const auto i=(std::size_t(y)*w+x)*4;
            const auto byte=[](double value){return static_cast<std::uint8_t>(std::lround(std::clamp(value,0.,1.)*255));};
            if(pixel.a>0){rgba[i]=byte(pixel.r/pixel.a);rgba[i+1]=byte(pixel.g/pixel.a);rgba[i+2]=byte(pixel.b/pixel.a);}
            rgba[i+3]=byte(pixel.a*coverage);
        }
        auto image=ii::gui::rgba_image(w,h,std::move(rgba));if(!image)return std::unexpected(image.error());
        key_=key;image_=*image;return image_;
    }
public:
    [[nodiscard]] ii::core::Result<std::shared_ptr<const ii::gui::WidgetDrawing>> paint(
            const ii::gui::WidgetPaintState& state,bool configured_enabled,double radius=Policy::radius,
            Variant variant=Variant::standard){
        try{
            const auto width=state.size.width,height=state.size.height,scale=state.backing_scale;
            if(!std::isfinite(width)||!std::isfinite(height)||width<0||height<0||width>8192||height>8192||
               !std::isfinite(scale)||scale<=0||!std::isfinite(radius)||radius<0||radius>8192||
               (variant!=Variant::standard&&variant!=Variant::inline_)||(variant==Variant::inline_&&radius!=0))
                return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid text field material geometry");
            auto drawing=std::make_shared<ii::gui::WidgetDrawing>();
            if(width>0&&height>0){
                const auto raster_scale=(std::min)({(std::max)(scale,.125),2048./width,2048./height,
                    std::sqrt(1048576./(width*height))});
                auto image=raster({width,height,raster_scale,radius,state.enabled&&configured_enabled,variant});
                if(!image)return std::unexpected(image.error());
                const ii::gui::Rect bounds{0,0,float(width),float(height)};
                if(state.enabled&&configured_enabled)
                    drawing->backdrop=ii::gui::BackdropMaterial{.bounds=bounds,
                        .radius=float((std::min)({radius,width/2,height/2})),
                        .blur_radius=float(variant==Variant::inline_?Policy::inline_blur_radius:
                            Policy::blur_radius),.source=ii::gui::BackdropSource::previous_layers};
                ii::gui::ImageRun ink;ink.bounds=bounds;ink.image=*image;drawing->images.push_back(std::move(ink));
            }
            return std::shared_ptr<const ii::gui::WidgetDrawing>(std::move(drawing));
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Text field material rendering failed",std::current_exception());}
    }
};
} // namespace ii::gui
