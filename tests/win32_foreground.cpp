#include "../src/Platform/WindowForeground.h"
#include "../src/Platform/Win32Foreground.h"
#include <stdexcept>
#include <vector>

namespace {
using Tracker = ii::gui::detail::WindowForeground;
using Visibility = ii::gui::detail::Win32WindowVisibility;
void check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }

void messages_and_multiple_windows() {
    Tracker tracker;
    std::vector<bool> events;
    tracker.set_handler([&](bool value) { events.push_back(value); });
    tracker.open(); tracker.deliver();
    check(events.empty(), "A driver without windows invented a foreground state");
    int first = 0, second = 0;
    Visibility a, b;
    a.seed(true, false); b.seed(true, false);
    tracker.open_window(&first, a.visible()); tracker.open_window(&second, b.visible()); tracker.deliver();
    check(events == std::vector<bool>{true}, "Visible windows did not enter foreground");
    const auto apply = [&](const void* id, Visibility& state, UINT message, WPARAM value, LPARAM detail) {
        if (state.update(message, value, detail)) tracker.set_window_visible(id, state.visible());
        tracker.deliver();
    };
    apply(&first, a, WM_SHOWWINDOW, 0, SW_OTHERZOOM);
    apply(&second, b, WM_ACTIVATEAPP, 0, 0);
    check(events.size() == 1, "Occlusion or focus incorrectly hid the application");
    apply(&first, a, WM_SIZE, SIZE_MINIMIZED, 0);
    check(events.size() == 1, "One minimized window hid another visible window");
    apply(&second, b, WM_SIZE, SIZE_MINIMIZED, 0);
    check(events == std::vector<bool>({true, false}), "All minimized windows did not enter background");
    apply(&first, a, WM_SHOWWINDOW, 0, 0);
    apply(&first, a, WM_SIZE, SIZE_RESTORED, 0);
    check(events.size() == 2, "Restoring a hidden window made the app foreground");
    WINDOWPOS shown{}; shown.flags = SWP_SHOWWINDOW;
    apply(&first, a, WM_WINDOWPOSCHANGED, 0, reinterpret_cast<LPARAM>(&shown));
    check(events == std::vector<bool>({true, false, true}), "Position-based showing was lost");
    apply(&first, a, WM_SIZE, SIZE_MAXHIDE, 0);
    apply(&first, a, WM_SHOWWINDOW, 0, SW_OTHERZOOM);
    check(events.size() == 3, "Another maximized window altered visibility");
    WINDOWPOS hidden{}; hidden.flags = SWP_HIDEWINDOW;
    apply(&first, a, WM_WINDOWPOSCHANGED, 0, reinterpret_cast<LPARAM>(&hidden));
    check(events == std::vector<bool>({true, false, true, false}), "Hiding the last visible window was lost");
    apply(&second, b, WM_SIZE, SIZE_MAXIMIZED, 0);
    check(events == std::vector<bool>({true, false, true, false, true}), "Maximizing a shown window did not restore foreground");
    tracker.remove_window(&first); tracker.remove_window(&second); tracker.deliver();
    check(events.back() == false, "Closing the last window did not enter background");
    tracker.close();
}

void invalid_and_reentrant_events() {
    Visibility state;
    state.seed(false, false);
    WINDOWPOS conflicting{}; conflicting.flags = SWP_SHOWWINDOW | SWP_HIDEWINDOW;
    check(!state.update(WM_WINDOWPOSCHANGED, 0, reinterpret_cast<LPARAM>(&conflicting)) && !state.visible(),
          "Conflicting position flags invented a visible state");
    check(!state.update(WM_WINDOWPOSCHANGED, 0, 0), "A missing position was accepted");
    Tracker tracker;
    std::vector<bool> events;
    int window = 0;
    tracker.set_handler([&](bool value) { events.push_back(value); if (!value) tracker.set_window_visible(&window, true); });
    tracker.open_window(&window, false); tracker.open(); tracker.deliver();
    check(events == std::vector<bool>({false, true}), "Reentrant Win32 foreground delivery was reordered");
    tracker.close();
}
}

int main() { messages_and_multiple_windows(); invalid_and_reentrant_events(); }
