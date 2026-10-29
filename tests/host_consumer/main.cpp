#include <iiGUI/iiGUI.h>
#include <iostream>
int main(){
    const auto capabilities=ii::gui::host_capabilities();
    // Construction must not open a desktop window; linking still pulls the
    // entire platform host and verifies its private native dependencies.
    ii::gui::Host host({.title="Installed host contract"});
    if(!host.is_ui_thread()||!capabilities.clipboard_text||!capabilities.external_urls)return 1;
    const auto snapshot=host.snapshot();if(snapshot||snapshot.error().code!=ii::core::ErrorCode::closed)return 2;
    std::cout<<"INSTALLED_HOST_CONTRACT_OK backend="<<int(capabilities.backend)<<'\n';
}
