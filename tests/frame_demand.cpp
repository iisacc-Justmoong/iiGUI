#include "../src/Platform/FrameDemand.h"
#include <stdexcept>

namespace {
void check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
}
int main() {
    using ii::gui::detail::FrameDemand;
    FrameDemand demand;
    check(!demand.needed(), "an idle host requested frames");
    demand.foreground(true);
    check(!demand.needed(), "foreground alone requested frames");
    demand.motion(true); demand.motion(true);
    check(demand.needed(), "active motion did not request frames");
    demand.motion(false);
    check(demand.needed(), "one finished motion stopped another");
    demand.foreground(false);
    check(!demand.needed(), "background host requested frames");
    demand.foreground(true);
    check(demand.needed(), "remaining motion did not resume");
    demand.continuous(true); demand.motion(false);
    check(demand.needed(), "finishing motion stopped the explicit frame loop");
    demand.continuous(false);
    check(!demand.needed(), "last request did not retire frames");
    demand.motion(false); demand.motion(true); demand.motion(false);
    check(!demand.needed(), "unbalanced retirement changed later requests");

    // Independent window trees share one cadence: removing or finishing one
    // tree must not stop a second tree or an explicitly requested game loop.
    demand.motion(true); // primary window
    demand.motion(true); // secondary window
    demand.motion(false);
    check(demand.needed(), "retiring one window stopped another window's motion");
    demand.continuous(true);
    demand.motion(false);
    check(demand.needed(), "retiring secondary motion stopped continuous frames");
    demand.continuous(false);
    check(!demand.needed(), "window cadence survived its last frame request");
}
