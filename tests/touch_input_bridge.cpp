#include "../src/Platform/TouchInputBridge.h"
#include <cmath>
#include <stdexcept>
using namespace ii::gui;
using namespace ii::runtime::touch;
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int main(){
    TouchSample sample{.identity=17,.x=12,.y=40,.timestamp=5,.pressure=.4,.tilt_x=20,.tilt_y=-10,.pen=true};
    auto press=detail::widget_touch_event({TouchEventKind::pointer_down,sample});
    check(press&&press->device==PointerDevice::pen&&press->device_id==17&&press->pressure==.4&&press->buttons==1,
          "A raw pencil sample lost its identity or pressure");
    auto pan=detail::widget_touch_event({TouchEventKind::pan,sample,8,-11});
    check(pan&&pan->device==PointerDevice::touch&&pan->scroll_x==8&&pan->scroll_y==-11&&!pan->control,
          "Touch movement did not retain the physical finger direction");
    auto zoom=detail::widget_touch_event({TouchEventKind::pinch,sample,0,0,2});
    auto scale=zoom?pinch_scale_factor(*zoom):std::nullopt;
    check(scale&&std::abs(*scale-2)<1e-12&&zoom->scroll_y>0,"Pinch-out did not increase zoom");
    check(!detail::widget_touch_event({TouchEventKind::pinch,sample,0,0,0}),"Invalid pinch was delivered");
}
