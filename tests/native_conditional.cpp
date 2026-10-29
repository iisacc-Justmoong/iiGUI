#include "NativeConditionalRuntime.generated.h"
#include "../src/iiGUI/WidgetState.h"
#include <iostream>
#include <stdexcept>
using namespace ii;
namespace {
void check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
void good(core::Result<void> result) { if (!result) { if (result.error().cause) std::rethrow_exception(result.error().cause); throw std::runtime_error(result.error().message); } }
template<class T> T take(core::Result<T> result) { if (!result) { if (result.error().cause) std::rethrow_exception(result.error().cause); throw std::runtime_error(result.error().message); } return std::move(*result); }
}
int main() {
    try {
        using View = lvui_generated::NativeConditionalRuntime;
        auto state = std::make_shared<gui::detail::WidgetState>([](runtime::Task task) { task(); });
        auto host = take(state->content_host({}, "root", {640,400}));
        std::shared_ptr<View::Scope> scope;
        good(host.set_content([&](gui::WidgetBuilder& builder) {
            return View::build(builder, {}, {}, &scope);
        }));
        check(scope && scope->active(), "Generated activation did not open its lifetime");
        gui::Widget conditional;
        for (const auto& node : take(state->snapshots()))
            if (node.kind == gui::WidgetKind::content_host && node.id != "root") conditional = take(state->find(node.id));
        check(!conditional.is_null(), "No native conditional host was built");
        const auto generation = take(conditional.snapshot()).content_generation;
        auto first = take(conditional.content_item("value"));
        auto tail = take(host.content_item("tail"));
        check(take(first.snapshot()).text == "4" && take(conditional.measure(640)).height >= 80,
              "Initial branch was empty or clipped");
        check(NativeFixture::constructions == 2 && NativeFixture::destructions == 0,
              "Inactive branch or redraw created an extra backend");
        scope->backend_0.title.set("Changed");
        scope->backend_0.ready.set(true);
        check(take(tail.snapshot()).text == "Changed" && take(conditional.snapshot()).content_generation == generation &&
              NativeFixture::constructions == 2, "Unrelated data rebuilt the conditional generation");
        scope->backend_0.ready.set(false);
        check(!first.snapshot() && NativeFixture::destructions == 1 && take(conditional.measure(640)).height >= 40,
              "Switching did not retire the previous branch");
        auto waiting = take(conditional.content_item("waiting"));
        const auto waiting_generation = take(conditional.snapshot()).content_generation;
        NativeFixture::fail_construction = true;
        scope->backend_0.ready.set(true);
        auto graph = scope->native_lifetime.lock();
        check(graph && graph->failures()->size() == 1 && waiting.snapshot() &&
              take(conditional.snapshot()).content_generation == waiting_generation,
              "A failed candidate replaced the working branch");
        scope->backend_0.ready.set(false);
        scope->backend_0.ready.set(true);
        check(graph->failures()->empty() && !waiting.snapshot() &&
              take(take(conditional.content_item("value")).snapshot()).text == "4" && NativeFixture::constructions == 3,
              "A valid condition did not recover with a fresh activation");
        std::weak_ptr<View::Scope> retired = scope;
        state->close();
        check(!scope->active() && !tail.snapshot(), "Generation retirement admitted stale UI/model work");
        scope.reset(); graph.reset();
        check(retired.expired() && NativeFixture::destructions == 3, "Retired factories or callbacks retained native backends");
        std::cout << "Generated native conditionals: switch, identity, rollback and retirement passed without a window\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
