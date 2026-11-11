#pragma once

#include "WaylandImeEvents.h"
#include <iiGUI/NativeWidgets.h>
#include <stdexcept>
#include <variant>
#include <vector>

namespace ii::gui::detail {
struct WaylandPointerEvent final { WidgetInput input; std::uint32_t serial=0; };
using WaylandQueuedInput=std::variant<WidgetInput,WaylandImeEvent,WaylandPointerEvent>;
class WaylandInputQueue final {
    std::vector<std::optional<WaylandQueuedInput>> slots_;
    std::size_t head_=0,count_=0;
public:
    explicit WaylandInputQueue(std::size_t capacity=512):slots_(capacity){if(!capacity)throw std::invalid_argument("Wayland input queue capacity is zero");}
    bool push(WidgetInput value,std::optional<std::uint32_t> serial={}){
        if(serial) return append(WaylandPointerEvent{std::move(value),*serial});
        if(count_&&value.kind==WidgetInputKind::pointer_move){
            auto* prior=std::get_if<WidgetInput>(&*slots_[(head_+count_-1)%slots_.size()]);
            if(prior&&prior->kind==value.kind&&prior->device==value.device&&prior->device_id==value.device_id){*prior=std::move(value);return true;}
        }
        return append(std::move(value));
    }
    bool push(WaylandImeEvent value){return append(std::move(value));}
    std::optional<WaylandQueuedInput> pop(){if(!count_)return {};auto value=std::move(slots_[head_]);slots_[head_].reset();head_=(head_+1)%slots_.size();--count_;return value;}
    bool empty()const noexcept{return !count_;}
private:
    bool append(WaylandQueuedInput value){if(count_==slots_.size())return false;slots_[(head_+count_)%slots_.size()]=std::move(value);++count_;return true;}
};
} // namespace ii::gui::detail
