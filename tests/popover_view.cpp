#include <iiGUI/NativePopoverView.h>
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
struct Callbacks{std::function<void()> opened,closed,canceled;};
struct Policy{using Model=PopoverModel;using Options=PopoverConfiguration;using Placement=PopoverPlacement;
    using MaterialModel=ii::gui::MaterialModel;using MaterialOptions=MaterialConfiguration;using MaterialKind=ii::gui::MaterialKind;
    using Gradient=BasicMaterialGradient<GradientPolicy>;using Callbacks=::Callbacks;
    static WidgetColor default_surface(){return {.2,.2,.2,1};}};
}
int main(){
    using Popup=ii::gui::BasicNativePopover<Policy>;
    auto tree=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{320,200}));std::optional<Popup> popup;
    auto payload=std::make_shared<int>(7);std::weak_ptr<int> weak=payload;int opened=0;
    good(host.set_content([&](auto& builder)->Result<void>{
        ii::gui::PopoverConfiguration options;options.width=180;options.height=100;options.motion_enabled=false;
        auto created=Popup::create(builder,{},"popover",options,Callbacks{.opened=[payload,&opened]{++opened;}});
        if(!created)return std::unexpected(created.error());popup=*created;return {};
    }));
    payload.reset();require(!weak.expired(),"Live popover lost its callback");
    good(popup->root().open_at(0,0));require(opened==1,"Popover open callback was lost");
    std::weak_ptr<const ii::gui::RasterImage> image;
    {auto frame=take(tree->drawing_snapshot({320,200}));
     for(const auto& layer:frame.overlays)if(layer.backdrop&&layer.backdrop->underlay)image=layer.backdrop->underlay;
     require(!image.expired(),"Popover underlay was not generated");}
    good(host.clear_content());require(weak.expired()&&image.expired(),"Retired popover retained callback or underlay resources");
    require(!popup->set_options({}),"Retired popover accepted input");tree->close();
}
