#pragma once
#include <iiGUI/Desktop.h>
#include <iiGUI/NativeWidgets.h>
#include <memory>
struct wl_display;
struct wl_surface;
namespace ii::gui::detail {
struct WaylandCallbacks final {
    std::function<void(Size,double)> configure;
    runtime::Task close,frame;
    std::function<void(WidgetInput)> input;
};
// One owner-thread connection. Workers only signal Host's eventfd. Protocol
// objects, clipboard offers and pending transfers retire with the connection.
class WaylandConnection final {
public:
    WaylandConnection();
    ~WaylandConnection();
    WaylandConnection(const WaylandConnection&)=delete;
    void open(const HostOptions&,WaylandCallbacks);
    void wait(int wake_fd,int timeout);
    void dispatch();
    void title(std::string_view);
    void minimum(Size);
    void resize(Size);
    void action(WindowAction);
    void request_frame();
    void cancel_frame();
    wl_display* display()const;
    wl_surface* surface()const;
    bool configured()const;
    Size size()const;
    double scale()const;
    core::Result<ClipboardContent> clipboard();
    core::Result<void> clipboard(const ClipboardContent&);
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
WaylandConnection* wayland_connection()noexcept;
enum class WaylandEventKind { resize,input,expose };
struct WaylandSurfaceEvent {WaylandEventKind kind;const WidgetInput* input=nullptr;};
}
