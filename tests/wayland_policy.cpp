#include "iiGUI/WaylandPolicy.h"
#include <iostream>
int main(){
    using namespace ii::gui;using namespace ii::gui::detail;
    if(wayland_configure_size({640,420},0,0)!=Size{640,420})return 1;
    if(wayland_configure_size({640,420},900,0)!=Size{900,420})return 2;
    if(wayland_buffer_scale(2)!=2)return 3;
    if(!full_surface_canvas({.viewport={0,0,640,420}},{640,420})||full_surface_canvas({.viewport={1,0,640,420}},{640,420})||full_surface_canvas({.viewport={0,0,320,420}},{640,420}))return 7;
    for(auto size:{-1,32768}){try{(void)wayland_configure_size({640,420},size,0);return 4;}catch(const std::out_of_range&){}}
    for(auto scale:{0,9}){try{(void)wayland_buffer_scale(scale);return 5;}catch(const std::out_of_range&){}}
    try{(void)wayland_configure_size({0,420},0,0);return 6;}catch(const std::out_of_range&){}
    std::cout<<"WAYLAND_POLICY_OK\n";
}
