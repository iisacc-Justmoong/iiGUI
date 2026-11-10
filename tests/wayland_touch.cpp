#include "../src/Platform/WaylandTouch.h"

#include <limits>
#include <stdexcept>

using namespace ii::gui;
using namespace ii::gui::detail;
namespace {
void check(bool value) { if (!value) throw std::runtime_error("Wayland touch contract failed"); }
template<class F> void rejects(F action) {
    bool rejected = false;
    try { action(); } catch (const std::exception&) { rejected = true; }
    check(rejected);
}
}

int main() {
    WaylandTouchState state;
    auto down = state.down(42, 10.5, 20.25, 1234);
    check(down && down->kind == WidgetInputKind::pointer_down && down->device == PointerDevice::touch &&
          down->device_id == 42 && down->x == 10.5 && down->y == 20.25 && down->timestamp == 1.234 && down->buttons == 1);
    auto move = state.motion(42, 15, 22, 1500);
    check(move && move->kind == WidgetInputKind::pointer_move && move->device_id == 42 && move->buttons == 1);
    check(!state.motion(7, 1, 2, 1501));
    auto up = state.up(42, 1600);
    check(up && up->kind == WidgetInputKind::pointer_up && up->x == 15 && up->y == 22 && up->buttons == 0);
    check(!state.up(42, 1601) && !state.cancel());

    check(bool(state.down(1, 0, 0, 1)));
    auto canceled = state.down(2, 3, 4, 2);
    check(canceled && canceled->kind == WidgetInputKind::pointer_cancel && canceled->device_id == 1);
    check(!state.motion(1, 1, 1, 3) && !state.up(2, 4) && !state.motion(1, 2, 2, 5));
    check(!state.up(1, 6));
    check(bool(state.down(3, 5, 6, 7)));
    auto capability_removed = state.cancel();
    check(capability_removed && capability_removed->kind == WidgetInputKind::pointer_cancel && capability_removed->device_id == 3);
    check(!state.motion(3, 0, 0, 8));

    rejects([&] { (void)state.down(4, std::numeric_limits<double>::infinity(), 0, 9); });
    check(bool(state.down(4, 0, 0, 9)));
    rejects([&] { (void)state.down(4, 0, 0, 10); });
    check(bool(state.cancel()));
    check(bool(state.down(0, 1, 2, 1)));
    check(bool(state.down(1, 1, 2, 2)));
    for (int id = 2; id < 32; ++id) check(!state.down(id, 1, 2, 3));
    rejects([&] { (void)state.down(32, 1, 2, 4); });
    check(!state.cancel());
    check(bool(state.down(32, 1, 2, 5)) && bool(state.cancel()));
}
