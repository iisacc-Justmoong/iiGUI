#pragma once
#include "NativeWidgets.h"
#include <stdexcept>
namespace ii::gui::detail {
// Browser callbacks may arrive while an Asyncify wait is suspended. Copy them
// into bounded C++ storage and invoke application code only after that wait
// returns. Consecutive motion is coalesced without dropping an input edge.
class HostInputQueue final {
    std::vector<std::optional<WidgetInput>> slots_;
    std::size_t head_=0,count_=0;
public:
    explicit HostInputQueue(std::size_t capacity=512):slots_(capacity){if(!capacity)throw std::invalid_argument("Input queue capacity is zero");}
    bool push(WidgetInput value){
        if(count_&&value.kind==WidgetInputKind::pointer_move){auto& previous=*slots_[(head_+count_-1)%slots_.size()];if(previous.kind==value.kind&&previous.device==value.device&&previous.device_id==value.device_id){previous=std::move(value);return true;}}
        if(count_==slots_.size())return false;slots_[(head_+count_)%slots_.size()]=std::move(value);++count_;return true;
    }
    std::optional<WidgetInput> pop(){if(!count_)return {};auto value=std::move(slots_[head_]);slots_[head_].reset();head_=(head_+1)%slots_.size();--count_;return value;}
    bool empty()const noexcept{return !count_;}
};
}
