#include <iiGUI/Host.h>
#include <iostream>
int main() {
    if(ii::gui::HostCapabilities{}.backend!=ii::gui::HostBackend::none)return 5;
    const auto c=ii::gui::host_capabilities();
    if(!c.clipboard_text||!c.external_urls) return 1;
    if(c.backend==ii::gui::HostBackend::web && (c.secondary_windows||!c.display_frames)) return 2;
    if(c.backend==ii::gui::HostBackend::wayland && (!c.file_dialogs||!c.display_frames||!c.text_input)) return 3;
    if(c.backend==ii::gui::HostBackend::android && (!c.display_frames||c.file_dialogs||c.shaped_text)) return 4;
    std::cout<<"HOST_CONTRACT_OK backend="<<int(c.backend)<<'\n';
}
