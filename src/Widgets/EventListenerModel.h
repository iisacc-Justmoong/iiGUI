#pragma once
#include <cmath>

namespace ii::gui {
struct EventListenerOptions final {
    double width=640,height=480;
    bool size_to_content=false;
    friend bool operator==(const EventListenerOptions&,const EventListenerOptions&)=default;
};
// Event delivery and extent validation are independent of any declaration language.
class EventListenerModel final {
    EventListenerOptions configuration_;
public:
    [[nodiscard]] const EventListenerOptions& configuration()const noexcept{return configuration_;}
    [[nodiscard]] static bool validate(const EventListenerOptions& value)noexcept {
        return std::isfinite(value.width)&&std::isfinite(value.height)&&value.width>=0&&value.height>=0&&value.width<=8192&&value.height<=8192;
    }
    bool set_configuration(EventListenerOptions value)noexcept {
        if(!validate(value))return false;configuration_=value;return true;
    }
};
} // namespace ii::gui
