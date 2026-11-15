#include "NativeSingleClickRuntime.generated.h"
#include "../src/Widgets/WidgetState.h"
#include <cstdio>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
void check(bool value, const char *message) { if (!value) throw std::runtime_error(message); }
void good(ii::core::Result<void> value) { if (!value) throw std::runtime_error(value.error().message); }
template<class T> T take(ii::core::Result<T> value) {
    if (!value) throw std::runtime_error(value.error().message); return std::move(*value);
}
void exercise() {
    using namespace ii;
    auto state = std::make_shared<gui::detail::WidgetState>([](runtime::Task task) { task(); });
    auto host = take(state->content_host({}, "root", {320, 240}));
    std::shared_ptr<lvui_generated::NativeSingleClickRuntime::Scope> scope;
    good(host.set_content([&](gui::WidgetBuilder &builder) {
        return lvui_generated::NativeSingleClickRuntime::build(builder, {}, [](auto &) {}, &scope);
    }));
    auto list = take(host.content_item("results"));
    (void)take(take(host.content_item("results/viewport")).snapshot());
    (void)take(state->drawing_snapshot({320, 240}));
    const auto row = take(take(list.keyed_content_item("0", "row")).snapshot()).bounds;
    const auto press = [&](double time) {
        good(state->input({.kind=gui::WidgetInputKind::pointer_down,.x=row.x+5,.y=row.y+5,.timestamp=time,.buttons=1,.native_click_count=1}));
        good(state->input({.kind=gui::WidgetInputKind::pointer_up,.x=row.x+5,.y=row.y+5,.timestamp=time+.01}));
    };
    press(1); check(scope->backend_0.activations == std::vector<std::string>{"0"}, "LVUI single click did not activate");
    scope->backend_0.select_on_click.set(false); press(2);
    check(scope->backend_0.activations.size() == 1, "reactively disabled activation fired");
    scope->backend_0.select_on_click.set(true); press(3);
    check(scope->backend_0.activations == std::vector<std::string>({"0", "0"}), "reactively re-enabled activation failed");
    check(scope->backend_0.file_revision.get() == 0, "view input changed the file revision");
    std::weak_ptr<lvui_generated::NativeSingleClickRuntime::Scope> retired = scope;
    NativeNavigationBackend::on_activate = [&] { state->close(); };
    press(4); scope.reset();
    check(retired.expired() && !list.snapshot(), "activation retained a retired view");
}
}
int main() { try { exercise(); } catch (const std::exception &error) { std::fprintf(stderr, "%s\n", error.what()); return 1; } }
