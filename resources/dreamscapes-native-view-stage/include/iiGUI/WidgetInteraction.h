#pragma once
#include <iiGUI/Graphics.h>
#include <iiGUI/WindowGeometry.h>
#include <iiSDKCore/Runtime/Query.h>
#include <functional>

namespace ii::gui {
enum class WidgetPressOrigin { none, pointer, keyboard };
// Small owner-thread metadata in absolute native-surface coordinates. No text,
// document, paint provider or native control is copied to observe interaction.
struct IIGUI_EXPORT WidgetInteraction final {
    Rect bounds,clip;
    Size surface,implicit_size;
    bool alive=true,visible=false,enabled=false,hovered=false,pressed=false,focused=false,opened=false;
    bool input_blocked=false;
    [[nodiscard]] bool contains(double x,double y)const noexcept;
    friend bool operator==(const WidgetInteraction&,const WidgetInteraction&)=default;
};
using WidgetInteractionPort=runtime::QueryPort<WidgetInteraction>;
// One control-owned receiver per widget; independent of external QueryPort subscriptions.
using WidgetInteractionHandler=std::function<void(const WidgetInteraction&)>;
} // namespace ii::gui
