#pragma once
#include "Graphics.h"
#include "Host.h"
#include <iiSDKCore/Runtime/Query.h>

namespace ii::gui {
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
} // namespace ii::gui
