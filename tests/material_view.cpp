#include <iiGUI/NativeMaterialView.h>
#include <iiGUI/WidgetState.h>
#include <array>
#include <memory>
#include <optional>
#include <stdexcept>
namespace {
using namespace ii::gui;using ii::core::Result;
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(Result<T> value){if(!value)throw std::runtime_error(value.error().message);return std::move(*value);}
void good(Result<void> value){if(!value)throw std::runtime_error(value.error().message);}
struct GradientPolicy{using Material=MaterialModel;inline static constexpr std::array<double,3> edge_rgb{.1,.5,1};
    static std::uint64_t geometry_revision(const Material&){return 0;}};
struct Policy{using Model=MaterialModel;using Gradient=BasicMaterialGradient<GradientPolicy>;
    using Options=MaterialConfiguration;using Color=WidgetColor;using Backdrop=MaterialBackdrop;};
}
int main(){
    using Surface=ii::gui::BasicNativeMaterial<Policy>;
    auto tree=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{200,100}));std::optional<Surface> surface;
    good(host.set_content([&](auto& builder)->Result<void>{
        ii::gui::MaterialConfiguration options;options.kind=ii::gui::MaterialKind::panel;options.width=100;options.height=50;
        auto created=Surface::create(builder,{},"surface",options);if(!created)return std::unexpected(created.error());surface=*created;return {};
    }));
    std::weak_ptr<const ii::gui::RasterImage> weak;
    {auto frame=take(tree->drawing_snapshot({200,100}));
     for(const auto& layer:frame.overlays)if(layer.backdrop&&layer.backdrop->underlay)weak=layer.backdrop->underlay;
     require(!weak.expired(),"Material did not produce a retained gradient");}
    good(host.clear_content());require(weak.expired(),"Retired material retained its gradient through a public handle");
    require(!surface->set_options({}),"Retired material accepted input");
    ii::gui::MaterialConfiguration neutral;neutral.kind=ii::gui::MaterialKind::window;
    neutral.width=100;neutral.height=50;neutral.intense_opacity=0;neutral.faint_opacity=0;neutral.motion_enabled=false;
    good(host.set_content([&](auto& builder)->Result<void>{
        auto created=Surface::create(builder,{},"neutral-window",neutral);if(!created)return std::unexpected(created.error());surface=*created;return {};
    }));
    const auto check_neutral=[&](double opacity,double blur){
        auto frame=take(tree->drawing_snapshot({200,100}));bool found=false;
        for(const auto& layer:frame.overlays)if(layer.backdrop){
            require(!layer.backdrop->underlay&&layer.backdrop->tint.alpha==opacity&&layer.backdrop->blur_radius==blur,
                    "Neutral window material created an accent underlay or lost its density");found=true;
        }
        require(found,"Neutral window material was not drawn");
    };
    check_neutral(.75,64);neutral.density=ii::gui::MaterialDensity::glass25;good(surface->set_options(neutral));
    check_neutral(.25,16);tree->close();
}
