#pragma once
#include <iiGUI/NativeWidgets.h>
#include "WebTextInput.h"
#include <atomic>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

namespace ii::gui::detail {
[[nodiscard]] inline std::optional<WidgetInput> web_accessibility_input(std::string_view id,int action){
    if(action<0||action>8)return std::nullopt;
    constexpr WidgetKey keys[]{WidgetKey::none,WidgetKey::none,WidgetKey::left,WidgetKey::right,WidgetKey::up,WidgetKey::down,WidgetKey::home,WidgetKey::end,WidgetKey::none};
    WidgetInput input;input.kind=action==0?WidgetInputKind::focus:action==1?WidgetInputKind::activate:action==8?WidgetInputKind::focus_lost:WidgetInputKind::key_down;
    input.key=keys[action];if(action!=8)input.target=id;return input;
}
enum class WebHostEventKind { input, resize, visibility, context_lost, context_restored, text_input };
struct WebHostEvent final {
    WebHostEventKind kind=WebHostEventKind::input;
    WidgetInput input;
    bool hidden=false;
    std::uint32_t accessibility_generation=0;
    std::optional<WebTextMessage> text;
};

// The browser thread only copies events here. The owner releases the lock
// before invoking any widgets or browser APIs, so neither thread waits for a
// browser operation while holding this lock.
class WebHostEvents final {
    std::mutex mutex_;
    std::vector<std::optional<WebHostEvent>> slots_;
    std::size_t head_=0, count_=0;
    std::atomic<bool> overflow_=false;
public:
    explicit WebHostEvents(std::size_t capacity=512):slots_(capacity) {
        if (!capacity) throw std::invalid_argument("Web event queue capacity is zero");
    }
    void push(WebHostEvent event) noexcept {
        try {
            std::lock_guard lock(mutex_);
            if (count_) {
                auto& last=*slots_[(head_+count_-1)%slots_.size()];
                const bool resize=last.kind==WebHostEventKind::resize && event.kind==last.kind;
                const bool motion=last.kind==WebHostEventKind::input && event.kind==last.kind &&
                    last.input.kind==WidgetInputKind::pointer_move && event.input.kind==last.input.kind &&
                    event.input.device==last.input.device && event.input.device_id==last.input.device_id;
                if (resize || motion) { last=std::move(event);return; }
            }
            if (count_==slots_.size()) { overflow_.store(true,std::memory_order_release);return; }
            slots_[(head_+count_)%slots_.size()]=std::move(event);++count_;
        } catch (...) { overflow_.store(true,std::memory_order_release); }
    }
    std::optional<WebHostEvent> pop() {
        std::lock_guard lock(mutex_);
        if (!count_) return {};
        auto event=std::move(slots_[head_]);slots_[head_].reset();
        head_=(head_+1)%slots_.size();--count_;return event;
    }
    bool take_overflow() noexcept { return overflow_.exchange(false,std::memory_order_acq_rel); }
};
}
