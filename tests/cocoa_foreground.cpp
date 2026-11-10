#include "../src/Platform/WindowForeground.h"
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using Tracker = ii::gui::detail::WindowForeground;
void check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }

void aggregate_windows_and_hidden_state() {
    Tracker tracker;
    std::vector<bool> events;
    tracker.set_handler([&](bool value) { events.push_back(value); });
    tracker.open();
    tracker.set_hidden(false);
    tracker.deliver();
    check(events.empty(), "An empty application driver invented an initial state");
    int first = 1, second = 2, unknown = 3;
    tracker.open_window(&first, true); tracker.open_window(&first, true); tracker.deliver();
    check(events == std::vector<bool>{true}, "The first visible window did not enter foreground");
    tracker.open_window(&second, true);
    tracker.set_window_visible(&first, false);
    tracker.set_window_visible(&unknown, true);
    tracker.deliver();
    check(events == std::vector<bool>{true}, "A minimized or unknown window changed a visible group");
    tracker.set_window_visible(&second, false); tracker.deliver();
    check(events == std::vector<bool>({true, false}), "All minimized windows did not enter background");
    tracker.set_hidden(true);
    tracker.set_window_visible(&first, true); tracker.deliver();
    check(events.size() == 2, "A hidden application became foreground");
    tracker.set_hidden(false); tracker.deliver();
    check(events == std::vector<bool>({true, false, true}), "Unhiding a visible window did not enter foreground");
    tracker.remove_window(&first); tracker.deliver();
    check(events == std::vector<bool>({true, false, true, false}), "Closing the last visible window was lost");
    tracker.remove_window(&second); tracker.deliver();
    check(events.size() == 4, "Closing an already minimized window duplicated the state");
    tracker.open_window(&first, true); tracker.deliver();
    check(events.back(), "A reopened window did not restore foreground");
    tracker.close(); tracker.set_hidden(true); tracker.open_window(&second, true); tracker.deliver();
    check(events.size() == 5, "A closed tracker delivered another state");
}

void initial_state_and_reentry() {
    Tracker tracker;
    std::vector<bool> events;
    int window = 0;
    tracker.set_handler([&](bool visible) {
        events.push_back(visible);
        if (!visible && events.size() == 1) tracker.set_hidden(false);
    });
    tracker.set_hidden(true);
    tracker.open_window(&window, true);
    tracker.set_hidden(false);
    tracker.set_hidden(true);
    tracker.open(); tracker.deliver();
    check(events == std::vector<bool>({false, true}), "Initial coalescing or reentrant ordering failed");
}

void close_and_failure_during_delivery() {
    Tracker tracker;
    int window = 0, calls = 0;
    tracker.set_handler([&](bool) { ++calls; tracker.close(); });
    tracker.open_window(&window, true); tracker.open(); tracker.deliver();
    tracker.open_window(&window, true); tracker.deliver();
    check(calls == 1, "Closing during delivery did not stop subsequent callbacks");

    Tracker failing;
    failing.set_handler([](bool) { throw std::runtime_error("foreground callback failure"); });
    failing.open_window(&window, true); failing.open();
    bool caught = false;
    try { failing.deliver(); }
    catch (const std::runtime_error& error) { caught = std::string(error.what()) == "foreground callback failure"; }
    check(caught, "A foreground callback failure was lost");
    failing.close();
}
}

int main() { aggregate_windows_and_hidden_state(); initial_state_and_reentry(); close_and_failure_during_delivery(); }
