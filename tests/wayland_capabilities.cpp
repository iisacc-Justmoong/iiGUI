#include <iiGUI/Host.h>
#include <cstdio>
int main() {
    const auto capability=ii::gui::host_capabilities();
    if(capability.backend!=ii::gui::HostBackend::wayland || !capability.secondary_windows ||
       !ii::gui::supports_application_driver() || ii::gui::supports_application_foreground()) {
        std::fprintf(stderr,"Wayland does not advertise its independent window driver\n");
        return 1;
    }
}
