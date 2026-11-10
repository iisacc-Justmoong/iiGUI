#include "../src/Graphics/GpuImage.h"

#include <cmath>
#include <iostream>
#include <stdexcept>

using namespace ii::gui;
using namespace ii::gui::detail;
namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
bool equal(float a,float b){return std::abs(a-b)<0.001f;}
}
int main(){
    try {
        ImageRun image{.bounds={10,20,40,30},.tint=Color{.2f,.3f,.4f,.5f},.opacity=.8f,
                       .source={.25f,.1f,.5f,.8f},.radius=6,.border_width=2,
                       .border={1,0,0,.7f},.gradient=ImageGradient{.height=10,.middle_position=.3f}};
        const ContentTransform transform{.origin_x=20,.origin_y=10,.scale=.5f,.opacity=.25f,.offset_x=3,.offset_y=4};
        const auto values=gpu_image_uniform(image,transform,2,{0,0,100,80});
        check(equal(values.bounds[0],36)&&equal(values.bounds[1],38)&&equal(values.bounds[2],40)&&equal(values.bounds[3],30),"Image bounds did not follow content and device scale");
        check(equal(values.source[0],.25f)&&equal(values.source[2],.5f),"Source crop was not retained");
        check(equal(values.style[0],6)&&equal(values.style[1],2)&&equal(values.style[2],.2f)&&equal(values.style[3],3),"Mask, border, opacity or paint flags changed");
        check(equal(values.viewport[0],200)&&equal(values.viewport[1],160)&&equal(values.viewport[2],10)&&equal(values.viewport[3],.3f),"Physical viewport or gradient changed");
        auto before=foreground_order(true),after=foreground_order(false);
        check(before[0]==ForegroundPass::images&&before[1]==ForegroundPass::text&&after[0]==ForegroundPass::text&&after[1]==ForegroundPass::images,"Foreground order changed");
        return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
