#include <iiGUI/NativeSegmentedControlView.h>
#include <iiGUI/WidgetState.h>
#include <memory>
#include <optional>
#include <stdexcept>
#include <thread>
#include <vector>
namespace {
void require(bool v,const char* m){if(!v)throw std::runtime_error(m);}
template<class T>T take(ii::core::Result<T> r){if(!r)throw std::runtime_error(r.error().message);return std::move(*r);}
void good(ii::core::Result<void> r){if(!r)throw std::runtime_error(r.error().message);}
}
int main(){
    using namespace ii::gui;
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{200,120}));
    std::optional<NativeSegmentedControl> group;SegmentedControlConfiguration config;
    config.background={.2,.3,.4,1};config.border={.6,.7,.8,1};
    good(host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto value=NativeSegmentedControl::create(builder,{},"segments",config);if(!value)return std::unexpected(value.error());group=*value;
        auto child=builder.action_surface(group->root(),"child",{.text="One",.height=24},{});if(!child)return std::unexpected(child.error());return {};
    }));
    auto frame=take(tree->drawing_snapshot({200,120}));
    bool drawn=!frame.panels.empty();for(const auto& layer:frame.overlays)drawn|=!layer.panels.empty();
    for(const auto& scene:frame.scenes)for(const auto& layer:scene.layers)drawn|=!layer.panels.empty();
    if(!drawn){auto bounds=take(group->root().snapshot()).bounds;throw std::runtime_error("Segmented surface was not drawn, bounds "+
        std::to_string(bounds.width)+"x"+std::to_string(bounds.height));}
    const auto root_bounds=take(group->root().snapshot()).bounds;bool border=false;
    const auto inspect=[&](const auto& panels){for(const auto& panel:panels)if(panel.bounds==root_bounds&&panel.stroke_width>0)border=true;};
    inspect(frame.panels);for(const auto& layer:frame.overlays)inspect(layer.panels);
    for(const auto& scene:frame.scenes)for(const auto& layer:scene.layers)inspect(layer.panels);
    require(!border,"Default segmented surface drew a border");
    std::vector<bool> applied;auto owner=std::make_shared<int>(1);std::weak_ptr<int> weak=owner;
    good(group->register_tone_sink(BorderlessTonePort{.apply=[&,keep=owner](bool value)->ii::core::Result<void>{applied.push_back(value);return {};}}));
    owner.reset();require(!weak.expired()&&applied==std::vector<bool>{true},"Child policy not retained or initialized");
    good(group->set_options(config));require(applied.size()==1,"Identical options redelivered a child policy");
    auto updated=config;updated.force_borderless_tone=false;good(group->set_options(updated));
    require(applied==std::vector<bool>({true,false}),"Changed policy was not delivered");
    auto invalid=updated;invalid.spacing=-1;require(!group->set_options(invalid)&&take(group->options())==updated,"Invalid options replaced prior state");
    bool wrong_thread=false;std::thread worker([&]{auto result=group->set_options(updated);wrong_thread=!result&&result.error().code==ii::core::ErrorCode::wrong_executor;});worker.join();
    require(wrong_thread,"Foreign thread accepted");
    auto weak_port=group->weak_tone_registration_port();good(host.clear_content());
    require(weak.expired()&&!group->options(),"Retired group retained child policy or remained usable");
    require(!weak_port(BorderlessTonePort{.apply=[](bool)->ii::core::Result<void>{return {};}}),"Retired registration remained usable");
    good(host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto value=NativeSegmentedControl::create(builder,{},"reentrant",config);if(!value)return std::unexpected(value.error());group=*value;return {};
    }));
    unsigned calls=0;
    good(group->register_tone_sink([&](bool value)->ii::core::Result<void>{
        ++calls;if(!value){group.reset();good(host.clear_content());}return {};
    }));
    good(group->register_tone_sink([&](bool)->ii::core::Result<void>{++calls;return {};}));
    auto interrupted=group->set_options(updated);
    require(!interrupted&&interrupted.error().code==ii::core::ErrorCode::closed&&calls==3,
            "Retiring during child notification continued delivery or lost its state");
    tree->close();
}
