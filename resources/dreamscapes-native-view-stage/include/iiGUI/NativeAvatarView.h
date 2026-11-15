#pragma once
#include <iiGUI/AvatarModel.h>
#include <iiGUI/NativeImageView.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <exception>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <string_view>
#include <thread>
#include <utility>

namespace ii::gui {
// Asset providers and theme values are supplied by the embedding application.
struct AvatarInput final {
    std::shared_ptr<const RasterImage> image;
    AvatarConfiguration configuration;
    WidgetColor placeholder_color{1,1,1,1};
    std::shared_ptr<const ImageRasterSource> placeholder_source;
    friend bool operator==(const AvatarInput&,const AvatarInput&)=default;
};
inline AvatarSize avatar_size(AvatarSize value) noexcept { return value; }
inline WidgetColor avatar_color(WidgetColor value) noexcept { return value; }

[[nodiscard]] inline ii::core::Result<std::uint32_t>
avatar_placeholder_pixels(ii::gui::Size extent,double backing_scale){
    if(!std::isfinite(extent.width)||!std::isfinite(extent.height)||extent.width<0||extent.height<0||
       extent.width>32767||extent.height>32767||!std::isfinite(backing_scale)||backing_scale<=0)
        return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid avatar raster request");
    const auto side=(std::min)(double(extent.width),double(extent.height));
    if(side==0)return std::uint32_t{0};
    const auto sample=(std::min)(512.,std::ceil(side*.5*(std::max)(2.,backing_scale)));
    return static_cast<std::uint32_t>((std::max)(1.,sample));
}

// The application owns the underlying asset and decoder. The framework owns
// extent/scale sampling and does not request a raster for an empty extent.
using AvatarRasterDecoder=std::function<ii::core::Result<std::shared_ptr<const RasterImage>>(std::uint32_t)>;
[[nodiscard]] inline std::shared_ptr<const ImageRasterSource>
make_avatar_placeholder_source(AvatarRasterDecoder decoder){
    if(!decoder)return {};
    return std::make_shared<const ImageRasterSource>([decoder=std::move(decoder)](Size extent,double scale)
            ->ii::core::Result<std::shared_ptr<const RasterImage>> {
        auto pixels=avatar_placeholder_pixels(extent,scale);
        if(!pixels)return std::unexpected(pixels.error());
        if(!*pixels)return std::shared_ptr<const RasterImage>{};
        try {
            auto result=decoder(*pixels);
            if(!result)return std::unexpected(result.error());
            if(!*result||(*result)->width()!=*pixels||(*result)->height()!=*pixels)
                return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid Avatar raster");
            return *result;
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Avatar raster decoding failed",std::current_exception());}
    });
}

class NativeAvatar final {
    struct State final {
        NativeImage image;
        const std::thread::id owner=std::this_thread::get_id();
        std::uint64_t revision=0;
        explicit State(NativeImage value):image(std::move(value)){}
    };
    std::shared_ptr<State> state_;
    explicit NativeAvatar(NativeImage image):state_(std::make_shared<State>(std::move(image))){}
    [[nodiscard]] static ii::core::Result<ImageInput> prepare(const AvatarInput& input){
        Avatar model;if(!model.set_configuration(input.configuration))
            return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid Avatar configuration");
        if((input.placeholder_source && !*input.placeholder_source) || !valid_image_tint(input.placeholder_color))
            return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid Avatar placeholder color");
        auto configuration=model.image_configuration();
        auto pixels=input.image;
        std::optional<double> relative_padding;
        ImageRasterSource raster_source;
        if(!pixels){
            // The Avatar variants center the original user silhouette at
            // half the circle's diameter. Keep the circle and the hit extent
            // independent of the size of the rasterized placeholder.
            if(input.placeholder_source) raster_source=*input.placeholder_source;
            relative_padding=.25;
            configuration.fit=ImageFit::contain;
        }
        std::optional<ImageSize> size;if(pixels)size=ImageSize{double(pixels->width()),double(pixels->height())};
        if(!Image::validate(configuration,size))return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid Avatar image");
        ImageInput result{std::move(pixels),std::move(configuration),relative_padding,std::move(raster_source)};
        if(!input.image)result.tint=input.placeholder_color;
        return result;
    }
public:
    [[nodiscard]] static ii::core::Result<NativeAvatar> create(ii::gui::WidgetBuilder& builder,ii::gui::Widget parent,
            std::string_view id,const AvatarInput& input={}){
        try {
        auto prepared=prepare(input);if(!prepared)return std::unexpected(prepared.error());
        auto image=NativeImage::create(builder,parent,id,*prepared);if(!image)return std::unexpected(image.error());
        if(auto enabled=image->root().set_enabled(input.configuration.enabled);!enabled)return std::unexpected(enabled.error());
        return NativeAvatar(std::move(*image));
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Avatar construction failed",std::current_exception());}
    }
    [[nodiscard]] ii::gui::Widget root()const{return state_->image.root();}
    [[nodiscard]] ii::core::Result<void> set_options(const AvatarInput& input)const {
        try {
        if(state_->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"Avatar requires its owner thread");
        auto prepared=prepare(input);if(!prepared)return std::unexpected(prepared.error());
        if(state_->revision==(std::numeric_limits<std::uint64_t>::max)())return ii::core::fail(ii::core::ErrorCode::out_of_range,"Avatar revision exhausted");
        const auto revision=++state_->revision;
        if(auto changed=state_->image.set_options(*prepared);!changed)return changed;
        if(state_->revision!=revision)return {};
        return state_->image.root().set_enabled(input.configuration.enabled);
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Avatar update failed",std::current_exception());}
    }
};
} // namespace ii::gui
