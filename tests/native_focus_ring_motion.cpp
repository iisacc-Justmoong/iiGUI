#include <iiGUI/NativeFocusRingView.h>
#include "../src/Widgets/WidgetState.h"
#include <iiSDKCore/Runtime/FrameLoop.h>
#include <chrono>
#include <memory>
#include <optional>
#include <stdexcept>

namespace {
void require(bool value,const char* why){if(!value)throw std::runtime_error(why);}
template<class T>T take(ii::core::Result<T> value){if(!value)throw std::runtime_error(value.error().message);return std::move(*value);}
void good(ii::core::Result<void> value){if(!value)throw std::runtime_error(value.error().message);}
}
int main(){
    using namespace ii::gui;using namespace std::chrono_literals;
    ii::runtime::EventLoop events;ii::runtime::FrameLoop frames(events);auto preferences=std::make_shared<MotionPreferences>();
    detail::WidgetFrameHooks hooks;hooks.frames=[&]()->ii::core::Result<ii::runtime::FrameLoop*>{return &frames;};hooks.preferences=preferences;
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();},WidgetAppearance{},std::move(hooks));
    auto host=take(tree->content_host({},"host",{160,100}));std::optional<NativeFocusRing> ring;
    FocusRingOptions options{.width=120,.height=40,.radius=4,.stroke=1.5,.color={1,0,0,1}};
    good(host.set_content([&](auto& builder)->ii::core::Result<void>{
        auto made=NativeFocusRing::create(builder,{},"ring",options);if(!made)return std::unexpected(made.error());ring=std::move(*made);return {};
    }));
    const auto start=ii::runtime::Clock::now();(void)frames.step(start);const auto idle=frames.stats().callbacks;
    (void)frames.step(start+10ms);require(frames.stats().callbacks==idle,"Idle focus ring requested frames");
    options.active=true;good(ring->set_options(options));(void)frames.step(start+20ms);
    require(frames.stats().callbacks>idle,"Focus transition did not request a frame");
    (void)frames.step(start+1s);const auto settled=frames.stats().callbacks;(void)frames.step(start+2s);
    require(frames.stats().callbacks==settled,"Settled focus ring kept requesting frames");
    good(preferences->set_reduced_motion(true));options.active=false;good(ring->set_options(options));
    const auto reduced=frames.stats().callbacks;(void)frames.step(start+3s);
    require(frames.stats().callbacks==reduced,"Reduced motion scheduled a focus animation");
    good(preferences->set_reduced_motion(false));options.active=true;good(ring->set_options(options));
    options.motion_enabled=false;good(ring->set_options(options));const auto disabled=frames.stats().callbacks;
    (void)frames.step(start+4s);require(frames.stats().callbacks==disabled,"Disabled focus motion retained a scheduled frame");
    good(host.clear_content());require(!ring->set_options(options),"Retired focus ring accepted motion updates");tree->close();
}
