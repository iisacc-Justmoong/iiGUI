#pragma once
#include <iiGUI/Export.h>
#include <iiSDKCore/Core/Result/Result.h>
#include <iiSDKCore/Runtime/Query.h>
#include <memory>

namespace ii::gui {
struct MotionState final {
    bool enabled=true,reduced_motion=false;
    double speed=1;
    std::uint64_t revision=1;
    [[nodiscard]] bool animated()const noexcept{return enabled&&!reduced_motion;}
};
using MotionChangePort=runtime::QueryPort<MotionState>;
// Owner-thread preferences shared by native controls. Observers are weak Core
// query receivers; no timer, process singleton or GUI toolkit is involved.
class IIGUI_EXPORT MotionPreferences final {
public:
    MotionPreferences();
    ~MotionPreferences();
    MotionPreferences(const MotionPreferences&)=delete;
    MotionPreferences& operator=(const MotionPreferences&)=delete;
    [[nodiscard]] core::Result<MotionState> state()const;
    [[nodiscard]] core::Result<runtime::Clock::duration> duration(runtime::Clock::duration)const;
    [[nodiscard]] core::Result<void> set_enabled(bool);
    [[nodiscard]] core::Result<void> set_reduced_motion(bool);
    [[nodiscard]] core::Result<void> set_speed(double);
    [[nodiscard]] core::Result<void> observe(std::shared_ptr<const MotionChangePort>);
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
