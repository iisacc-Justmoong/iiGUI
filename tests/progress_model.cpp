#include <iiGUI/ProgressIndicator.h>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace {
void require(bool value) { if (!value) throw std::runtime_error("iiGUI progress model contract failed"); }
bool near(double a, double b) { return std::abs(a - b) < 1e-9; }
}

int main() {
    ii::gui::ProgressModel bar;
    require(bool(bar.set_values({0, 100, 25, 75})));
    auto geometry = bar.geometry(200, 6);
    require(geometry && near(geometry->fill_x, 50) && near(geometry->fill_width, 100));
    require(near(geometry->track_radius, 3) && near(geometry->fill_radius, 3));
    require(bool(bar.set_values({100, 0, 80, 20})));
    require(near(bar.normalized_current(), .8));
    geometry = bar.geometry(100, 6);
    require(geometry && near(geometry->fill_x, 20) && near(geometry->fill_width, 60));
    require(bool(bar.set_values({0, 100, -50, 150})));
    geometry = bar.geometry(10, 1);
    require(geometry && near(geometry->fill_x, 0) && near(geometry->fill_width, 10) &&
            near(geometry->stroke_width, 0.5));
    require(bool(bar.set_values({5, 5, 4, 5})));
    require(near(bar.normalized_current(), 1));
    geometry = bar.geometry(20, 6);
    require(geometry && near(geometry->fill_width, 20));
    const auto before = bar.values();
    require(!bar.set_current(std::numeric_limits<double>::quiet_NaN()) && bar.values() == before);
    require(!bar.set_values({0, std::numeric_limits<double>::infinity(), 0, 0}) && bar.values() == before);
    require(!bar.geometry(-1, 6) && !bar.geometry(std::numeric_limits<double>::infinity(), 6));
    geometry = bar.geometry(0, 0);
    require(geometry && geometry->track_radius == 0 && geometry->stroke_width == 0);

    auto settings = bar.configuration();
    settings.values = {100, 0, 80, 20};
    settings.style.size = ii::gui::ProgressSize::regular;
    settings.style.shape = ii::gui::ProgressShape::round_rect;
    settings.style.corner_radius = 1;
    settings.style.width = 200;
    settings.style.regular_height = 4;
    settings.style.displayed_start = .25;
    settings.style.displayed_progress = .5;
    settings.style.fill = {.1, .2, .3, .4};
    require(bool(bar.set_configuration(settings)));
    require(bar.extent().width == 200 && bar.extent().height == 4);
    geometry = bar.geometry(200, 4);
    require(geometry && near(geometry->fill_x, 50) && near(geometry->fill_width, 100) &&
            near(geometry->track_radius, 1) && near(geometry->fill_radius, 1));
    settings.style.displayed_start = .8;
    settings.style.displayed_progress = .9;
    settings.style.shape = ii::gui::ProgressShape::cylinder;
    require(bool(bar.set_configuration(settings)));
    geometry = bar.geometry(200, 4);
    require(geometry && near(geometry->fill_x, 160) && near(geometry->fill_width, 40) &&
            near(geometry->track_radius, 2));
    const auto previous = bar.configuration();
    settings.style.width = -1;
    settings.values.current = 80;
    require(!bar.set_configuration(settings) && bar.configuration() == previous);
    settings = previous;
    settings.style.track.red = 2;
    require(!bar.set_configuration(settings) && bar.configuration() == previous);
    require(ii::gui::progress_size(1) == ii::gui::ProgressSize::regular &&
            ii::gui::progress_shape(0) == ii::gui::ProgressShape::round_rect);
    bool rejected = false;
    try { (void)ii::gui::progress_size(2); } catch (const std::invalid_argument&) { rejected = true; }
    require(rejected);

    ii::gui::ProgressState source;
    source.numbers = {{"minimumValue", 10}, {"maximumValue", 110}, {"begin", 30}, {"done", 80}};
    ii::gui::ProgressStateKeys keys;
    keys.minimum = "  ";
    keys.start = " begin ";
    keys.current = "done";
    auto state_values = ii::gui::progress_values({0, 100, 0, 50}, keys, source);
    require(state_values == ii::gui::ProgressValues{10, 110, 30, 80});
    keys.maximum = "missing";
    state_values = ii::gui::progress_values({0, 100, 0, 50}, keys, source);
    require(state_values == ii::gui::ProgressValues{10, 100, 30, 80});
    std::optional<ii::gui::ProgressState> absent;
    require(ii::gui::progress_values({0, 100, 0, 50}, keys, absent) == ii::gui::ProgressValues{0, 100, 0, 50});
    auto owned = std::make_shared<ii::gui::ProgressState>(source);
    require(ii::gui::progress_values({0, 100, 0, 50}, keys, owned) == state_values);
    std::shared_ptr<const ii::gui::ProgressStateSource> provider = owned;
    std::weak_ptr<const ii::gui::ProgressStateSource> borrowed = provider;
    require(ii::gui::progress_values({0, 100, 0, 50}, keys, borrowed) == state_values);
    provider.reset(); owned.reset();
    require(ii::gui::progress_values({0, 100, 0, 50}, keys, borrowed) == ii::gui::ProgressValues{0, 100, 0, 50});
    source.numbers["done"] = std::numeric_limits<double>::quiet_NaN();
    settings = previous;
    settings.values = ii::gui::progress_values(settings.values, keys, source);
    require(!bar.set_configuration(settings) && bar.configuration() == previous);

    using namespace std::chrono_literals;
    ii::gui::ProgressModel animated;
    auto animated_config = animated.configuration();
    animated_config.values.current = 100;
    animated_config.motion.curve = ii::gui::WidgetMotionCurve::linear;
    animated_config.motion.duration = 400ms;
    const auto t = std::chrono::steady_clock::time_point{} + 10s;
    require(bool(animated.set_animated_configuration(animated_config, t)) && animated.animating());
    require(near(animated.geometry(200, 6)->fill_width, 0));
    require(animated.advance(t+200ms));
    require(near(animated.geometry(200, 6)->fill_width, 100));
    ii::gui::ProgressModel color_animation;
    require(bool(color_animation.set_animated_configuration(animated_config, t)) && color_animation.advance(t+200ms));
    auto recolored = animated_config; recolored.style.fill = {1, 0, 0, 1};
    require(bool(color_animation.set_animated_configuration(recolored, t+200ms)) && color_animation.animating());
    require(color_animation.advance(t+300ms) && near(color_animation.geometry(200, 6)->fill_width, 150));
    require(!color_animation.advance(t+400ms) && near(color_animation.geometry(200, 6)->fill_width, 200));
    animated_config.values.current = 20;
    require(bool(animated.set_animated_configuration(animated_config, t+200ms)) && animated.animating());
    require(near(animated.geometry(200, 6)->fill_width, 100));
    require(animated.advance(t+400ms));
    require(near(animated.geometry(200, 6)->fill_width, 70));
    animated_config.motion.reduced_motion = true;
    require(bool(animated.set_animated_configuration(animated_config, t+400ms)) && !animated.animating());
    require(near(animated.geometry(200, 6)->fill_width, 40));
    const auto before_invalid_motion = animated.configuration();
    animated_config.motion.duration = -1ms;
    animated_config.values.current = 80;
    require(!animated.set_animated_configuration(animated_config, t+500ms) &&
            animated.configuration() == before_invalid_motion && near(animated.geometry(200, 6)->fill_width, 40));
}
