#include <iiGUI/WidgetSurfaceNative.h>
#include <iiGUI/WaylandConnection.h>
#include <vector>

namespace {std::vector<int> observed_input;int resize_events=0;std::function<void(const ii::gui::WidgetInput&)> input_hook;}
void fake_wayland_reset_resize_events(){resize_events=0;}
int fake_wayland_resize_events(){return resize_events;}
void fake_wayland_clear_input_log(){observed_input.clear();}
std::vector<int> fake_wayland_input_log(){return observed_input;}
void fake_wayland_input_hook(std::function<void(const ii::gui::WidgetInput&)> value){input_hook=std::move(value);}

namespace ii::gui::detail {
struct NativeWidgetSurface::Impl final {
    WaylandConnection* connection;
    WidgetSurfaceCallbacks callbacks;
    Impl(WaylandConnection* value,WidgetSurfaceCallbacks handlers):connection(value),callbacks(std::move(handlers)){}
};
NativeWidgetSurface::NativeWidgetSurface(void* parent,WidgetSurfaceCallbacks callbacks)
    :impl_(std::make_shared<Impl>(static_cast<WaylandConnection*>(parent),std::move(callbacks))){}
NativeWidgetSurface::~NativeWidgetSurface()=default;
void NativeWidgetSurface::invalidate(){}
void NativeWidgetSurface::accessibility_changed(){}
void NativeWidgetSurface::accessibility_value_changed(std::string_view){}
void NativeWidgetSurface::set_frame_paced(bool){}
void NativeWidgetSurface::flush(){}
Size NativeWidgetSurface::size()const{return impl_->connection->size();}
double NativeWidgetSurface::backing_scale()const{return impl_->connection->scale();}
std::string NativeWidgetSurface::resolve_font_family(std::string_view value)const{return std::string(value);}
GraphicsInfo NativeWidgetSurface::info(){return {};}
PixelBuffer NativeWidgetSurface::capture(){return {};}
bool NativeWidgetSurface::native_event(const void* raw){const auto& event=*static_cast<const WaylandSurfaceEvent*>(raw);
    if(event.kind==WaylandEventKind::resize)++resize_events;
    if(event.kind==WaylandEventKind::input&&event.input){if(input_hook)input_hook(*event.input);if(event.input->text=="1")observed_input.push_back(1);else if(event.input->text=="3")observed_input.push_back(3);}
    if(event.kind==WaylandEventKind::ime&&event.ime)observed_input.push_back(event.ime->kind==WaylandImeEventKind::enter?2:4);
    return true;}
}
