#pragma once
#include <iiGUI/ImageModel.h>
#include <iiGUI/NativeWidgets.h>
#include <cmath>
#include <cstdint>
#include <exception>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <string_view>
#include <thread>
#include <utility>

namespace ii::gui {
// The iiGUI image view owns validation, geometry, accessibility and drawing.
using ImageRasterSource = std::function<ii::core::Result<std::shared_ptr<const ii::gui::RasterImage>>(
    ii::gui::Size, double)>;
struct ImageInput final {
    std::shared_ptr<const ii::gui::RasterImage> image;
    ImageConfiguration configuration;
    // Optional fraction of the allocated shorter side, evaluated at paint time.
    std::optional<double> relative_padding;
    // Optional alternative to fixed pixels, resolved for each paint request.
    ImageRasterSource raster_source;
    // Optional encoded-sRGB color for a monochrome image mask.
    std::optional<WidgetColor> tint;
};
inline std::optional<WidgetColor> image_color(std::optional<ii::gui::WidgetColor> color) {
    if (!color) return std::nullopt;
    return WidgetColor{color->red, color->green, color->blue, color->alpha};
}
inline bool valid_image_tint(WidgetColor color) noexcept {
    const auto channel=[](double value){return std::isfinite(value)&&value>=0&&value<=1;};
    return channel(color.red)&&channel(color.green)&&channel(color.blue)&&channel(color.alpha);
}
inline ImageFit image_fit(ImageFit value) noexcept { return value; }
inline std::optional<ImageFit> image_fit(std::optional<ImageFit> value) noexcept { return value; }


class NativeImage final {
    struct Prepared final {
        Image view;
        std::shared_ptr<const ii::gui::RasterImage> pixels;
        std::optional<double> relative_padding;
        ImageRasterSource raster_source;
        std::optional<WidgetColor> tint;
    };
    using Snapshot = std::shared_ptr<const Prepared>;
    struct State final {
        Snapshot current;
        const std::thread::id owner=std::this_thread::get_id();
        std::uint64_t revision=0;
    };
    ii::gui::Widget root_;
    std::shared_ptr<State> state_;

    NativeImage(ii::gui::Widget root, std::shared_ptr<State> state)
        : root_(std::move(root)), state_(std::move(state)) {}

    [[nodiscard]] static ii::core::Result<Snapshot> prepare(const ImageInput& input) {
        if(input.relative_padding&&(!std::isfinite(*input.relative_padding)||*input.relative_padding<0||*input.relative_padding>.5))
            return ii::core::fail(ii::core::ErrorCode::invalid_argument, "Invalid Image relative padding");
        if(input.image&&input.raster_source)
            return ii::core::fail(ii::core::ErrorCode::invalid_argument, "Conflicting Image raster sources");
        if(input.tint&&!valid_image_tint(*input.tint))
            return ii::core::fail(ii::core::ErrorCode::invalid_argument, "Invalid Image tint");
        auto next = std::make_shared<Prepared>();
        std::optional<ImageSize> size;
        if (input.image) size = ImageSize{double(input.image->width()), double(input.image->height())};
        // A deferred source is present even though its dimensions are known
        // only at paint time. The real dimensions are validated when supplied.
        else if(input.raster_source) size = ImageSize{1,1};
        if (!next->view.set_configuration(input.configuration, size))
            return ii::core::fail(ii::core::ErrorCode::invalid_argument, "Invalid Image configuration");
        next->pixels = input.image;
        next->relative_padding = input.relative_padding;
        next->raster_source = input.raster_source;
        next->tint = input.tint;
        return Snapshot(std::move(next));
    }
    [[nodiscard]] static ii::gui::AccessibilitySemantics semantics(const Snapshot& snapshot) {
        ii::gui::AccessibilitySemantics value{};
        value.role = ii::gui::AccessibilityRole::image;
        value.name = snapshot->view.configuration().accessibility_name;
        return value;
    }
    [[nodiscard]] static ii::core::Result<std::shared_ptr<const ii::gui::WidgetDrawing>>
    paint(const Snapshot& snapshot, const ii::gui::WidgetPaintState& state) {
        auto pixels = snapshot->pixels;
        const Image* view = &snapshot->view;
        std::optional<Image> supplied_view;
        if(snapshot->raster_source){
            auto supplied=snapshot->raster_source(state.size,state.backing_scale);
            if(!supplied)return std::unexpected(supplied.error());
            pixels=std::move(*supplied);
            const auto size=pixels?std::optional<ImageSize>{ImageSize{double(pixels->width()),double(pixels->height())}}:std::nullopt;
            supplied_view=snapshot->view;
            if(!supplied_view->set_configuration(snapshot->view.configuration(),size))
                return ii::core::fail(ii::core::ErrorCode::invalid_argument, "Invalid Image supplied raster");
            view=&*supplied_view;
        }
        const auto geometry = view->geometry(state.size.width, state.size.height, snapshot->relative_padding);
        if (!geometry) return ii::core::fail(ii::core::ErrorCode::invalid_argument, "Invalid Image extent");
        const auto rectangle = [](ImageRect value) {
            return ii::gui::Rect{float(value.x), float(value.y), float(value.width), float(value.height)};
        };
        const auto linear = [](WidgetColor value) {
            const auto channel = [](double c) { return c <= .04045 ? c / 12.92 : std::pow((c + .055) / 1.055, 2.4); };
            return ii::gui::Color{float(channel(value.red)), float(channel(value.green)),
                                  float(channel(value.blue)), float(value.alpha)};
        };
        auto drawing = std::make_shared<ii::gui::WidgetDrawing>();
        const auto& style = view->configuration();
        if (geometry->outer.width > 0 && geometry->outer.height > 0) {
            if (style.background) {
                const auto color = linear(*style.background);
                drawing->panels.push_back({rectangle(geometry->outer), color, color, float(geometry->radius)});
            }
            if (style.border && geometry->border_width > 0) {
                const auto color = linear(*style.border);
                drawing->panels.push_back({rectangle(geometry->outer), color, color, float(geometry->radius),
                                           float(geometry->border_width)});
            }
        }
        if (geometry->draw_image && pixels) {
            ii::gui::ImageRun run{};
            run.bounds = rectangle(geometry->image); run.image = std::move(pixels);
            run.source = rectangle(geometry->source); run.radius = float(geometry->image_radius);
            if(snapshot->tint)run.tint=linear(*snapshot->tint);
            if (geometry->border_on_image) {
                run.border = linear(*style.border);
                run.border_width = float(geometry->border_width);
            }
            drawing->images.push_back(std::move(run));
        }
        return std::shared_ptr<const ii::gui::WidgetDrawing>(std::move(drawing));
    }
public:
    [[nodiscard]] static ii::core::Result<NativeImage> create(ii::gui::WidgetBuilder& builder,
            ii::gui::Widget parent, std::string_view id, const ImageInput& input) {
        try {
            auto prepared = prepare(input);
            if (!prepared) return std::unexpected(prepared.error());
            auto state = std::make_shared<State>();
            state->current = std::move(*prepared);
            const auto& style = state->current->view.configuration();
            auto root = builder.absolute_stack(parent, id, {style.width, style.height});
            if (!root) return std::unexpected(root.error());
            if (auto accessible = root->set_accessibility_semantics(semantics(state->current)); !accessible)
                return std::unexpected(accessible.error());
            if (auto installed = root->set_paint_handler([state](const ii::gui::WidgetPaintState& paint_state) {
                    return paint(state->current, paint_state);
                }); !installed) return std::unexpected(installed.error());
            return NativeImage(*root, std::move(state));
        } catch (...) {
            return ii::core::fail(ii::core::ErrorCode::failed, "Image construction failed", std::current_exception());
        }
    }

    [[nodiscard]] ii::gui::Widget root() const { return root_; }
    [[nodiscard]] ii::core::Result<void> set_options(const ImageInput& input) const {
        try {
            if (state_->owner != std::this_thread::get_id())
                return ii::core::fail(ii::core::ErrorCode::wrong_executor, "Image requires its owner thread");
            auto prepared = prepare(input);
            if (!prepared) return std::unexpected(prepared.error());
            if (state_->revision == (std::numeric_limits<std::uint64_t>::max)())
                return ii::core::fail(ii::core::ErrorCode::out_of_range, "Image revision exhausted");
            const auto revision = ++state_->revision;
            if (auto current = root_.snapshot(); !current) return std::unexpected(current.error());
            if (state_->revision != revision) return {};
            const auto& style = (*prepared)->view.configuration();
            if (auto resized = root_.set_extent({style.width, style.height}); !resized) return resized;
            if (state_->revision != revision) return {};
            if (auto accessible = root_.set_accessibility_semantics(semantics(*prepared)); !accessible) return accessible;
            if (state_->revision != revision) return {};
            state_->current = std::move(*prepared);
            return root_.request_repaint();
        } catch (...) {
            return ii::core::fail(ii::core::ErrorCode::failed, "Image update failed", std::current_exception());
        }
    }
};
} // namespace ii::gui
