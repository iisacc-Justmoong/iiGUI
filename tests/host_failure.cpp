#include "../src/Platform/HostFailure.h"
#include <memory>
#include <stdexcept>
#include <string>

namespace {
void check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
using ii::core::Error;
using ii::core::ErrorCode;

void first_failure_and_reentry() {
    ii::gui::detail::HostFailure failure;
    auto token = std::make_shared<int>(42);
    std::weak_ptr<int> weak = token;
    int calls = 0;
    bool delivery_valid = false;
    const auto cause = std::make_exception_ptr(std::runtime_error("original exception"));
    failure.set_handler([&, keep = token](const Error& error) {
        ++calls;
        failure.set_handler({});
        failure = Error{ErrorCode::failed, "nested failure"};
        delivery_valid = !weak.expired() && *keep == 42 && error.code == ErrorCode::full &&
                         error.message == "first" && error.cause == cause;
    });
    token.reset();
    failure = Error{ErrorCode::full, "first", cause};
    failure = Error{ErrorCode::closed, "later"};
    check(calls == 1 && delivery_valid && failure && failure->code == ErrorCode::full && failure->message == "first" && weak.expired(),
          "Host failure was repeated, replaced, or retained its callback capture");
}

void throwing_handler_preserves_error() {
    ii::gui::detail::HostFailure failure;
    int calls = 0;
    failure.set_handler([&](const Error&) { ++calls; throw std::runtime_error("handler failed"); });
    failure = Error{ErrorCode::invalid_argument, "native error"};
    failure = Error{ErrorCode::failed, "second"};
    check(calls == 1 && failure && failure->code == ErrorCode::invalid_argument && failure->message == "native error",
          "A throwing handler replaced or repeated the native error");
}
} // namespace

int main() { first_failure_and_reentry(); throwing_handler_preserves_error(); }
