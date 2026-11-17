#pragma once
#include <iiGUI/NativeWidgets.h>
#include "WebTextInput.h"
#include <atomic>
#include <functional>
namespace ii::gui::detail {
struct WebSurfaceParent final {
    Size size;double scale=1;bool visible=true,accessibility_enabled=false;std::atomic<std::int32_t>* wake=nullptr;
    std::uint32_t accessibility_serial=0;
    std::atomic<std::uint32_t> accessibility_active=0;
    std::atomic<bool> accessibility_overflow=false;
    std::function<void(WidgetInput,std::uint32_t)> accessibility_post;
    std::uint32_t text_serial=0;
    std::atomic<std::uint32_t> text_active=0;
    std::atomic<bool> text_overflow=false;
    std::function<void(WebTextMessage)> text_post;
};
enum class WebSurfaceEventKind { resize,input,restore,lost,text_input };
struct WebSurfaceEvent {WebSurfaceEventKind kind;const WidgetInput* input=nullptr;const WebTextMessage* text=nullptr;};
// JS bridges only control the single browser canvas and OS requests. Widget
// layout, hit testing, paint order and callbacks remain in compiled C++.
void web_wake(std::atomic<std::int32_t>&)noexcept;
}
