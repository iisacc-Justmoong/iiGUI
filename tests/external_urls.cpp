#include "../src/iiGUI/DesktopCommon.h"
#include <iostream>
int main(){
    using ii::gui::detail::valid_external_url;
    if(!valid_external_url("society://account")||!valid_external_url("https://iisacc.com/Account"))return 1;
    for(auto value:{"file:///etc/passwd","javascript:alert(1)","society://account@foreign.invalid","society://account?token=secret","society://other","society://account#fragment","society://account\n"})
        if(valid_external_url(value))return 2;
    std::cout<<"EXTERNAL_URL_POLICY_OK\n";
}
