#include "../src/Platform/WindowForeground.h"
#include "../src/Platform/X11Foreground.h"
#include <stdexcept>
#include <vector>

namespace {
using Tracker = ii::gui::detail::WindowForeground;
using WindowState = ii::gui::detail::X11WindowForeground;
void check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }

XEvent map_event(Window receiver, Window target) {
    XEvent event{}; event.xmap.type = MapNotify; event.xmap.event = receiver; event.xmap.window = target; return event;
}
XEvent unmap_event(Window receiver, Window target) {
    XEvent event{}; event.xunmap.type = UnmapNotify; event.xunmap.event = receiver; event.xunmap.window = target; return event;
}

void mapped_windows_and_unknown_initial_state() {
    Tracker tracker; WindowState a, b;
    int first = 0, second = 0;
    constexpr Window first_window = 101, second_window = 202;
    std::vector<bool> delivered;
    tracker.set_handler([&](bool value) { delivered.push_back(value); });
    tracker.open(); tracker.deliver();
    check(delivered.empty(), "A driver without a mapped window invented a state");
    check(!a.apply(first_window, map_event(second_window, first_window), tracker, &first), "Another receiver affected the window");
    check(!a.apply(first_window, map_event(first_window, second_window), tracker, &first), "Another target affected the window");
    tracker.deliver(); check(delivered.empty(), "Unrelated events produced a state");
    check(a.apply(first_window, map_event(first_window, first_window), tracker, &first), "First map was ignored");
    check(b.apply(second_window, map_event(second_window, second_window), tracker, &second), "Second map was ignored");
    tracker.deliver(); check(delivered == std::vector<bool>{true}, "Mapped windows did not enter foreground");
    check(!a.apply(first_window, map_event(first_window, first_window), tracker, &first), "Duplicate map changed state");
    XEvent unrelated{}; unrelated.type = FocusOut;
    check(!a.apply(first_window, unrelated, tracker, &first), "Focus changed mapped state");
    unrelated.type = VisibilityNotify;
    check(!a.apply(first_window, unrelated, tracker, &first), "Occlusion changed mapped state");
    check(a.apply(first_window, unmap_event(first_window, first_window), tracker, &first), "First unmap was ignored");
    tracker.deliver(); check(delivered.size() == 1, "One unmapped window hid another mapped window");
    check(b.apply(second_window, unmap_event(second_window, second_window), tracker, &second), "Second unmap was ignored");
    tracker.deliver(); check(delivered == std::vector<bool>({true, false}), "All unmapped windows did not enter background");
    check(a.apply(first_window, map_event(first_window, first_window), tracker, &first), "Remap was ignored");
    tracker.deliver(); check(delivered == std::vector<bool>({true, false, true}), "Remap did not enter foreground");
    a.retire(tracker, &first); tracker.deliver();
    check(delivered == std::vector<bool>({true, false, true, false}), "Closing the last mapped window did not enter background");
    check(!a.apply(first_window, map_event(first_window, first_window), tracker, &first), "Retired window accepted a late event");
    b.retire(tracker, &second); tracker.close();
}

void pre_ready_and_reentrant_delivery() {
    Tracker tracker; WindowState window; int identity = 0; constexpr Window native = 303;
    std::vector<bool> delivered;
    tracker.set_handler([&](bool value) {
        delivered.push_back(value);
        if (!value) window.apply(native, map_event(native, native), tracker, &identity);
    });
    check(window.apply(native, map_event(native, native), tracker, &identity), "Initial map was ignored");
    check(window.apply(native, unmap_event(native, native), tracker, &identity), "Initial unmap was ignored");
    tracker.open(); tracker.deliver();
    check(delivered == std::vector<bool>({false, true}), "Pre-ready coalescing or reentrant ordering failed");
    window.retire(tracker, &identity); tracker.close();
}

void first_observed_unmap_is_known_background() {
    Tracker tracker; WindowState window; int identity = 0; constexpr Window native = 404;
    std::vector<bool> delivered;
    tracker.set_handler([&](bool value) { delivered.push_back(value); });
    tracker.open(); tracker.deliver();
    check(delivered.empty(), "Unknown state was delivered before the first event");
    check(window.apply(native, unmap_event(native, native), tracker, &identity), "Known initial unmap was ignored");
    tracker.deliver(); check(delivered == std::vector<bool>{false}, "Initial unmap did not enter background");
    window.retire(tracker, &identity); tracker.close();
}

void retired_before_first_event() {
    Tracker tracker; WindowState window; int identity = 0; constexpr Window native = 505;
    std::vector<bool> delivered;
    tracker.set_handler([&](bool value) { delivered.push_back(value); });
    window.retire(tracker, &identity);
    check(!window.apply(native, map_event(native, native), tracker, &identity), "A failed window accepted a late event");
    tracker.open(); tracker.deliver();
    check(delivered.empty(), "Retiring before the first event invented a state");
    tracker.close();
}
}

int main() { mapped_windows_and_unknown_initial_state(); pre_ready_and_reentrant_delivery(); first_observed_unmap_is_known_background(); retired_before_first_event(); }
