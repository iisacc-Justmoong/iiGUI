#include "../src/Widgets/WidgetState.h"
#include "../src/Widgets/WidgetSurface.h"

#include <iostream>
#include <memory>
#include <stdexcept>

using namespace ii::gui;
using namespace ii::gui::detail;
namespace {
void check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
void good(ii::core::Result<void> result) { if (!result) throw std::runtime_error(result.error().message); }

struct Observed final { int invalidations = 0, flushes = 0, destroyed = 0; };
struct Surface final : WidgetSurface {
    explicit Surface(std::shared_ptr<Observed> value) : state(std::move(value)) {}
    ~Surface() override { ++state->destroyed; }
    void invalidate() override { ++state->invalidations; }
    void accessibility_changed() override {}
    void accessibility_value_changed(std::string_view) override {}
    void set_frame_paced(bool) override {}
    void flush() override { ++state->flushes; }
    Size size() const override { return {200, 100}; }
    double backing_scale() const override { return 1; }
    std::string resolve_font_family(std::string_view value) const override { return std::string(value); }
    GraphicsInfo info() override { return {}; }
    PixelBuffer capture() override { return {}; }
    bool native_event(const void*) override { return false; }
#if defined(_WIN32)
    std::optional<std::intptr_t> native_result(const void*) override { return {}; }
#endif
    std::shared_ptr<Observed> state;
};
}
int main() {
    try {
        auto observed=std::make_shared<Observed>();
        auto tree=std::make_shared<WidgetState>([](ii::runtime::Task task){task();});
        good(tree->construct([](WidgetBuilder& b) -> ii::core::Result<void> {
            auto root=b.vstack({},"root"); if(!root)return std::unexpected(root.error());
            auto label=b.label(*root,"label",{.text="Before"});
            return label ? ii::core::Result<void>{} : std::unexpected(label.error());
        }));
        int calls=0;
        good(tree->attach(nullptr, [&](void*, WidgetSurfaceCallbacks callbacks) -> std::unique_ptr<WidgetSurface> {
            check(bool(callbacks.frame) && bool(callbacks.input), "Surface received no frame or input callback");
            ++calls; return std::make_unique<Surface>(observed);
        }));
        check(calls==1 && observed->flushes==1, "Explicit surface factory was not used exactly once");
        auto label=tree->find("label"); check(bool(label), "Label missing");
        good(label->set_text("After")); check(observed->invalidations>0, "Surface did not receive invalidation");
        tree->close(); check(observed->destroyed==1, "Closing the tree did not destroy its surface");
        return 0;
    } catch(const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
