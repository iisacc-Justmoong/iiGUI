#pragma once

#include <iiGUI/NativeView.h>
#include <exception>
#include <memory>
#include <stdexcept>
#include <utility>

namespace ii::gui {

// The source owns only the factory. Each call creates a fresh widget generation;
// the builder and generated view retain their own activation and resources.
class CompiledViewSource final : public ii::gui::NativeViewSource {
public:
    explicit CompiledViewSource(ii::gui::WidgetFactory factory) : factory_(std::move(factory)) {
        if (!factory_) throw std::invalid_argument("A compiled View source requires a factory");
    }
    [[nodiscard]] ii::core::Result<void> build(ii::gui::WidgetBuilder& widgets) const override {
        try { return factory_(widgets); }
        catch (...) { return ii::core::fail(ii::core::ErrorCode::failed, "Compiled View construction failed", std::current_exception()); }
    }
private:
    ii::gui::WidgetFactory factory_;
};

[[nodiscard]] inline std::shared_ptr<ii::gui::NativeViewSource> compiled_view_source(ii::gui::WidgetFactory factory) {
    return std::make_shared<CompiledViewSource>(std::move(factory));
}

template<class GeneratedView>
[[nodiscard]] std::shared_ptr<ii::gui::NativeViewSource> compiled_view_source() {
    return compiled_view_source([](ii::gui::WidgetBuilder& widgets) { return GeneratedView::build(widgets); });
}

} // namespace ii::gui
