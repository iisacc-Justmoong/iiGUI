#include "iiGUI/WaylandPolicy.h"
#include "../src/Graphics/NativeCanvas.h"
#include <limits>
#include <iostream>
int main(){
    using namespace ii::gui;using namespace ii::gui::detail;
    if(wayland_configure_size({640,420},0,0)!=Size{640,420})return 1;
    if(wayland_configure_size({640,420},900,0)!=Size{900,420})return 2;
    if(wayland_surface_size({800.6,600.4})!=Size{801,600})return 19;
    if(wayland_buffer_scale(2)!=2)return 3;
    WaylandScaleState display;
    if(!display.output(2)||display.scale()!=2||display.buffer_scale()!=2||display.cursor_scale()!=2)return 10;
    if(!display.preferred(150)||display.scale()!=1.25||display.buffer_scale()!=1||display.cursor_scale()!=2)return 11;
    if(display.preferred(150)||display.output(3)||display.scale()!=1.25)return 12;
    if(!display.clear_preferred()||display.scale()!=3||display.buffer_scale()!=3||display.cursor_scale()!=3)return 13;
    if(display.clear_preferred()||display.output(3))return 14;
    WaylandScaleState same_ratio;
    if(!same_ratio.output(2)||!same_ratio.preferred(240)||same_ratio.scale()!=2||same_ratio.buffer_scale()!=1)return 23;
    if(!same_ratio.clear_preferred()||same_ratio.scale()!=2||same_ratio.buffer_scale()!=2)return 24;
    if(!display.preferred(90)||display.scale()!=.75||display.cursor_scale()!=1)return 15;
    if(wayland_fractional_pixels(101,150)!=126||wayland_fractional_pixels(102,150)!=128)return 16;
    CanvasFrame canvas{.viewport={0,0,101,102}};
    if(canvas_pixels(canvas,1.25,CanvasPixelRounding::nearest)!=std::pair<std::uint32_t,std::uint32_t>{126,128})return 20;
    if(canvas_pixels(canvas,1.25,CanvasPixelRounding::outward)!=std::pair<std::uint32_t,std::uint32_t>{127,128})return 21;
    try{(void)canvas_pixels(canvas,std::numeric_limits<double>::quiet_NaN(),CanvasPixelRounding::nearest);return 22;}catch(const std::invalid_argument&){}
    for(unsigned invalid:{0u,961u}){try{display.preferred(invalid);return 17;}catch(const std::out_of_range&){}}
    if(display.scale()!=.75)return 18;
    if(!full_surface_canvas({.viewport={0,0,640,420}},{640,420})||full_surface_canvas({.viewport={1,0,640,420}},{640,420})||full_surface_canvas({.viewport={0,0,320,420}},{640,420}))return 7;
    for(auto size:{-1,32768}){try{(void)wayland_configure_size({640,420},size,0);return 4;}catch(const std::out_of_range&){}}
    for(auto scale:{0,9}){try{(void)wayland_buffer_scale(scale);return 5;}catch(const std::out_of_range&){}}
    try{(void)wayland_configure_size({0,420},0,0);return 6;}catch(const std::out_of_range&){}
    std::cout<<"WAYLAND_POLICY_OK\n";
}
