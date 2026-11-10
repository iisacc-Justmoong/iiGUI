#pragma once
#include <iiGUI/Desktop.h>
#include <iiGUI/Host.h>
#include <iiGUI/NativeWidgets.h>
#include "WaylandImeEvents.h"
#include <memory>
#include <span>
struct wl_display;
struct wl_surface;
namespace ii::gui::detail {
struct WaylandCallbacks final {
    // True means the host scheduled a surface commit for this configuration.
    std::function<bool(Size,double)> configure;
    runtime::Task close,frame;
    std::function<void(WidgetInput)> input;
    std::function<void(WaylandImeEvent)> ime;
    std::function<void(bool)> activity;
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
    static void wait_many(std::span<WaylandConnection* const>, int wake_fd, int timeout);
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
    std::optional<DisplayPixels> display_pixels()const;
    core::Result<ClipboardContent> clipboard();
    core::Result<void> clipboard(const ClipboardContent&);
    std::uint64_t claim_text_input();
    void release_text_input(std::uint64_t claim);
    std::optional<std::uint64_t> text_input_focus(std::uint64_t claim)const;
    std::optional<std::uint32_t> text_input_publish(std::uint64_t claim,std::uint64_t epoch,const WaylandImePublication&);
    std::optional<std::uint32_t> text_input_disable(std::uint64_t claim,std::uint64_t epoch);
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
WaylandConnection* wayland_connection()noexcept;
enum class WaylandEventKind { resize,input,expose,ime };
struct WaylandSurfaceEvent {WaylandEventKind kind;const WidgetInput* input=nullptr;const WaylandImeEvent* ime=nullptr;};
}
