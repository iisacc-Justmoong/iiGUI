#include "../src/Platform/HostDriverPolicy.h"
#include "../src/Platform/HostCapabilitiesPolicy.h"
#include "../src/Platform/WindowChromeGeometry.h"
#include <iiGUI/Host.h>
#include <cstdio>
#include <stdexcept>
#include <thread>

namespace {
void check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
}

int main() {
  try {
    ii::gui::require_application_driver_abi_v1();
    check(ii::gui::detail::show_native_window(false), "A normal host became hidden");
    check(!ii::gui::detail::deliver_opened_before_visible(false), "A normal host started before it became visible");
    check(!ii::gui::detail::show_native_window(true), "The application driver would display its native anchor");
    check(ii::gui::detail::deliver_opened_before_visible(true), "The hidden application driver could not start");
    const auto capability = ii::gui::host_capabilities();
    using ii::gui::HostBackend;
    for (auto backend : {HostBackend::win32, HostBackend::x11, HostBackend::wayland}) {
        const auto chrome=ii::gui::window_chrome_presentation(backend);
        check(chrome.titlebar && chrome.client_controls && !chrome.native_controls,
              "Desktop custom chrome must expose three client controls");
    }
    const auto mac_chrome=ii::gui::window_chrome_presentation(HostBackend::cocoa);
    check(mac_chrome.titlebar && mac_chrome.native_controls && !mac_chrome.client_controls,
          "macOS must retain its native window controls");
    for (auto backend : {HostBackend::uikit,HostBackend::android}) {
        const auto chrome=ii::gui::window_chrome_presentation(backend);
        check(chrome.titlebar && !chrome.native_controls && !chrome.client_controls,
              "Tablet chrome must show a titlebar without window controls");
    }
    using ii::gui::detail::WindowResizeEdge;
    using ii::gui::detail::window_resize_edge;
    check(window_resize_edge(1,1,800,600,6,6)==WindowResizeEdge::top_left &&
          window_resize_edge(799,599,800,600,6,6)==WindowResizeEdge::bottom_right &&
          window_resize_edge(400,2,800,600,6,6)==WindowResizeEdge::top &&
          window_resize_edge(2,300,800,600,6,6)==WindowResizeEdge::left &&
          window_resize_edge(400,300,800,600,6,6)==WindowResizeEdge::none &&
          window_resize_edge(-1,20,800,600,6,6)==WindowResizeEdge::none,
          "Custom chrome resize hit testing did not preserve edges and content");
    for (const auto backend : {HostBackend::cocoa, HostBackend::win32, HostBackend::x11,
                               HostBackend::wayland, HostBackend::uikit})
        check(ii::gui::detail::has_application_driver(backend), "A supported independent driver was not advertised");
    for (const auto backend : {HostBackend::android, HostBackend::web, HostBackend::none})
        check(!ii::gui::detail::has_application_driver(backend), "An unimplemented independent driver was advertised");
    const auto uikit = ii::gui::detail::host_capabilities_for(HostBackend::uikit);
    check(uikit.backend == HostBackend::uikit && uikit.secondary_windows && uikit.display_frames,
          "UIKit did not advertise its independent scene host contract");
    const auto android = ii::gui::detail::host_capabilities_for(HostBackend::android);
    check(android.shaped_text && !android.text_input && !android.accessibility,
          "Android did not distinguish editor text shaping from its unavailable IME and accessibility");
    check(ii::gui::detail::host_capabilities_for(HostBackend::wayland).accessibility&&
          ii::gui::detail::host_capabilities_for(HostBackend::x11).accessibility,
          "Linux hosts did not advertise their compiled AT-SPI transport");
    const bool expected = capability.backend == HostBackend::cocoa || capability.backend == HostBackend::win32 ||
                          capability.backend == HostBackend::x11 || capability.backend == HostBackend::wayland ||
                          capability.backend == HostBackend::uikit;
    if (capability.backend == HostBackend::cocoa) {
        ii::gui::Host external;
        bool rejected_foreign_thread = false;
        std::thread foreign([&] {
            const auto result = external.show_external();
            rejected_foreign_thread = !result && result.error().code == ii::core::ErrorCode::wrong_executor;
        });
        foreign.join();
        check(rejected_foreign_thread, "An external host was started on the wrong thread");
        const auto not_running = external.show_external();
        check(!not_running && not_running.error().code == ii::core::ErrorCode::closed,
              "An external host accepted an absent application event loop");
        ii::runtime::EventLoop borrowed_loop;
        ii::gui::HostOptions borrowed_options; borrowed_options.event_loop = &borrowed_loop;
        ii::gui::Host borrowed(borrowed_options);
        const auto borrowed_result = borrowed.show_external();
        check(!borrowed_result && borrowed_result.error().code == ii::core::ErrorCode::invalid_argument,
              "An external host accepted a borrowed Core event loop");
        ii::gui::Host lifecycle;
        check(bool(lifecycle.set_application_activity_handler([](bool) {})), "Could not set activity handler");
        const auto lifecycle_result = lifecycle.show_external();
        check(!lifecycle_result && lifecycle_result.error().code == ii::core::ErrorCode::invalid_argument,
              "An external host accepted application lifecycle ownership");
        ii::gui::HostOptions incompatible_options; incompatible_options.renderer=ii::gui::Renderer::vulkan;
        ii::gui::Host incompatible(incompatible_options); int borrowed_parent = 0;
        const auto incompatible_result = incompatible.attach_external(&borrowed_parent);
        check(!incompatible_result && incompatible_result.error().code == ii::core::ErrorCode::unsupported,
              "An embedded host accepted a renderer without transparent composition");
    }
    check(ii::gui::supports_application_driver() == expected, "The application driver capability disagrees with the native backend");
    if (expected) {
        ii::gui::Host parent;
        ii::gui::Host child;
        check(bool(child.set_application_driver(true)), "Could not configure a driver before run");
        bool wrong_thread = false;
        std::thread foreign([&] {
            const auto result = child.set_application_driver(false);
            wrong_thread = !result && result.error().code == ii::core::ErrorCode::wrong_executor;
        });
        foreign.join();
        check(wrong_thread, "A foreign thread changed application driver ownership");
        const auto shown = child.show(parent);
        check(!shown && shown.error().code == ii::core::ErrorCode::invalid_argument,
              "A driver-only host was accepted as a secondary window");
        check(bool(child.set_application_driver(false)), "Could not disable an unstarted driver");
        const auto unavailable_parent = child.show(parent);
        check(!unavailable_parent && unavailable_parent.error().code == ii::core::ErrorCode::closed,
              "Disabling the driver did not restore normal parent validation");
        ii::runtime::EventLoop loop;
        ii::runtime::FrameLoop frames(loop);
        ii::gui::HostOptions options;
        options.event_loop = &loop;
        options.frame_loop = &frames;
        ii::gui::Host paced(options);
        const auto unsupported_cadence = paced.set_application_driver(true);
        check(!unsupported_cadence && unsupported_cadence.error().code == ii::core::ErrorCode::invalid_argument,
              "A hidden driver accepted a window-bound display frame loop");
    }
  } catch (const std::exception& error) {
    std::fprintf(stderr, "%s\n", error.what());
    return 1;
  }
}
