#include "../src/Platform/UIKitWindowGeometry.h"
#include "../src/Platform/UIKitDisplayScale.h"
#include <limits>
#include "../src/Widgets/WidgetState.h"
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {
using namespace ii;
void check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
void good(core::Result<void> result) { if (!result) throw std::runtime_error(result.error().message); }
template<class T> T take(core::Result<T> result) {
    if (!result) throw std::runtime_error(result.error().message);
    return std::move(*result);
}

struct Port final {
    std::shared_ptr<gui::detail::UIKitWindowGeometry> policy;
    std::shared_ptr<gui::detail::WidgetState> state;
    gui::WindowPropertiesTarget target;
    std::vector<gui::WindowGeometry> applied;
    explicit Port(bool managed) : policy(std::make_shared<gui::detail::UIKitWindowGeometry>(managed)) {
        gui::detail::WidgetFrameHooks hooks;
        hooks.window_geometry = [this](const gui::WindowGeometry& input) -> core::Result<void> {
            auto value = policy->prepare(input);
            if (policy->managed() && (value.width || value.height || value.minimum_width || value.minimum_height))
                return core::fail(core::ErrorCode::unsupported, "UIKit scene geometry belongs to the operating system");
            applied.push_back(std::move(value));
            return {};
        };
        state = std::make_shared<gui::detail::WidgetState>([](runtime::Task task) { task(); }, gui::WidgetAppearance{}, std::move(hooks));
        good(state->construct([&](gui::WidgetBuilder& widgets) -> core::Result<void> {
            target = take(widgets.window_properties());
            return {};
        }));
    }
    ~Port() { state->close(); }
    void commit() { good(state->commit_window_properties()); policy->finish_initial(); }
};

void initial_geometry_is_a_hint_but_later_updates_are_not() {
    Port port(true);
    gui::WindowProperties initial{.geometry={.title="Canvas", .width=800, .height=600,
                                             .minimum_width=320, .minimum_height=220}};
    good(port.target.update(initial));
    port.commit();
    check(port.applied.size()==1 && port.applied[0].title=="Canvas" &&
          !port.applied[0].width && !port.applied[0].height &&
          !port.applied[0].minimum_width && !port.applied[0].minimum_height,
          "Initial scene geometry prevented the title from being published");
    auto title=initial; title.geometry.title="Updated";
    good(port.target.update(title));
    check(port.applied.size()==2 && port.applied.back().title=="Updated" && !port.applied.back().width,
          "A title update reintroduced the initial size");
    auto resized=title; resized.geometry.width=900;
    auto rejected=port.target.update(resized);
    check(!rejected && rejected.error().code==core::ErrorCode::unsupported &&
          take(port.target.snapshot())==title && port.applied.size()==2,
          "A later scene-size update was silently accepted or changed the published state");
}

void empty_initial_commit_does_not_swallow_later_size() {
    Port port(true);
    port.commit();
    auto rejected=port.target.update({.geometry={.width=900}});
    check(!rejected && rejected.error().code==core::ErrorCode::unsupported && port.applied.empty(),
          "An empty initial commit made the next size update look initial");
}

void in_scene_secondary_windows_keep_geometry() {
    Port port(false);
    good(port.target.update({.geometry={.title="Tools", .width=400, .height=300}}));
    port.commit();
    check(port.applied.size()==1 && port.applied[0].width==400 && port.applied[0].height==300,
          "An overlay window lost its initial geometry");
    good(port.target.update({.geometry={.title="Tools", .width=500, .height=300}}));
    check(port.applied.size()==2 && port.applied.back().width==500,
          "An overlay window lost its later resize");
}
}

int main() {
    initial_geometry_is_a_hint_but_later_updates_are_not();
    empty_initial_commit_does_not_swallow_later_size();
    in_scene_secondary_windows_keep_geometry();
    using ii::gui::detail::uikit_display_scale_changed;
    check(uikit_display_scale_changed(0,2) && uikit_display_scale_changed(2,3),"UIKit scale transition was lost");
    check(!uikit_display_scale_changed(2,2) && !uikit_display_scale_changed(2,0) &&
          !uikit_display_scale_changed(2,std::numeric_limits<double>::quiet_NaN()),"UIKit admitted an unchanged or invalid scale");
}
