#include "../src/Platform/WaylandConnectionRegistry.h"
#include <cstdio>
#include <stdexcept>

namespace {
void check(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
struct Connection { int id; };
void focus_and_retirement() {
    ii::gui::detail::WaylandConnectionRegistry<Connection> registry;
    Connection a{1}, b{2}, absent{3};
    check(!registry.current(), "An empty connection registry selected a clipboard owner");
    registry.add(&a); registry.add(&b);
    check(registry.current() == &a, "Opening another window stole clipboard focus");
    check(registry.select(&b) && registry.current() == &b, "A focused window did not become the clipboard owner");
    check(!registry.select(&absent) && registry.current() == &b, "An unknown connection stole clipboard focus");
    registry.remove(&b);
    check(registry.current() == &a, "Retiring a focused window lost the remaining clipboard connection");
    registry.remove(&b); registry.remove(&a);
    check(!registry.current(), "Retired connections remained available");
}
void retirement_prefers_the_most_recent_surviving_focus() {
    ii::gui::detail::WaylandConnectionRegistry<Connection> registry;
    Connection a{1}, b{2}, never_focused{3};
    registry.add(&a); registry.add(&b);
    check(registry.select(&b) && registry.select(&a), "Focus sequence was not accepted");
    registry.add(&never_focused);
    check(registry.current() == &a, "An unfocused new connection stole focus");
    registry.remove(&a);
    check(registry.current() == &b, "Retirement did not restore the most recently focused surviving connection");
    registry.remove(&b);
    check(registry.current() == &never_focused, "Retirement lost the last surviving connection");
}
void duplicate_and_null_are_rejected() {
    ii::gui::detail::WaylandConnectionRegistry<Connection> registry;
    Connection a{1}; registry.add(&a);
    bool duplicate = false, invalid = false;
    try { registry.add(&a); } catch (const std::invalid_argument&) { duplicate = true; }
    try { registry.add(nullptr); } catch (const std::invalid_argument&) { invalid = true; }
    check(duplicate && invalid && registry.current() == &a, "An invalid connection changed the registry");
}
} // namespace
int main() {
    try { focus_and_retirement(); retirement_prefers_the_most_recent_surviving_focus(); duplicate_and_null_are_rejected(); }
    catch (const std::exception& error) { std::fprintf(stderr,"%s\n",error.what()); return 1; }
}
