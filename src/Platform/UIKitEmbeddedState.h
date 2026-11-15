#pragma once
#include <cmath>
#include <optional>

namespace ii::gui::detail {
struct UIKitEmbeddedMetrics final {
    double width=0,height=0,scale=1;
    bool attached=false,sceneActive=false,applicationActive=false;
};
struct UIKitEmbeddedChange final {
    bool geometry=false,activity=false,detached=false,active=false;
};
enum class UIKitPickerAdmission { ready, busy, unavailable };

// Owner-thread policy for a borrowed UIKit surface. It never schedules work or
// retains native objects; an owning adapter interprets only returned changes.
class UIKitEmbeddedState final {
public:
    [[nodiscard]] bool attach(const UIKitEmbeddedMetrics& metrics) noexcept {
        if(phase_!=Phase::created||!metrics.attached||!valid(metrics))return false;
        metrics_=metrics;phase_=Phase::live;return true;
    }
    [[nodiscard]] std::optional<UIKitEmbeddedChange> update(const UIKitEmbeddedMetrics& metrics) noexcept {
        if(phase_!=Phase::live)return {};
        if(!metrics.attached){phase_=Phase::closed;return UIKitEmbeddedChange{.detached=true};}
        if(!valid(metrics))return {};
        const bool previousActive=active(metrics_);const bool nextActive=active(metrics);
        const bool geometry=metrics.width!=metrics_.width||metrics.height!=metrics_.height||metrics.scale!=metrics_.scale;
        metrics_=metrics;
        return UIKitEmbeddedChange{.geometry=geometry,.activity=previousActive!=nextActive,.active=nextActive};
    }
    [[nodiscard]] bool close() noexcept {
        if(phase_!=Phase::live)return false;phase_=Phase::closed;return true;
    }
    [[nodiscard]] bool live()const noexcept{return phase_==Phase::live;}
    [[nodiscard]] UIKitPickerAdmission pickerAdmission(bool ownerAttached,bool modalOpen,bool transitioning)const noexcept {
        if(phase_!=Phase::live||!ownerAttached)return UIKitPickerAdmission::unavailable;
        if(!active(metrics_)||modalOpen||transitioning)return UIKitPickerAdmission::busy;
        return UIKitPickerAdmission::ready;
    }
private:
    enum class Phase {created,live,closed};
    static bool valid(const UIKitEmbeddedMetrics& metrics) noexcept {
        return std::isfinite(metrics.width)&&std::isfinite(metrics.height)&&std::isfinite(metrics.scale)&&
            metrics.width>=0&&metrics.height>=0&&metrics.width<=32767&&metrics.height<=32767&&metrics.scale>0;
    }
    static bool active(const UIKitEmbeddedMetrics& metrics) noexcept{return metrics.sceneActive&&metrics.applicationActive;}
    Phase phase_=Phase::created;
    UIKitEmbeddedMetrics metrics_;
};
}
