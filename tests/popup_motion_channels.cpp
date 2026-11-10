#include <iiGUI/WidgetState.h>
#include <iiSDKCore/Runtime/FrameLoop.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

using namespace ii;
using namespace std::chrono_literals;
static_assert(IIGUI_POPOVER_UNDERLAY_BOUNDS_API>=1);

using BuilderPopover = core::Result<gui::Widget> (gui::WidgetBuilder::*)(gui::Widget,std::string_view,
    gui::PopoverOptions,gui::PopoverEvents,gui::detail::PopoverMotionAbiV2);
using WidgetPopoverOptions = core::Result<void> (gui::Widget::*)(gui::PopoverOptions,
    gui::detail::PopoverMotionAbiV2) const;
using StatePopover = core::Result<gui::Widget> (gui::detail::WidgetState::*)(gui::Widget,std::string_view,
    gui::PopoverOptions,gui::PopoverEvents,gui::detail::PopoverMotionAbiV2);
using StatePopoverOptions = core::Result<void> (gui::detail::WidgetState::*)(std::size_t,gui::PopoverOptions,
    gui::detail::PopoverMotionAbiV2);
static_assert(std::is_same_v<decltype(&gui::WidgetBuilder::popover),BuilderPopover>);
static_assert(std::is_same_v<decltype(&gui::Widget::set_popover_options),WidgetPopoverOptions>);
static_assert(std::is_same_v<decltype(&gui::detail::WidgetState::popover),StatePopover>);
static_assert(std::is_same_v<decltype(&gui::detail::WidgetState::popover_options),StatePopoverOptions>);

namespace {
void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
template<class T> T take(core::Result<T> result) {
    if (!result) {
        if (result.error().cause) std::rethrow_exception(result.error().cause);
        throw std::runtime_error(result.error().message);
    }
    return std::move(*result);
}
void good(core::Result<void> result) {
    if (!result) {
        if (result.error().cause) std::rethrow_exception(result.error().cause);
        throw std::runtime_error(result.error().message);
    }
}
bool near(double value, double expected, double tolerance = .04) {
    return std::abs(value - expected) < tolerance;
}
struct Fixture final {
    runtime::EventLoop events;
    runtime::FrameLoop frames{events};
    std::shared_ptr<gui::MotionPreferences> preferences = std::make_shared<gui::MotionPreferences>();
    std::shared_ptr<gui::detail::WidgetState> tree;
    gui::Widget host, popup, label;
    gui::PopoverOptions options;
    runtime::Clock::time_point start;
    int closed = 0;

    explicit Fixture(bool separate = true) {
        gui::detail::WidgetFrameHooks hooks;
        hooks.frames = [&]() -> core::Result<runtime::FrameLoop*> { return &frames; };
        hooks.preferences = preferences;
        tree = std::make_shared<gui::detail::WidgetState>(
            [](runtime::Task task) { task(); }, gui::WidgetAppearance{}, std::move(hooks));
        host = take(tree->content_host({}, "host", {400, 300}));
        options.width = 160;
        options.height = 100;
        options.padding = 0;
        options.margins = 0;
        options.transition = gui::PopoverTransition{
            .enter_duration = 400ms, .exit_duration = 200ms,
            .enter_easing = animation::Easing::linear, .exit_easing = animation::Easing::linear,
            .collapsed_scale = .5, .preferences = preferences};
        if (separate) {
            options.transition->opacity = gui::PopoverOpacityTransition{
                .enter_duration = 160ms, .exit_duration = 150ms,
                .enter_easing = animation::Easing::linear, .exit_easing = animation::Easing::linear};
            options.transition->hold_scale_on_exit = true;
        }
        good(host.set_content([&](gui::WidgetBuilder& builder) -> core::Result<void> {
            auto root = take(builder.vstack({}, "root"));
            popup = take(builder.popover(root, "popup", options, {.closed = [&] { ++closed; }}));
            label = take(builder.label(popup, "label", {.text = "Popup motion"}));
            return {};
        }));
        (void)take(tree->drawing_snapshot({400, 300}));
        good(popup.open_at(20, 20));
        start = runtime::Clock::now();
        // Put the test clock ahead of real time, then begin a fresh transition
        // at an exactly known frame boundary even when opening is delayed.
        step(1h);
        good(popup.close_popup());
        step(2h);
        good(popup.open_at(20, 20));
        start += 2h;
        closed = 0;
    }
    ~Fixture() { tree->close(); }
    void step(std::chrono::milliseconds offset) { (void)frames.step(start + offset); }
    gui::ContentTransform transform() {
        auto scene = take(label.snapshot_scene());
        for (const auto& layer : scene.layers)
            for (const auto& text : layer.texts)
                if (text.text == "Popup motion") return layer.content_transform;
        throw std::runtime_error("Popup label was not present in its scene");
    }
};
void independent_tracks() {
    Fixture f;
    f.step(80ms);
    auto first = f.transform();
    if (!near(first.scale, .6) || !near(first.opacity, .5))
        std::cerr << "First enter scale=" << first.scale << " opacity=" << first.opacity << '\n';
    require(near(first.scale, .6) && near(first.opacity, .5),
            "Opacity and scale did not use their separate enter durations");
    f.step(200ms);
    auto second = f.transform();
    require(near(second.scale, .75) && near(second.opacity, 1),
            "Opacity did not finish independently of scale");
    f.step(500ms);
    good(f.popup.close_popup());
    auto closed = take(f.popup.snapshot());
    require(!closed.opened && closed.visible && closed.exiting && f.closed == 1,
            "Closing did not immediately revoke input and retain exit pixels");
    f.step(575ms);
    auto exit = f.transform();
    require(near(exit.scale, 1) && near(exit.opacity, .5),
            "Exit did not hold scale while fading independently");
    good(f.popup.open_at(20, 20));
    auto reopening = f.transform();
    require(near(reopening.scale, exit.scale, .005) && near(reopening.opacity, exit.opacity, .005),
            "Reopening changed the currently displayed transform");
    f.step(655ms);
    auto resumed = f.transform();
    require(near(resumed.scale, 1) && near(resumed.opacity, .75),
            "Reopening did not resume both tracks from their current values");
    f.step(1100ms);
    good(f.popup.close_popup());
    f.step(1300ms);
    require(!take(f.popup.snapshot()).visible, "Completed exit retained the popup");
    good(f.popup.open_at(20, 20));
    f.step(1380ms);
    auto fresh = f.transform();
    require(near(fresh.scale, .6) && near(fresh.opacity, .5),
            "A fresh opening reused the previous exit transform");
}
void compatibility() {
    Fixture f(false);
    f.step(200ms);
    auto middle = f.transform();
    require(near(middle.scale, .75) && near(middle.opacity, .5),
            "The original single-progress transition changed");
    f.step(500ms);
    good(f.popup.close_popup());
    f.step(600ms);
    auto exiting = f.transform();
    require(near(exiting.scale, .75) && near(exiting.opacity, .5),
            "The original single-progress exit changed");
}
void preferences_and_validation() {
    Fixture f;
    f.step(80ms);
    auto invalid = f.options;
    invalid.transition->opacity->enter_duration = -1ms;
    auto rejected = f.popup.set_popover_options(invalid);
    require(!rejected && rejected.error().code == core::ErrorCode::invalid_argument,
            "A negative opacity duration was accepted");
    auto before = f.transform();
    require(near(before.scale, .6) && near(before.opacity, .5),
            "An invalid setting changed the active transition");
    auto shared = f.options;
    shared.transition->opacity.reset();
    auto switching = f.popup.set_popover_options(shared);
    require(!switching && switching.error().code == core::ErrorCode::busy,
            "An active transition discarded its independent opacity channel");
    auto still = f.transform();
    require(near(still.scale, before.scale, .005) && near(still.opacity, before.opacity, .005),
            "A rejected channel change altered the displayed transform");
    good(f.preferences->set_speed(2));
    auto faster = f.transform();
    require(near(faster.scale, .7) && near(faster.opacity, 1),
            "Motion speed did not update both tracks from the same start");
    good(f.preferences->set_reduced_motion(true));
    auto settled = f.transform();
    require(near(settled.scale, 1, .005) && near(settled.opacity, 1, .005) &&
            !take(f.popup.snapshot()).transitioning,
            "Reduced motion did not settle both tracks");
    good(f.popup.close_popup());
    require(!take(f.popup.snapshot()).visible, "Reduced motion retained an exit frame");
}
void interrupted_entry() {
    Fixture f;
    f.step(80ms);
    const auto before = f.transform();
    good(f.popup.close_popup());
    f.step(155ms);
    const auto exit = f.transform();
    require(near(exit.scale, before.scale, .005) && near(exit.opacity, .25),
            "Closing during entry did not freeze its current scale and fade its current opacity");
    good(f.popup.open_at(20, 20));
    const auto resumed = f.transform();
    require(near(resumed.scale, exit.scale, .005) && near(resumed.opacity, exit.opacity, .005),
            "Reopening an interrupted entry jumped to another transform");
}
void opacity_only() {
    Fixture f;
    f.step(500ms);
    good(f.popup.close_popup());
    f.step(700ms);
    auto options = f.options;
    options.transition->enter_duration = options.transition->exit_duration = 0ms;
    good(f.popup.set_popover_options(options));
    good(f.popup.open_at(20, 20));
    f.step(780ms);
    const auto enter = f.transform();
    require(near(enter.scale, 1, .005) && near(enter.opacity, .5),
            "Opacity-only entry failed to reserve or use its frame source");
    f.step(900ms);
    good(f.popup.close_popup());
    f.step(975ms);
    const auto exit = f.transform();
    require(near(exit.scale, 1, .005) && near(exit.opacity, .5),
            "Opacity-only exit ended before the fade completed");
}
void missing_frame_source() {
    auto tree = std::make_shared<gui::detail::WidgetState>([](runtime::Task task) { task(); });
    auto host = take(tree->content_host({}, "host", {200, 100}));
    gui::Widget retained;
    good(host.set_content([&](gui::WidgetBuilder& builder) -> core::Result<void> {
        retained = take(builder.label({}, "retained", {.text = "Existing"}));
        return {};
    }));
    auto failed = host.set_content([&](gui::WidgetBuilder& builder) -> core::Result<void> {
        gui::PopoverOptions options;
        options.transition = gui::PopoverTransition{};
        options.transition->opacity = gui::PopoverOpacityTransition{.enter_duration = 160ms};
        auto result = builder.popover({}, "popup", options);
        if (!result) return std::unexpected(result.error());
        return {};
    });
    auto retained_snapshot = retained.snapshot();
    require(!failed && failed.error().code == core::ErrorCode::unsupported &&
            retained_snapshot && retained_snapshot->text == "Existing",
            "Opacity-only motion without frames replaced the existing content");
    tree->close();
}
void opaque_popup_border() {
    auto tree=std::make_shared<gui::detail::WidgetState>([](runtime::Task task){task();});
    auto host=take(tree->content_host({},"opaque-host",{300,200}));gui::Widget popup;
    gui::PopoverOptions options;options.width=160;options.height=100;options.opaque_base=true;
    options.border_width=1;options.border_color={1,1,1,.22};
    good(host.set_content([&](gui::WidgetBuilder& builder)->core::Result<void>{
        auto result=builder.popover({},"opaque-popup",options);if(!result)return std::unexpected(result.error());popup=*result;return {};
    }));
    good(popup.open_at(20,20));
    const auto has_border=[&]{const auto frame=take(tree->drawing_snapshot({300,200}));const auto bounds=take(popup.snapshot()).bounds;
        for(const auto& layer:frame.overlays)for(const auto& panel:layer.panels)
            if(panel.bounds==bounds&&panel.stroke_width==1&&near(panel.top.alpha,.22,.0001))return true;
        return false;};
    require(has_border(),"Opaque popup omitted its configured border");
    options.border_width=0;good(popup.set_popover_options(options));
    require(!has_border(),"Opaque popup retained a disabled border");
    good(host.clear_content());tree->close();
}
void popup_inner_highlight() {
    auto tree=std::make_shared<gui::detail::WidgetState>([](runtime::Task task){task();});
    auto host=take(tree->content_host({},"highlight-host",{300,200}));gui::Widget popup;
    gui::PopoverOptions options;options.width=160;options.height=100;options.opaque_base=false;
    options.blur_radius=16;options.inner_highlight_opacity=.14;options.inner_highlight_offset_y=1;
    good(host.set_content([&](gui::WidgetBuilder& builder)->core::Result<void>{
        auto result=builder.popover({},"highlight-popup",options);if(!result)return std::unexpected(result.error());popup=*result;return {};
    }));
    good(popup.open_at(20,20));
    const auto has_highlight=[&](double opacity,double offset){const auto frame=take(tree->drawing_snapshot({300,200}));
        for(const auto& layer:frame.overlays)if(layer.backdrop&&
            near(layer.backdrop->inner_highlight_opacity,opacity,.0001)&&
            near(layer.backdrop->inner_highlight_offset_y,offset,.0001))return true;
        return false;};
    require(has_highlight(.14,1),"Popup material omitted its inner highlight");
    auto invalid=options;invalid.inner_highlight_opacity=1.1;
    require(!popup.set_popover_options(invalid)&&has_highlight(.14,1),"Invalid highlight opacity replaced valid options");
    invalid=options;invalid.inner_highlight_offset_y=std::numeric_limits<double>::infinity();
    require(!popup.set_popover_options(invalid)&&has_highlight(.14,1),"Invalid highlight offset replaced valid options");
    options.inner_highlight_opacity=.06;options.inner_highlight_offset_y=-2;good(popup.set_popover_options(options));
    require(has_highlight(.06,-2),"Popup material did not update its highlight");
    options.blur_radius.reset();good(popup.set_popover_options(options));
    require(has_highlight(.06,-2),"Non-blurred translucent popup lost its highlight");
    options.opaque_base=true;good(popup.set_popover_options(options));
    require(has_highlight(.06,-2),"Opaque popup lost its inner highlight");
    const auto opaque=take(tree->drawing_snapshot({300,200}));bool isolated=false;
    for(const auto& layer:opaque.overlays)if(layer.backdrop&&
        near(layer.backdrop->inner_highlight_opacity,.06,.0001))
        isolated=!layer.backdrop->capture&&layer.backdrop->base.alpha==1&&layer.backdrop->blur_radius==0;
    require(isolated,"Opaque popup highlight captured the preceding scene");
    good(host.clear_content());tree->close();
}
void animated_popup_inner_highlight() {
    Fixture f;
    f.options.opaque_base=false;f.options.blur_radius=16;
    f.options.inner_highlight_opacity=.14;f.options.inner_highlight_offset_y=1;
    good(f.popup.set_popover_options(f.options));
    const auto found=[&]{const auto frame=take(f.tree->drawing_snapshot({400,300}));
        for(const auto& layer:frame.overlays)if(layer.backdrop&&
            near(layer.backdrop->inner_highlight_opacity,.14,.0001)&&
            near(layer.backdrop->inner_highlight_offset_y,1,.0001))return true;
        return false;};
    require(found(),"Animated popup lost its material highlight");
    f.options.blur_radius.reset();good(f.popup.set_popover_options(f.options));
    require(found(),"Animated translucent popup without blur lost its material highlight");
}
void popup_material_underlay() {
    auto tree=std::make_shared<gui::detail::WidgetState>([](runtime::Task task){task();});
    auto host=take(tree->content_host({},"underlay-host",{300,200}));gui::Widget popup;
    auto image=take(gui::rgba_image(2,2,std::vector<std::uint8_t>(16,255)));
    gui::PopoverOptions options;options.width=160;options.height=100;options.opaque_base=false;options.blur_radius=16;
    good(host.set_content([&](gui::WidgetBuilder& builder)->core::Result<void>{
        auto result=builder.popover({},"underlay-popup",options);if(!result)return std::unexpected(result.error());popup=*result;return {};
    }));
    bool guarded=false;int calls=0;
    const auto provider=[&](gui::Size size,double scale)->core::Result<std::shared_ptr<const gui::RasterImage>>{
        ++calls;require(size.width==160&&size.height==100&&scale>0,"Underlay did not receive the actual popup extent and scale");
        const auto blocked=popup.close_popup();guarded=!blocked&&blocked.error().code==core::ErrorCode::busy;return image;
    };
    good(popup.set_popover_underlay(provider,{.2,.4,.6,1}));good(popup.open_at(20,20));
    const auto has_underlay=[&]{const auto frame=take(tree->drawing_snapshot({300,200}));const auto bounds=take(popup.snapshot()).bounds;
        for(const auto& layer:frame.overlays)if(layer.backdrop&&layer.backdrop->bounds==bounds&&
            layer.backdrop->underlay==image&&layer.backdrop->underlay_bounds==bounds&&layer.backdrop->underlay_tint&&
            near(layer.backdrop->underlay_tint->red,std::pow((.2+.055)/1.055,2.4),.0001))return true;
        return false;};
    require(has_underlay()&&guarded&&calls>0,"Popup material did not compose its guarded underlay");
    require(!popup.set_popover_underlay(provider,{2,0,0,1})&&has_underlay(),"Invalid underlay tint replaced the previous provider");
    good(popup.set_popover_underlay([](gui::Size,double)->core::Result<std::shared_ptr<const gui::RasterImage>>{
        return core::fail(core::ErrorCode::failed,"Underlay failed");},{}));
    require(!tree->drawing_snapshot({300,200}),"Underlay provider failure was silently ignored");
    good(popup.set_popover_underlay(provider,{.2,.4,.6,1}));require(has_underlay(),"Popup underlay did not recover");
    options.opaque_base=true;options.blur_radius.reset();options.inner_highlight_opacity=.14;
    good(popup.set_popover_options(options));
    require(has_underlay(),"Opaque popup lost its material underlay");
    const auto opaque=take(tree->drawing_snapshot({300,200}));bool isolated=false;
    for(const auto& layer:opaque.overlays)if(layer.backdrop&&layer.backdrop->underlay==image)
        isolated=!layer.backdrop->capture&&layer.backdrop->base.alpha==1&&
            layer.backdrop->blur_radius==0&&near(layer.backdrop->inner_highlight_opacity,.14,.0001);
    require(isolated,"Opaque popup underlay did not use an isolated opaque base and highlight");
    options.opaque_base=false;options.blur_radius=16;good(popup.set_popover_options(options));require(has_underlay(),"Popup underlay did not return after enabling blur");
    good(popup.set_popover_underlay({},{}));require(!has_underlay(),"Removing the underlay left the previous image visible");
    good(host.clear_content());tree->close();
}
void animated_popup_underlay() {
    Fixture f;
    auto image=take(gui::rgba_image(1,1,std::vector<std::uint8_t>(4,255)));
    f.options.opaque_base=false;f.options.blur_radius.reset();f.options.inner_highlight_opacity=0;
    good(f.popup.set_popover_options(f.options));
    good(f.popup.set_popover_underlay([image](gui::Size,double)->core::Result<std::shared_ptr<const gui::RasterImage>>{return image;}));
    const auto frame=take(f.tree->drawing_snapshot({400,300}));
    bool found=false;for(const auto& layer:frame.overlays)if(layer.backdrop&&layer.backdrop->underlay==image)found=true;
    require(found,"Animated translucent popup without blur lost its underlay");
    good(f.popup.set_popover_underlay_with_bounds([image](gui::Size,double)->core::Result<gui::PopoverUnderlayFrame>{
        return gui::PopoverUnderlayFrame{image,{-4,-6,168,112}};
    }));
    f.options.opaque_base=true;f.options.inner_highlight_opacity=.14;
    good(f.popup.set_popover_options(f.options));f.step(200ms);
    const auto bounds=take(f.popup.snapshot()).bounds;
    const gui::Rect expected{bounds.x-4,bounds.y-6,bounds.width+8,bounds.height+12};
    const auto has_opaque_material=[&]{const auto result=take(f.tree->drawing_snapshot({400,300}));
        for(const auto& layer:result.overlays)if(layer.backdrop&&layer.backdrop->underlay==image&&
            layer.backdrop->underlay_bounds==expected&&!layer.backdrop->capture&&layer.backdrop->base.alpha==1&&
            near(layer.backdrop->inner_highlight_opacity,.14,.0001)&&layer.content_transform.scale<1)return true;
        return false;};
    require(has_opaque_material(),"Animated opaque popup lost its transformed material");
    f.step(500ms);good(f.popup.close_popup());f.step(575ms);
    const auto closing=take(f.tree->drawing_snapshot({400,300}));found=false;
    for(const auto& layer:closing.overlays)if(layer.backdrop&&layer.backdrop->underlay==image&&
        !layer.backdrop->capture&&layer.content_transform.opacity>0&&layer.content_transform.opacity<1)found=true;
    require(found,"Animated opaque popup lost its material during exit");
}
void popup_underlay_expanded_bounds() {
    auto tree=std::make_shared<gui::detail::WidgetState>([](runtime::Task task){task();});
    auto host=take(tree->content_host({},"bounds-host",{300,200}));gui::Widget popup;
    auto image=take(gui::rgba_image(2,2,std::vector<std::uint8_t>(16,255)));
    gui::PopoverOptions options;options.width=160;options.height=100;options.opaque_base=false;options.blur_radius=16;
    good(host.set_content([&](gui::WidgetBuilder& builder)->core::Result<void>{
        auto result=builder.popover({},"bounds-popup",options);if(!result)return std::unexpected(result.error());popup=*result;return {};
    }));
    bool guarded=false;
    good(popup.set_popover_underlay_with_bounds([&](gui::Size size,double scale)->core::Result<gui::PopoverUnderlayFrame>{
        require(size.width==160&&size.height==100&&scale>0,"Expanded underlay did not receive the popup extent");
        const auto blocked=popup.close_popup();guarded=!blocked&&blocked.error().code==core::ErrorCode::busy;
        return gui::PopoverUnderlayFrame{image,{-16,-8,192,116}};
    },{.2,.4,.6,1}));good(popup.open_at(40,40));
    const auto snapshot=take(popup.snapshot()).bounds;const auto frame=take(tree->drawing_snapshot({300,200}));
    const gui::Rect expected{snapshot.x-16,snapshot.y-8,snapshot.width+32,snapshot.height+16};
    require(guarded&&std::ranges::any_of(frame.overlays,[&](const auto& layer){return layer.backdrop&&
        layer.backdrop->underlay==image&&layer.backdrop->underlay_bounds==expected;}),
        "Expanded popup underlay was not translated into window coordinates");
    require(!popup.set_popover_underlay_with_bounds([image](gui::Size,double)->core::Result<gui::PopoverUnderlayFrame>{
        return gui::PopoverUnderlayFrame{image,{0,0,1,1}};
    },{2,0,0,1}),"Expanded underlay accepted an invalid tint");
    const auto retained=take(tree->drawing_snapshot({300,200}));
    require(std::ranges::any_of(retained.overlays,[&](const auto& layer){return layer.backdrop&&
        layer.backdrop->underlay==image&&layer.backdrop->underlay_bounds==expected;}),
        "Invalid expanded underlay tint replaced the previous provider");
    for(const auto invalid:{gui::Rect{0,0,-1,100},gui::Rect{0,0,8193,100},
                            gui::Rect{std::numeric_limits<float>::quiet_NaN(),0,1,1}}){
        good(popup.set_popover_underlay_with_bounds([image,invalid](gui::Size,double)->core::Result<gui::PopoverUnderlayFrame>{
            return gui::PopoverUnderlayFrame{image,invalid};
        }));
        require(!tree->drawing_snapshot({300,200}),"Invalid expanded underlay bounds were accepted");
    }
    good(popup.set_popover_underlay([image](gui::Size,double)->core::Result<std::shared_ptr<const gui::RasterImage>>{return image;}));
    const auto legacy=take(tree->drawing_snapshot({300,200}));
    require(std::ranges::any_of(legacy.overlays,[&](const auto& layer){return layer.backdrop&&
        layer.backdrop->underlay==image&&layer.backdrop->underlay_bounds==snapshot;}),
        "Legacy popup underlay did not retain the popup bounds");
    good(popup.set_popover_underlay_with_bounds({},{}));
    const auto removed=take(tree->drawing_snapshot({300,200}));
    require(std::ranges::none_of(removed.overlays,[&](const auto& layer){return layer.backdrop&&layer.backdrop->underlay;}),
        "Removing an expanded underlay left its image visible");
    good(host.clear_content());tree->close();
}
void popup_material_follows_content() {
    Fixture f;
    auto image=take(gui::rgba_image(1,1,std::vector<std::uint8_t>(4,255)));
    f.options.opaque_base=false;f.options.blur_radius=16;f.options.surface_opacity=.25;
    f.options.surface_color=gui::WidgetColor{.2,.3,.4,1};
    f.options.border_width=1;f.options.border_color={1,1,1,.22};
    f.options.inner_highlight_opacity=.14;
    f.options.shadow=gui::PopoverShadow{};
    good(f.popup.set_popover_options(f.options));
    bool guarded=false;
    good(f.popup.set_popover_underlay_with_bounds([&](gui::Size,double)->core::Result<gui::PopoverUnderlayFrame>{
        const auto blocked=f.popup.set_popover_material_follows_content(false);
        guarded=!blocked&&blocked.error().code==core::ErrorCode::busy;
        return gui::PopoverUnderlayFrame{image,{-4,-6,168,112}};
    }));
    auto unsupported=f.host.set_popover_material_follows_content(true);
    require(!unsupported&&unsupported.error().code==core::ErrorCode::invalid_argument,
        "Material motion was accepted on a non-Popover widget");
    good(f.popup.set_popover_material_follows_content(true));f.step(80ms);
    const auto content=f.transform();
    const auto combined=[&](const gui::CanvasFrame& frame){
        return std::ranges::any_of(frame.overlays,[&](const auto& layer){
            return layer.backdrop&&layer.shadow&&layer.backdrop->underlay==image&&
                layer.backdrop->blur_radius==16&&layer.backdrop->border_width==1&&
                near(layer.backdrop->inner_highlight_opacity,.14,.0001)&&
                near(layer.backdrop->tint.alpha,.25,.0001)&&
                near(layer.content_transform.scale,content.scale,.001)&&
                near(layer.content_transform.opacity,content.opacity,.001);
        });
    };
    require(combined(take(f.tree->drawing_snapshot({400,300})))&&guarded,
        "Animated Popover material did not follow its content as one complete layer");
    f.step(500ms);good(f.popup.close_popup());f.step(575ms);
    const auto exiting=f.transform();const auto exit_frame=take(f.tree->drawing_snapshot({400,300}));
    require(std::ranges::any_of(exit_frame.overlays,[&](const auto& layer){return layer.backdrop&&layer.shadow&&
        layer.backdrop->underlay==image&&near(layer.content_transform.scale,exiting.scale,.001)&&
        near(layer.content_transform.opacity,exiting.opacity,.001)&&layer.content_transform.opacity>0;}),
        "Closing Popover lost its transformed material before its exit finished");
    good(f.popup.set_popover_material_follows_content(false));
    const auto legacy=take(f.tree->drawing_snapshot({400,300}));
    require(std::ranges::none_of(legacy.overlays,[&](const auto& layer){return layer.backdrop&&layer.shadow&&
        layer.backdrop->underlay==image;}),"Turning off material motion did not restore legacy rendering");
}
void popup_untinted_underlay() {
    auto tree=std::make_shared<gui::detail::WidgetState>([](runtime::Task task){task();});
    auto host=take(tree->content_host({},"colored-host",{300,200}));gui::Widget popup;
    auto image=take(gui::rgba_image(1,1,{128,32,96,200}));
    gui::PopoverOptions options;options.width=160;options.height=100;options.opaque_base=false;options.blur_radius=16;
    good(host.set_content([&](gui::WidgetBuilder& builder)->core::Result<void>{
        popup=take(builder.popover({},"colored-popup",options));return {};
    }));
    bool guarded=false;
    auto provider=[&](gui::Size,double)->core::Result<gui::PopoverUnderlayFrame>{
        const auto blocked=popup.set_popover_untinted_underlay_with_bounds({});
        guarded=!blocked&&blocked.error().code==core::ErrorCode::busy;
        return gui::PopoverUnderlayFrame{image,{-4,-6,168,112}};
    };
    require(!host.set_popover_untinted_underlay_with_bounds(provider),
        "A non-Popover accepted an untinted material provider");
    good(popup.set_popover_underlay_with_bounds(provider,{.2,.4,.6,1}));good(popup.open_at(20,20));
    auto tinted=take(tree->drawing_snapshot({300,200}));
    require(std::ranges::any_of(tinted.overlays,[&](const auto& layer){return layer.backdrop&&
        layer.backdrop->underlay==image&&layer.backdrop->underlay_tint.has_value();}),
        "The existing tinted provider lost its color");
    good(popup.set_popover_untinted_underlay_with_bounds(provider));
    const auto bounds=take(popup.snapshot()).bounds;
    const gui::Rect expanded{bounds.x-4,bounds.y-6,bounds.width+8,bounds.height+12};
    auto colored=take(tree->drawing_snapshot({300,200}));
    require(guarded&&std::ranges::any_of(colored.overlays,[&](const auto& layer){return layer.backdrop&&
        layer.backdrop->underlay==image&&layer.backdrop->underlay_bounds==expanded&&!layer.backdrop->underlay_tint;}),
        "The colored provider lost its RGBA data, bounds or paint reentrancy guard");
    good(popup.set_popover_untinted_underlay_with_bounds({}));
    auto cleared=take(tree->drawing_snapshot({300,200}));
    require(std::ranges::none_of(cleared.overlays,[&](const auto& layer){return layer.backdrop&&
        layer.backdrop->underlay==image;}),"The cleared colored provider left an underlay");
    good(host.clear_content());tree->close();
}
}
int main() {
    try {
        independent_tracks();
        compatibility();
        preferences_and_validation();
        interrupted_entry();
        opacity_only();
        missing_frame_source();
        opaque_popup_border();
        popup_inner_highlight();
        animated_popup_inner_highlight();
        popup_material_underlay();
        animated_popup_underlay();
        popup_underlay_expanded_bounds();
        popup_material_follows_content();
        popup_untinted_underlay();
        std::cout << "Independent popup motion channels passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
