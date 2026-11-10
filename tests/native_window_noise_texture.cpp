#include <iiGUI/NativeWindowNoiseTextureView.h>
#include "../src/Widgets/WidgetState.h"
#include <memory>
#include <optional>
#include <stdexcept>

namespace {
void require(bool value,const char* why){if(!value)throw std::runtime_error(why);}
template<class T>T take(ii::core::Result<T> value){if(!value)throw std::runtime_error(value.error().message);return std::move(*value);}
void good(ii::core::Result<void> value){if(!value)throw std::runtime_error(value.error().message);}
std::shared_ptr<const ii::gui::RasterImage> image(const ii::gui::CanvasFrame& frame){
    if(!frame.images.empty())return frame.images.front().image;
    for(const auto& layer:frame.overlays)if(!layer.images.empty())return layer.images.front().image;
    throw std::runtime_error("Noise texture did not produce image ink");
}
}
int main(){
    using namespace ii::gui;
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{600,600}));std::optional<NativeWindowNoiseTexture> view;
    WindowNoiseViewOptions input{.width=512,.height=512,.texture={.color={.2,.2,.2,1},.primary_color={.7,.3,.2,1},.noise_strength=.3}};
    good(host.set_content([&](auto& builder)->ii::core::Result<void>{auto result=NativeWindowNoiseTexture::create(builder,{},"noise",input);
        if(!result)return std::unexpected(result.error());view=std::move(*result);return {};}));
    auto original=image(take(tree->drawing_snapshot({600,600})));
    input.width=400;good(view->set_options(input));
    require(image(take(tree->drawing_snapshot({600,600})))==original,"Resize rebuilt an unchanged texture");
    auto invalid=input;invalid.texture.noise_strength=2;
    require(!view->set_options(invalid)&&image(take(tree->drawing_snapshot({600,600})))==original,
            "Invalid noise options replaced the retained texture");
    input.texture.noise_strength=.4;good(view->set_options(input));
    require(image(take(tree->drawing_snapshot({600,600})))!=original,"Changed noise strength did not replace the texture");
    good(host.clear_content());require(!view->set_options(input),"Retired window noise accepted an update");tree->close();
}
