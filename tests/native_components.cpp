#include "NativeComponentsRuntime.generated.h"
#include "NativeComponentRetirementRuntime.generated.h"
#include "../src/iiGUI/WidgetState.h"
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
void conditional_retirement() {
    using View = lvui_generated::NativeComponentRetirementRuntime;
    NativeFixture::constructions = NativeFixture::destructions = 0;
    auto state = std::make_shared<gui::detail::WidgetState>([](runtime::Task task) { task(); });
    auto host = take(state->content_host({}, "root", {320, 240}));
    std::shared_ptr<View::Scope> scope;
    good(host.set_content([&](gui::WidgetBuilder& builder) { return View::build(builder, {}, {}, &scope); }));
    gui::Widget conditional;
    for (const auto& node : take(state->snapshots()))
        if (node.kind == gui::WidgetKind::content_host && node.id != "root") conditional = take(state->find(node.id));
    check(!conditional.is_null(), "A component condition did not create its content host");
    auto text = [&](std::string_view id) { return take(take(conditional.content_item(id)).snapshot()).text; };
    const auto old_action = take(conditional.content_item("Borrowed@0/action"));
    check(NativeFixture::constructions == 2, "A conditional component created more than its owned local backend");
    scope->backend_0.ready.set(false);
    check(NativeFixture::destructions == 1 && !old_action.activate() && scope->active(),
          "Retiring a child destroyed its borrowed parent or admitted a stale callback");
    scope->backend_0.count.set(9);
    scope->backend_0.title.set("Remounted");
    scope->backend_0.ready.set(true);
    check(NativeFixture::constructions == 3 && text("Borrowed@0/count") == "9" &&
          text("Borrowed@0/initial") == "9" && text("Borrowed@0/content@1/content") == "Remounted",
          "A remounted component did not activate a fresh local snapshot with the same borrowed parent");
    good(take(conditional.content_item("Borrowed@0/action")).activate());
    check(scope->backend_0.count.get() == 10 && text("Borrowed@0/count") == "10",
          "A remounted component's method or dependency was disconnected");
    NativeFixture::retirement_returns = 0;
    NativeFixture::retire_callback = [weak = std::weak_ptr(scope)] {
        if (auto owner = weak.lock()) owner->backend_0.ready.set(false);
    };
    const auto retire = take(conditional.content_item("Borrowed@0/retire"));
    good(retire.activate());
    NativeFixture::retire_callback = {};
    check(NativeFixture::retirement_returns == 1 && NativeFixture::destructions == 2 && scope->active() && !retire.activate(),
          "A component removed by its own method destroyed its active receiver or admitted another call");
    state->close(); scope.reset();
    check(NativeFixture::destructions == 3, "Conditional component retirement leaked its owned instances");
}
}
int main() {
    try {
        using View = lvui_generated::NativeComponentsRuntime;
        auto state = std::make_shared<gui::detail::WidgetState>([](runtime::Task task) { task(); });
        auto host = take(state->content_host({}, "root", {640, 480}));
        std::shared_ptr<View::Scope> scope;
        good(host.set_content([&](gui::WidgetBuilder& builder) { return View::build(builder, {}, {}, &scope); }));
        auto item = [&](std::string_view id) { return take(host.content_item(id)); };
        auto text = [&](std::string_view id) { return take(item(id).snapshot()).text; };
        const auto action = item("Outer@0/Inner@1/action");
        const auto second_action = item("Inner@3/action");
        check(NativeFixture::constructions == 5, "Borrowing a component parameter copied or reconstructed a backend");
        scope->backend_0.title.set("Definition");
        scope->backend_1.title.set("Caller");
        check(text("Outer@0/Inner@1/title") == "Caller" && text("Outer@0/Inner@1/content@2/body") == "Caller" &&
              text("Inner@3/title") == "Prefix" && text("Inner@3/content@4/defaultBody") == "Definition",
              "A component argument, slot or default lost its lexical owner");
        good(action.activate()); good(second_action.activate());
        check(scope->backend_1.count.get() == 4 && scope->backend_0.count.get() == 2,
              "A component method did not use the single borrowed instance");
        check(text("Outer@0/Inner@1/count") == "8" && text("Inner@3/count") == "8" &&
              text("Outer@0/Inner@1/frozen") == "2" && text("Inner@3/frozen") == "2",
              "A typed reactive parameter or static snapshot did not retain its contract");
        scope->value_prefix.set("Changed");
        scope->backend_1.title.set("Updated");
        check(text("Inner@3/title") == "Changed" && text("Outer@0/Inner@1/title") == "Updated" &&
              text("Outer@0/Inner@1/content@2/body") == "Updated" && NativeFixture::constructions == 5,
              "A parameter update rebuilt the component or lost its dependency");
        std::weak_ptr<View::Scope> retired = scope;
        state->close();
        check(!scope->active() && !action.activate(), "A retired component admitted a callback");
        scope.reset();
        check(retired.expired() && NativeFixture::destructions == 5, "A component slot retained its retired owners");
        conditional_retirement();
        std::cout << "AOT typed components: borrowed backend, reactive inputs, named slots, defaults and retirement passed\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
