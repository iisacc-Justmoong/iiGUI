#include "NativeViewInputsRuntime.generated.h"
#include "../src/Widgets/WidgetState.h"
#include <iostream>
#include <stdexcept>
using namespace ii;
namespace {
void check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
void good(core::Result<void> result) {
    if (!result) { if (result.error().cause) std::rethrow_exception(result.error().cause); throw std::runtime_error(result.error().message); }
}
template<class T> T take(core::Result<T> result) {
    if (!result) { if (result.error().cause) std::rethrow_exception(result.error().cause); throw std::runtime_error(result.error().message); }
    return std::move(*result);
}
}
int main() {
    try {
        using View = lvui_generated::NativeViewInputsRuntime;
        auto state = std::make_shared<gui::detail::WidgetState>([](runtime::Task task) { task(); });
        auto host = take(state->content_host({},"root",{640,400}));
        std::shared_ptr<View::Scope> scope;
        good(host.set_content([&](gui::WidgetBuilder& builder) { return View::build(builder,{}, {}, &scope); }));
        const auto label = take(host.content_item("heading@0/heading"));
        const auto button = take(host.content_item("action@1/action"));
        const auto supplied = take(host.content_item("content@2/supplied"));
        const auto frozen = take(host.content_item("content@2/frozen"));
        const auto captured = take(host.content_item("content@2/captured@3/captured"));
        const auto local = take(host.content_item("content@2/onlyHere@4/local"));
        const auto supplied_button = take(host.content_item("content@2/suppliedAction"));
        check(NativeFixture::constructions == 4,"View activation skipped or duplicated its lexical backend");
        scope->backend_0.title.set("Definition");
        scope->backend_0.count.set(8);
        scope->backend_1.title.set("Caller");
        check(take(label.snapshot()).text == "Caller::scope->" && take(supplied.snapshot()).text == "Caller" &&
              take(captured.snapshot()).text == "Definition" && take(local.snapshot()).text == "Caller" &&
              take(frozen.snapshot()).text == "2",
              "Supplied input or child lost its lexical receiver");
        good(button.activate()); good(supplied_button.activate());
        check(scope->backend_1.count.get() == 4 && scope->backend_0.count.get() == 8,
              "Supplied callback invoked the prototype's shadowed backend");
        scope->backend_1.title.set("Updated");
        check(take(label.snapshot()).text == "Updated::scope->" && take(supplied.snapshot()).text == "Updated" &&
              NativeFixture::constructions == 4,"Reactive argument update rebuilt or lost its activation");
        std::weak_ptr<View::Scope> retired = scope;
        state->close();
        check(!scope->active() && !button.activate(),"Retired input callback remained admitted");
        scope.reset();
        check(retired.expired() && NativeFixture::destructions == 4,"Supplied inputs or slots retained retired owners");
        std::cout << "Generated native View inputs: lexical values, callbacks, slots, updates and retirement passed\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
