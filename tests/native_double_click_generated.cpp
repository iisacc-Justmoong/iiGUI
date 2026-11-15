#include "NativeDoubleClickRuntime.generated.h"
#include "../src/Widgets/WidgetState.h"
#include <cstdio>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
void check(bool value, const char *message) { if (!value) throw std::runtime_error(message); }
void good(ii::core::Result<void> value) { if (!value) throw std::runtime_error(value.error().message); }
template<class T> T take(ii::core::Result<T> value) {
    if (!value) throw std::runtime_error(value.error().message);
    return std::move(*value);
}
void exercise() {
    using namespace ii;
    auto state = std::make_shared<gui::detail::WidgetState>([](runtime::Task task) { task(); });
    auto host = take(state->content_host({}, "root", {320, 240}));
    std::shared_ptr<lvui_generated::NativeDoubleClickRuntime::Scope> scope;
    good(host.set_content([&](gui::WidgetBuilder &builder) {
        return lvui_generated::NativeDoubleClickRuntime::build(builder, {}, [](auto &) {}, &scope);
    }));
    auto list = take(host.content_item("results"));
    auto scroll = take(host.content_item("results/viewport"));
    (void)take(scroll.snapshot());
    (void)take(state->drawing_snapshot({320, 240}));
    const auto row = take(take(list.keyed_content_item("0", "row")).snapshot()).bounds;
    const auto press = [&](double time, std::uint32_t count) {
        good(state->input({.kind=gui::WidgetInputKind::pointer_down,.x=row.x+5,.y=row.y+5,.timestamp=time,.buttons=1,.native_click_count=count}));
        good(state->input({.kind=gui::WidgetInputKind::pointer_up,.x=row.x+5,.y=row.y+5,.timestamp=time+.01}));
    };
    press(1, 1); press(1.1, 2);
    check(scope->backend_0.activations == std::vector<std::string>{"0"}, "generated LVUI view did not activate the row");
    scope->backend_0.select_on_click.set(false);
    press(2, 1); press(2.1, 2);
    check(scope->backend_0.activations.size() == 1, "reactively disabled double click still activated");
    scope->backend_0.select_on_click.set(true);
    press(3, 1); press(3.1, 2);
    check(scope->backend_0.activations == std::vector<std::string>({"0", "0"}), "reactively re-enabled double click failed");
    check(scope->backend_0.file_revision.get() == 0, "navigation modified the file revision");
    std::weak_ptr<lvui_generated::NativeDoubleClickRuntime::Scope> retired = scope;
    NativeNavigationBackend::on_activate = [&] { state->close(); };
    press(4, 1); press(4.1, 2); scope.reset();
    check(retired.expired() && !list.snapshot(), "activation retained a retired view");
}
}
int main() { try { exercise(); } catch (const std::exception &error) { std::fprintf(stderr, "%s\n", error.what()); return 1; } }
