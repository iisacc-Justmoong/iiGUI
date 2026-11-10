#include <iiGUI/NativeAvatarView.h>
#include <iiGUI/WidgetState.h>
#include <cmath>
#include <limits>
#include <optional>
#include <stdexcept>
#include <vector>
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(ii::core::Result<T> result){if(!result)throw std::runtime_error(result.error().message);return std::move(*result);}
void good(ii::core::Result<void> result){if(!result)throw std::runtime_error(result.error().message);}
}
int main(){
    require(take(ii::gui::avatar_placeholder_pixels({80,50},3))==75,
            "Avatar placeholder ignored allocation or backing scale");
    require(take(ii::gui::avatar_placeholder_pixels({8192,8192},10))==512 &&
            !ii::gui::avatar_placeholder_pixels({40,40},0),
            "Avatar raster bounds were not enforced");
    require(!ii::gui::make_avatar_placeholder_source({}),"An empty Avatar decoder was accepted");
    unsigned decodes=0;auto owner=std::make_shared<int>(1);std::weak_ptr<int> weak=owner;
    auto prepared=ii::gui::make_avatar_placeholder_source([owner,&decodes](std::uint32_t pixels)
            ->ii::core::Result<std::shared_ptr<const ii::gui::RasterImage>> {
        ++decodes;return ii::gui::rgba_image(pixels,pixels,std::vector<std::uint8_t>(pixels*pixels*4,255));
    });
    owner.reset();require(prepared&&!weak.expired(),"The Avatar decoder lifetime was not retained");
    auto sampled=take((*prepared)({80,50},3));
    require(sampled&&sampled->width()==75&&sampled->height()==75&&decodes==1,"Avatar decoder received the wrong sample size");
    require(!take((*prepared)({0,50},2))&&decodes==1&&!(*prepared)({40,40},0)&&decodes==1,
            "Empty or invalid Avatar geometry invoked the decoder");
    prepared.reset();require(weak.expired(),"The Avatar decoder outlived its source");
    auto failed=ii::gui::make_avatar_placeholder_source([](std::uint32_t)->ii::core::Result<std::shared_ptr<const ii::gui::RasterImage>> {
        return ii::core::fail(ii::core::ErrorCode::failed,"decoder failure");
    });
    auto failure=(*failed)({40,40},2);
    require(!failure&&failure.error().code==ii::core::ErrorCode::failed,"Avatar decoder failure was not propagated");
    auto empty=ii::gui::make_avatar_placeholder_source([](std::uint32_t)->ii::core::Result<std::shared_ptr<const ii::gui::RasterImage>> {
        return std::shared_ptr<const ii::gui::RasterImage>{};
    });
    require(!(*empty)({40,40},2),"An empty decoded Avatar image was accepted");
    auto wrong_size=ii::gui::make_avatar_placeholder_source([](std::uint32_t)->ii::core::Result<std::shared_ptr<const ii::gui::RasterImage>> {
        return ii::gui::rgba_image(2,2,std::vector<std::uint8_t>(16,255));
    });
    require(!(*wrong_size)({40,40},2),"An Avatar image with the wrong extent was accepted");
    auto throwing=ii::gui::make_avatar_placeholder_source([](std::uint32_t)->ii::core::Result<std::shared_ptr<const ii::gui::RasterImage>> {
        throw std::runtime_error("decoder threw");
    });
    auto thrown=(*throwing)({40,40},2);
    require(!thrown&&thrown.error().code==ii::core::ErrorCode::failed,"An Avatar decoder exception escaped the result contract");
    auto state=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(state->content_host({},"host",{200,200}));
    auto mask=take(ii::gui::rgba_image(2,2,std::vector<std::uint8_t>(16,255)));
    unsigned requests=0;
    auto source=std::make_shared<const ii::gui::ImageRasterSource>([&](ii::gui::Size,double)->ii::core::Result<std::shared_ptr<const ii::gui::RasterImage>>{++requests;return mask;});
    ii::gui::AvatarInput input;
    input.placeholder_source=source;
    input.placeholder_color={.25,.5,.75,1};
    input.configuration.width=input.configuration.height=40;
    std::optional<ii::gui::NativeAvatar> avatar;
    good(host.set_content([&](auto& builder)->ii::core::Result<void>{
        auto stack=builder.vstack({},"layout",{.spacing=0,.alignment=ii::gui::StackAlignment::leading});
        if(!stack)return std::unexpected(stack.error());
        auto result=ii::gui::NativeAvatar::create(builder,*stack,"avatar",input);
        if(!result)return std::unexpected(result.error());avatar=*result;return {};
    }));
    auto frame=take(state->drawing_snapshot({200,200}));auto images=frame.images;
    for(const auto& layer:frame.overlays)images.insert(images.end(),layer.images.begin(),layer.images.end());
    require(images.size()==1&&requests==1&&images[0].tint&&std::abs(images[0].bounds.width-20)<1e-5,
            "Supplied placeholder was not drawn with the avatar geometry");
    auto invalid=input;invalid.placeholder_color.alpha=std::numeric_limits<double>::quiet_NaN();
    require(!avatar->set_options(invalid),"Invalid placeholder color was accepted");
    good(host.clear_content());
    require(!avatar->set_options(input),"Retired avatar accepted an update");
    state->close();
}
