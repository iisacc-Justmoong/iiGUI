#include <iiGUI/NativeCheckBoxView.h>
#include <iiGUI/WidgetState.h>
#include <memory>
#include <optional>
#include <stdexcept>
#include <vector>
namespace {
void require(bool v,const char* m){if(!v)throw std::runtime_error(m);}
template<class T>T take(ii::core::Result<T> v){if(!v)throw std::runtime_error(v.error().message);return std::move(*v);}
void good(ii::core::Result<void> v){if(!v)throw std::runtime_error(v.error().message);}
struct AuthoredPolicy:ii::gui::DefaultCheckBoxHostPolicy {
    inline static std::weak_ptr<const ii::gui::RasterImage> enabled_image,disabled_image;
    static ii::core::Result<std::shared_ptr<const ii::gui::RasterImage>> authored_image(bool enabled){
        auto image=ii::gui::rgba_image(1,1,std::vector<std::uint8_t>{enabled?std::uint8_t(255):std::uint8_t(100),0,0,255});
        if(image)(enabled?enabled_image:disabled_image)=*image;
        return image;
    }
};
}
int main(){
    using namespace ii::gui;
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{160,100}));
    CheckBoxConfiguration input;input.text="Remember";input.motion_enabled=false;
    input.palette.checked={.2,.4,.6,1};input.palette.unchecked={.1,.1,.1,1};input.palette.mark={1,1,1,1};
    std::optional<NativeCheckBox> box;std::vector<bool> events;
    auto owner=std::make_shared<int>(1);std::weak_ptr<int> weak=owner;
    good(host.set_content([&](WidgetBuilder& b)->ii::core::Result<void>{
        auto result=NativeCheckBox::create(b,{},"check",input,[&,keep=owner](bool value){events.push_back(value);});
        if(!result)return std::unexpected(result.error());box=*result;return {};
    }));
    owner.reset();const auto root=box->root();const auto initial=take(root.snapshot());
    require(initial.action_role==ActionRole::check_box&&!initial.checked&&!weak.expired(),"checkbox role or callback ownership");
    (void)take(tree->drawing_snapshot({160,100}));good(root.activate());
    require(take(root.snapshot()).checked&&events==std::vector<bool>{true},"activation failed");
    good(box->set_options(input));require(take(root.snapshot()).checked,"unchanged source reset selection");
    good(box->set_checked(false));require(!take(root.snapshot()).checked&&events.size()==1,"explicit write failed or emitted callback");
    auto invalid=input;invalid.box_size=0;require(!box->set_options(invalid)&&!take(root.snapshot()).checked,"invalid state committed");
    good(host.clear_content());require(weak.expired()&&!box->set_options(input),"retired checkbox retained callback or accepted updates");
    input.checked=true;input.use_authored_checked_assets=true;
    std::optional<BasicNativeCheckBox<AuthoredPolicy>> authored;
    good(host.set_content([&](WidgetBuilder& b)->ii::core::Result<void>{
        auto result=BasicNativeCheckBox<AuthoredPolicy>::create(b,{},"authored",input);
        if(!result)return std::unexpected(result.error());authored=*result;return {};
    }));
    auto frame=take(tree->drawing_snapshot({160,100}));bool found=false;
    auto scan=[&](const auto& images){for(const auto& image:images)if(image.image&&image.image->width()==1&&!image.tint)found=true;};
    scan(frame.images);for(const auto& layer:frame.overlays)scan(layer.images);
    require(found&&!AuthoredPolicy::enabled_image.expired()&&!AuthoredPolicy::disabled_image.expired(),
            "injected authored image was not rendered or retained while live");
    frame={};good(host.clear_content());
    require(AuthoredPolicy::enabled_image.expired()&&AuthoredPolicy::disabled_image.expired(),
            "retired checkbox retained injected images");
    tree->close();
}
