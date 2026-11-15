#pragma once
#include <iiGUI/AlertButtonModel.h>
#include <iiGUI/NativeButtonView.h>
#include <cstdint>
#include <exception>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace ii::gui {
struct AlertButtonCallbacks final {
    std::function<void(const ButtonEvent&)> method;
    std::function<void()> clicked,pressed,released,canceled;
    std::vector<ButtonMethod> methods;
};
struct DefaultAlertButtonHostPolicy final {
    using Model=AlertButtonModel;using Options=AlertButtonOptions;using ButtonInput=ii::gui::ButtonInput;
    using Button=NativeButton;using Callbacks=AlertButtonCallbacks;using Method=ButtonMethod;
};
template<class Policy>
class BasicNativeAlertButton final {
    using AlertButton=typename Policy::Model;
    using AlertButtonConfiguration=typename Policy::Options;
    using ButtonInput=typename Policy::ButtonInput;
    using NativeButton=typename Policy::Button;
    using NativeAlertButtonCallbacks=typename Policy::Callbacks;
    using ButtonMethod=typename Policy::Method;
    struct State final {
        AlertButton model;
        std::optional<NativeButton> button;
        NativeAlertButtonCallbacks callbacks;
        const std::thread::id owner = std::this_thread::get_id();
        std::uint64_t revision = 0;
        bool live = false;
    };

    explicit BasicNativeAlertButton(std::shared_ptr<State> state) : state_(std::move(state)) {}

    [[nodiscard]] static ButtonInput button_input(const AlertButton& model) {
        ButtonInput result;
        result.configuration = model.appearance().button;
        return result;
    }

    [[nodiscard]] static ii::core::Result<void> check(const std::shared_ptr<State>& state) {
        if (state->owner != std::this_thread::get_id()) {
            return ii::core::fail(ii::core::ErrorCode::wrong_executor, "AlertButton requires its owner thread");
        }
        if (auto current = state->button->root().snapshot(); !current) return std::unexpected(current.error());
        return {};
    }

public:
    [[nodiscard]] static ii::core::Result<BasicNativeAlertButton> create(
            ii::gui::WidgetBuilder& builder, ii::gui::Widget parent, std::string_view id,
            AlertButtonConfiguration configuration = {}, NativeAlertButtonCallbacks callbacks = {}) {
        try {
            auto state = std::make_shared<State>();
            if (!state->model.set_configuration(std::move(configuration))) {
                return ii::core::fail(ii::core::ErrorCode::invalid_argument, "Invalid AlertButton configuration");
            }
            state->callbacks = std::move(callbacks);
            std::weak_ptr<State> weak = state;
            auto action = [weak] {
                auto state = weak.lock();
                if (!state || !state->live) return;
                auto source = state->button->root();
                auto current = source.snapshot();
                if (!current || !current->enabled || !current->visible || current->input_blocked) return;
                auto clicked = state->callbacks.clicked;
                if (clicked) clicked();
            };
            auto button = NativeButton::create(builder, parent, id, button_input(state->model), std::move(action));
            if (!button) return std::unexpected(button.error());
            state->button = *button;
            if (auto installed = button->set_method_registry(state->callbacks.method, state->callbacks.methods); !installed) {
                return std::unexpected(installed.error());
            }
            const auto event = [weak](auto member) {
                return [weak, member] {
                    if (auto state = weak.lock(); state && state->live) {
                        auto callback = state->callbacks.*member;
                        if (callback) callback();
                    }
                };
            };
            if (auto installed = button->set_events({.pressed = event(&NativeAlertButtonCallbacks::pressed),
                    .released = event(&NativeAlertButtonCallbacks::released),
                    .canceled = event(&NativeAlertButtonCallbacks::canceled)}); !installed) {
                return std::unexpected(installed.error());
            }
            if (auto retained = builder.retain(state); !retained) return std::unexpected(retained.error());
            if (auto published = builder.on_publish([weak] { if (auto state = weak.lock()) state->live = true; }); !published) {
                return std::unexpected(published.error());
            }
            if (auto retired = builder.on_retire([weak] { if (auto state = weak.lock()) state->live = false; }); !retired) {
                return std::unexpected(retired.error());
            }
            return BasicNativeAlertButton(std::move(state));
        } catch (...) {
            return ii::core::fail(ii::core::ErrorCode::failed, "AlertButton construction failed", std::current_exception());
        }
    }

    [[nodiscard]] ii::gui::Widget root() const { return state_->button->root(); }

    [[nodiscard]] ii::core::Result<AlertButtonConfiguration> options() const {
        if (auto valid = check(state_); !valid) return std::unexpected(valid.error());
        try { return state_->model.configuration(); }
        catch (...) { return ii::core::fail(ii::core::ErrorCode::failed, "AlertButton options failed", std::current_exception()); }
    }

    [[nodiscard]] ii::core::Result<void> set_options(AlertButtonConfiguration value) const {
        if (auto valid = check(state_); !valid) return valid;
        try {
            if (state_->model.configuration() == value) return {};
            AlertButton next;
            if (!next.set_configuration(std::move(value))) {
                return ii::core::fail(ii::core::ErrorCode::invalid_argument, "Invalid AlertButton configuration");
            }
            if (state_->revision == (std::numeric_limits<std::uint64_t>::max)()) {
                return ii::core::fail(ii::core::ErrorCode::out_of_range, "AlertButton revision exhausted");
            }
            auto previous = state_->model;
            const auto revision = ++state_->revision;
            state_->model = std::move(next);
            if (auto updated = state_->button->set_options(button_input(state_->model)); !updated) {
                if (state_->revision == revision) state_->model = std::move(previous);
                return updated;
            }
            return {};
        } catch (...) {
            return ii::core::fail(ii::core::ErrorCode::failed, "AlertButton update failed", std::current_exception());
        }
    }

    [[nodiscard]] ii::core::Result<void> set_callbacks(NativeAlertButtonCallbacks callbacks) const {
        if (auto valid = check(state_); !valid) return valid;
        try {
            if (auto installed=state_->button->set_method_registry(callbacks.method,callbacks.methods);!installed)return installed;
            state_->callbacks = std::move(callbacks);return {};
        } catch (...) { return ii::core::fail(ii::core::ErrorCode::failed,"AlertButton callbacks failed",std::current_exception()); }
    }

    [[nodiscard]] ii::core::Result<bool> has_methods() const {
        if (auto valid=check(state_);!valid)return std::unexpected(valid.error());return state_->button->has_methods();
    }
    [[nodiscard]] ii::core::Result<void> set_method(ButtonMethod method) const {
        if (auto valid=check(state_);!valid)return valid;
        try {
            if(auto result=state_->button->set_method(method);!result)return result;
            state_->callbacks.method=std::move(method);return {};
        } catch (...) { return ii::core::fail(ii::core::ErrorCode::failed,"AlertButton method failed",std::current_exception()); }
    }
    [[nodiscard]] ii::core::Result<void> set_methods(std::vector<ButtonMethod> methods) const {
        if (auto valid=check(state_);!valid)return valid;
        try {
            if(auto result=state_->button->set_methods(methods);!result)return result;
            state_->callbacks.methods=std::move(methods);return {};
        } catch (...) { return ii::core::fail(ii::core::ErrorCode::failed,"AlertButton methods failed",std::current_exception()); }
    }
    [[nodiscard]] ii::core::Result<void> invoke_methods(std::string_view trigger="clicked") const {
        if(auto valid=check(state_);!valid)return valid;return state_->button->invoke_methods(trigger);
    }

private:
    std::shared_ptr<State> state_;
};

using AlertButtonView=BasicNativeAlertButton<DefaultAlertButtonHostPolicy>;
} // namespace ii::gui
