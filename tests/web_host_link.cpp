#include <iiGUI/Host.h>

// Build and link this consumer only. It is not launched by the test suite.
int main() {
    ii::gui::Host host;
    return host.run({}) ? 0 : 1;
}
