#pragma once
#include "NativeWidgets.h"
#include <atomic>
namespace ii::gui::detail {
struct WebSurfaceParent final {Size size;double scale=1;bool visible=true;std::atomic<std::int32_t>* wake=nullptr;};
enum class WebSurfaceEventKind { resize,input,restore,lost };
struct WebSurfaceEvent {WebSurfaceEventKind kind;const WidgetInput* input=nullptr;};
// JS bridges only control the single browser canvas and OS requests. Widget
// layout, hit testing, paint order and callbacks remain in compiled C++.
void web_wake(std::atomic<std::int32_t>&)noexcept;
}
