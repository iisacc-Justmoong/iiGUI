#pragma once
#include <iiGUI/EventListenerModel.h>
#include <iiGUI/NativeWidgets.h>
#include <iiSDKCore/Core/Text/Utf8.h>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <exception>
#include <functional>
#include <limits>
#include <memory>
#include <string_view>
#include <thread>
#include <utility>

namespace ii::gui {
struct EventListenerCallbacks final {
    std::function<ii::core::Result<bool>(const ii::gui::WidgetInput&)> input;
    std::function<void(ii::gui::Rect)> resized;
};
class NativeEventListener final {
    static constexpr std::size_t maximum_deferred_inputs=256;
    struct PendingInput final {
        ii::gui::WidgetInput input;
        std::uint64_t callbacks_revision;
    };
    struct State final {
        EventListenerModel view;
        ii::gui::Widget root;
        EventListenerCallbacks callbacks;
        std::function<void(ii::gui::Rect)> layout;
        const std::thread::id owner=std::this_thread::get_id();
        std::uint64_t options_revision=0,callbacks_revision=0;
        std::deque<PendingInput> pending_inputs;
        std::uint64_t overflow_revision=0;
        bool live=false,retired=false,dispatching=false,input_overflow=false;
    };
    std::shared_ptr<State> state_;
    explicit NativeEventListener(std::shared_ptr<State> state):state_(std::move(state)){}
    struct Guard final {
        State& state;
        explicit Guard(State& value):state(value){state.dispatching=true;}
        ~Guard(){state.pending_inputs.clear();state.input_overflow=false;state.dispatching=false;}
    };
    [[nodiscard]] static bool valid_text(std::string_view value)noexcept {
        return value.size()<=65536&&value.find('\0')==std::string_view::npos&&ii::core::valid_utf8(value);
    }
    [[nodiscard]] static bool valid_input(const ii::gui::WidgetInput& value)noexcept {
#if defined(IIGUI_POINTER_CANCEL_API)
        constexpr auto last_input_kind=ii::gui::WidgetInputKind::pointer_cancel;
#elif defined(IIGUI_TEXT_SELECTION_API)
        constexpr auto last_input_kind=ii::gui::WidgetInputKind::text_selection;
#elif defined(IIGUI_KEY_CANCEL_API)
        constexpr auto last_input_kind=ii::gui::WidgetInputKind::key_cancel;
#else
        constexpr auto last_input_kind=ii::gui::WidgetInputKind::commit_composition;
#endif
        return unsigned(value.kind)<=unsigned(last_input_kind)&&unsigned(value.key)<=unsigned(ii::gui::WidgetKey::redo)&&
            unsigned(value.device)<=unsigned(ii::gui::PointerDevice::touch)&&std::isfinite(value.x)&&std::isfinite(value.y)&&
            std::isfinite(value.scroll_x)&&std::isfinite(value.scroll_y)&&std::isfinite(value.pressure)&&value.pressure>=0&&value.pressure<=1&&
            std::isfinite(value.tilt_x)&&std::abs(value.tilt_x)<=90&&std::isfinite(value.tilt_y)&&std::abs(value.tilt_y)<=90&&
            std::isfinite(value.rotation)&&std::isfinite(value.timestamp)&&value.timestamp>=0&&
            valid_text(value.text)&&valid_text(value.target);
    }
    [[nodiscard]] static ii::core::Result<bool> forward(const std::shared_ptr<State>& state,const ii::gui::WidgetInput& value){
        if(!state->live)return ii::core::fail(ii::core::ErrorCode::closed,"Event listener has retired");
        if(state->dispatching)return ii::core::fail(ii::core::ErrorCode::busy,"Event listener is already dispatching");
        if(!valid_input(value))return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid event listener input");
        Guard guard(*state);
        try {
            auto callback=state->callbacks.input;
            ii::core::Result<bool> result=callback?callback(value):ii::core::Result<bool>{false};
            std::size_t delivered=0;
            while(state->live&&!state->input_overflow&&!state->pending_inputs.empty()&&delivered<maximum_deferred_inputs){
                auto pending=std::move(state->pending_inputs.front());state->pending_inputs.pop_front();++delivered;
                if(pending.callbacks_revision!=state->callbacks_revision)continue;
                auto next=state->callbacks.input;
                auto delivered_result=next?next(pending.input):ii::core::Result<bool>{false};
                if(result&&!delivered_result)result=std::unexpected(delivered_result.error());
            }
            if(state->live&&(state->input_overflow||!state->pending_inputs.empty())){
                const auto revision=state->input_overflow?state->overflow_revision:state->callbacks_revision;
                state->pending_inputs.clear();state->input_overflow=true;
                if(revision==state->callbacks_revision){
                    auto cancel=state->callbacks.input;
                    auto canceled=cancel?cancel({.kind=ii::gui::WidgetInputKind::focus_lost}):ii::core::Result<bool>{false};
                    if(result&&!canceled)result=std::unexpected(canceled.error());
                }
            }
            return result;
        }
        catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Event listener callback failed",std::current_exception());}
    }
    // Native capture cancellation and nested event loops can reenter the
    // listener while the current callback is still on the stack. Returning
    // busy here would propagate through iiGUI as a fatal host error.
    [[nodiscard]] static ii::core::Result<bool> host_input(const std::shared_ptr<State>& state,const ii::gui::WidgetInput& value){
        if(!state->live)return false;
        if(!valid_input(value))return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid event listener input");
        if(!state->dispatching)return forward(state,value);
        try {
            if(state->input_overflow)return true;
            if(state->pending_inputs.size()>=maximum_deferred_inputs){
                state->pending_inputs.clear();state->overflow_revision=state->callbacks_revision;state->input_overflow=true;
            }else state->pending_inputs.push_back({value,state->callbacks_revision});
            return true;
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Cannot defer event listener input",std::current_exception());}
    }
public:
    [[nodiscard]] static ii::core::Result<NativeEventListener> create(ii::gui::WidgetBuilder& builder,ii::gui::Widget parent,
            std::string_view id,EventListenerOptions value={},EventListenerCallbacks callbacks={}){
        if(!EventListenerModel::validate(value))return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid event listener extent");
        try {
            auto state=std::make_shared<State>();(void)state->view.set_configuration(value);state->callbacks=std::move(callbacks);
            auto root=value.size_to_content
                ?builder.zstack(parent,id,{.spacing=0,.alignment=ii::gui::StackAlignment::leading,.intrinsic_width=true})
                :builder.absolute_stack(parent,id,{value.width,value.height});
            if(!root)return std::unexpected(root.error());state->root=*root;
            if(value.size_to_content)if(auto resized=root->set_extent({value.width,value.height});!resized)
                return std::unexpected(resized.error());
            if(auto retained=builder.retain(state);!retained)return std::unexpected(retained.error());std::weak_ptr<State> weak=state;
            if(auto installed=root->set_input_handler([weak](const ii::gui::WidgetInput& input)->ii::core::Result<bool>{
                if(auto state=weak.lock();state&&state->live)return host_input(state,input);return false;
            });!installed)return std::unexpected(installed.error());
            state->layout=[weak](ii::gui::Rect bounds){if(auto state=weak.lock();state&&state->live){auto callback=state->callbacks.resized;if(callback)callback(bounds);}};
            if(auto installed=root->set_layout_handler(state->layout);!installed)return std::unexpected(installed.error());
            // A candidate can be measured before it becomes visible. Resetting
            // layout delivery on publication makes the first live size observable.
            if(auto registered=builder.on_publish([weak]{if(auto state=weak.lock();state&&!state->retired){
                state->live=true;(void)state->root.set_layout_handler(state->layout);
            }});!registered)return std::unexpected(registered.error());
            if(auto registered=builder.on_retire([weak]{if(auto state=weak.lock()){
                state->retired=true;state->live=false;state->pending_inputs.clear();state->input_overflow=false;state->callbacks={};
            }});!registered)return std::unexpected(registered.error());
            return NativeEventListener(std::move(state));
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Event listener construction failed",std::current_exception());}
    }
    [[nodiscard]] ii::gui::Widget root()const{return state_->root;}
    [[nodiscard]] ii::core::Result<void> set_options(EventListenerOptions value)const {
        if(!EventListenerModel::validate(value))return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid event listener extent");
        if(state_->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"Event listener requires its owner thread");
        if(value.size_to_content!=state_->view.configuration().size_to_content)
            return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Event listener sizing mode cannot change after creation");
        if(state_->options_revision==(std::numeric_limits<std::uint64_t>::max)())return ii::core::fail(ii::core::ErrorCode::out_of_range,"Event listener options revision exhausted");
        const auto revision=++state_->options_revision;
        if(auto current=state_->root.snapshot();!current)return std::unexpected(current.error());
        if(state_->options_revision!=revision)return {};
        if(auto resized=state_->root.set_extent({value.width,value.height});!resized)return resized;
        if(state_->options_revision!=revision)return {};
        (void)state_->view.set_configuration(value);return {};
    }
    [[nodiscard]] ii::core::Result<void> set_callbacks(EventListenerCallbacks value)const {
        if(state_->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"Event listener requires its owner thread");
        if(state_->callbacks_revision==(std::numeric_limits<std::uint64_t>::max)())return ii::core::fail(ii::core::ErrorCode::out_of_range,"Event listener callbacks revision exhausted");
        const auto revision=++state_->callbacks_revision;
        if(auto current=state_->root.snapshot();!current)return std::unexpected(current.error());
        if(state_->callbacks_revision!=revision)return {};
        std::swap(state_->callbacks,value);state_->pending_inputs.clear();state_->input_overflow=false;return {};
    }
    [[nodiscard]] ii::core::Result<bool> dispatch(const ii::gui::WidgetInput& value)const {
        if(state_->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"Event listener requires its owner thread");
        if(state_->dispatching)return ii::core::fail(ii::core::ErrorCode::busy,"Event listener is already dispatching");
        if(auto current=state_->root.snapshot();!current)return std::unexpected(current.error());return forward(state_,value);
    }
};
} // namespace ii::gui
