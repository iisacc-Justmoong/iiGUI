#include "NativeScalarInputsRuntime.generated.h"
#include "NativeScalarForwardRuntime.generated.h"
#include "NativeScalarTypesRuntime.generated.h"
#include "NativeScalarConditionalRuntime.generated.h"
#include "NativeScalarRootRuntime.generated.h"
#include "NativeScalarFunctionDefaultRuntime.generated.h"
#include "../src/iiGUI/WidgetState.h"
#include <iostream>
#include <stdexcept>
using namespace ii;
void check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
template<class T> T take(core::Result<T> value) {
    if (!value) { if (value.error().cause) std::rethrow_exception(value.error().cause); throw std::runtime_error(value.error().message); }
    return std::move(*value);
}
void good(core::Result<void> value) {
    if (!value) { if (value.error().cause) std::rethrow_exception(value.error().cause); throw std::runtime_error(value.error().message); }
}
template<class View> struct Mounted final {
    std::shared_ptr<gui::detail::WidgetState> state = std::make_shared<gui::detail::WidgetState>([](runtime::Task task) { task(); });
    gui::Widget host = take(state->content_host({}, "root", {640,480}));
    std::shared_ptr<typename View::Scope> scope;
    Mounted() { good(host.set_content([&](gui::WidgetBuilder& builder) { return View::build(builder, {}, {}, &scope); })); }
    ~Mounted() { state->close(); scope.reset(); }
    std::string text(std::string_view suffix) {
        for (const auto& node : take(state->snapshots())) if (node.id.ends_with(suffix)) return node.text;
        throw std::runtime_error("Missing scalar input widget");
    }
};
using Pair = lvui_generated::NativeScalarInputsRuntime;
void sharing() {
    Mounted<Pair> m;
    check(m.scope->backend_0.calls == 1 && m.text("/first") == "2#1" && m.text("/second") == "2#1" &&
          m.text("/fallback") == "2#1" && m.text("/pinned") == "2#1", "Widget consumers did not share one C++ result");
    (void)take(m.state->snapshots()); (void)take(m.state->snapshots());
    check(m.scope->backend_0.calls == 1 && m.scope->backend_0.file_revision.get() == 0,
          "View inspection evaluated a backend method or mutated file state");
}
void updates() {
    Mounted<Pair> m;
    auto& model = m.scope->backend_0;
    model.count.set(5);
    check(model.calls == 2 && m.text("/first") == "5#2" && m.text("/second") == "5#2" &&
          m.text("/fallback") == "5#2" && m.text("/pinned") == "2#1", "One source update produced different input versions");
    model.visible.set(false);
    check(model.calls == 2 && model.file_revision.get() == 0, "An unrelated view state re-evaluated the input or changed the file");
}
void capture() {
    Mounted<lvui_generated::NativeScalarForwardRuntime> m;
    check(m.scope->backend_0.calls == 1 && m.scope->backend_1.calls == 1 && m.text("/other") == "2#1" &&
          m.text("/title") == "Explicit" && m.text("/fallback") == "Explicit" && m.text("/definition") == "2#1",
          "Caller capture, prior input defaults or definition defaults were shadowed");
    m.scope->backend_1.count.set(7);
    check(m.scope->backend_1.calls == 2 && m.scope->backend_0.calls == 1 && m.text("/other") == "7#2" && m.text("/definition") == "2#1",
          "A forwarded scalar input lost its original backend dependency");
    m.scope->backend_0.count.set(9);
    check(m.scope->backend_0.calls == 2 && m.text("/definition") == "9#2", "A default input lost its definition environment");
}
void types() {
    Mounted<lvui_generated::NativeScalarTypesRuntime> m;
    check(m.text("/integer") == "2" && m.text("/float") == "0.25" && m.text("/double") == "0.5" &&
          m.text("/metric") == "12" && m.text("/boolean") == "true" && m.text("/red") == "0.25", "Typed scalar input values changed");
    m.scope->backend_0.count.set(8); m.scope->backend_0.amount.set(.5); m.scope->backend_0.size.set(24);
    m.scope->backend_0.visible.set(false); m.scope->backend_0.ink.set({.75,.25,.5,1});
    check(m.text("/integer") == "8" && m.text("/float") == "0.5" && m.text("/double") == "1" &&
          m.text("/metric") == "24" && m.text("/boolean") == "false" && m.text("/red") == "0.75", "Typed inputs or Color members did not update");
}
void conditional() {
    Mounted<lvui_generated::NativeScalarConditionalRuntime> m;
    auto& model = m.scope->backend_0;
    check(model.calls == 1 && m.text("/first") == "2#1" && m.text("/second") == "2#1", "Conditional activation duplicated a scalar input");
    model.visible.set(false); model.count.set(7);
    check(model.calls == 1, "A removed conditional child kept evaluating its scalar input");
    model.visible.set(true);
    check(model.calls == 2 && m.text("/first") == "7#2" && m.text("/second") == "7#2" && m.text("/pinned") == "7#2",
          "Reactivation did not create one fresh input and Static snapshot");
}
void errors() {
    Mounted<Pair> m;
    auto& model = m.scope->backend_0;
    model.fail = true; model.count.set(6);
    check(model.calls == 2 && m.text("/first") == "2#1" && m.text("/second") == "2#1", "Failed input discarded its valid display");
    model.fail = false; model.count.set(7);
    check(model.calls == 3 && m.text("/first") == "7#3" && m.text("/second") == "7#3", "Failed input did not recover consistently");
    auto candidate = m.host.set_content([&](gui::WidgetBuilder& builder) {
        return Pair::build(builder, {}, [](auto& scope) { scope.backend_0.fail = true; });
    });
    check(!candidate && m.text("/first") == "7#3" && m.text("/second") == "7#3" && NativeScalarInputModel::live == 1,
          "Initial input failure replaced the live view or retained the failed activation");
}
void lifetime() {
    check(NativeScalarInputModel::live == 0, "Previous scalar input owner leaked");
    { Mounted<Pair> m; auto owner = m.scope;
      m.state->close(); owner->backend_0.count.set(9);
      check(owner->backend_0.calls == 1 && !owner->active(), "A closed widget graph continued evaluating its inputs"); }
    check(NativeScalarInputModel::live == 0, "Scalar input storage retained its backend activation");
}
void root() {
    Mounted<lvui_generated::NativeScalarRootRuntime> m;
    check(m.scope->backend_0.calls == 1 && m.text("/first") == "2#1" && m.text("/second") == "2#1",
          "Window forwarding or a View sequence duplicated its input");
    m.scope->backend_0.count.set(8);
    check(m.scope->backend_0.calls == 2 && m.text("/first") == "8#2" && m.text("/second") == "8#2", "Root input forwarding lost its shared value");
}
void function_default() {
    Mounted<lvui_generated::NativeScalarFunctionDefaultRuntime> m;
    auto& model = m.scope->backend_0;
    check(model.formatter_factories == 1 && model.formatter_calls == 1 && m.text("/first") == "2:25#1" && m.text("/second") == "2:25#1",
          "A function-valued prior input could not provide one scalar default");
    model.count.set(3);
    check(model.formatter_factories == 1 && model.formatter_calls == 2 && m.text("/first") == "3:25#2" && m.text("/second") == "3:25#2",
          "A default recomputed its function factory or invoked it per consumer");
    model.amount.set(.5);
    check(model.formatter_factories == 2 && model.formatter_calls == 3 && m.text("/first") == "3:50#3" && m.text("/second") == "3:50#3",
          "A default did not follow a new function version");
}
int main(int argc, char** argv) {
    try {
        std::string which = argc > 1 ? argv[1] : "sharing";
        if(which=="sharing")sharing();else if(which=="updates")updates();else if(which=="capture")capture();else if(which=="types")types();
        else if(which=="conditional")conditional();else if(which=="errors")errors();else if(which=="lifetime")lifetime();else if(which=="root")root();
        else if(which=="function_default")function_default();else throw std::runtime_error("Unknown scalar input test");
        std::cout << "AOT scalar input " << which << " passed\n";
    } catch(const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
