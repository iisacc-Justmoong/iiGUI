#include <iiGUI/NativeImageView.h>
#include <iiGUI/WidgetState.h>
#include <cmath>
#include <limits>
#include <optional>
#include <stdexcept>
#include <vector>
namespace {
void require(bool value,const char* text){if(!value)throw std::runtime_error(text);}
template<class T>T take(ii::core::Result<T> result){if(!result)throw std::runtime_error(result.error().message);return std::move(*result);}
void good(ii::core::Result<void> result){if(!result)throw std::runtime_error(result.error().message);}
}
int main(){
    auto state=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(state->content_host({},"host",{200,200}));
    ii::gui::ImageInput input;
    input.image=take(ii::gui::rgba_image(2,2,std::vector<std::uint8_t>(16,255)));
    input.configuration.width=input.configuration.height=40;
    input.relative_padding=.25;
    input.tint=ii::gui::WidgetColor{.5,.25,1,.75};
    std::optional<ii::gui::NativeImage> image;
    good(host.set_content([&](auto& builder)->ii::core::Result<void>{
        auto stack=builder.vstack({},"layout",{.spacing=0,.alignment=ii::gui::StackAlignment::leading});
        if(!stack)return std::unexpected(stack.error());
        auto created=ii::gui::NativeImage::create(builder,*stack,"image",input);
        if(!created)return std::unexpected(created.error());image=*created;return {};
    }));
    auto frame=take(state->drawing_snapshot({200,200}));
    auto images=frame.images;
    for(const auto& layer:frame.overlays) images.insert(images.end(),layer.images.begin(),layer.images.end());
    require(images.size()==1 && std::abs(images[0].bounds.width-20)<1e-5 && images[0].tint &&
            std::abs(images[0].tint->red-.21404114)<1e-5,"Image geometry or linear tint differs");
    auto invalid=input;invalid.tint=ii::gui::WidgetColor{2,0,0,1};
    require(!image->set_options(invalid),"Invalid image tint was accepted");
    good(host.clear_content());
    require(!image->set_options(input),"Retired image accepted an update");
    state->close();
}
