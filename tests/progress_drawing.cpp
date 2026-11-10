#include <iiGUI/ProgressDrawing.h>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void run(){
    using namespace ii::gui;
    ProgressDrawing input{.width=100,.height=6,.fill_x=20,.fill_width=40,.track_radius=3,
        .fill_radius=3,.stroke_width=1,.track={.5,.5,.5,.4},.fill={1,0,0,1},.outline={0,0,0,.2}};
    auto result=progress_drawing(input);check(bool(result),"Valid progress drawing failed");
    const auto& panels=(*result)->panels;
    check(panels.size()==3&&panels[0].bounds==Rect{0,0,100,6}&&panels[1].bounds==Rect{20,0,40,6}&&
          panels[2].bounds==panels[0].bounds,"Track, fill and outline order or geometry changed");
    check(std::abs(panels[0].top.red-.21404114f)<1e-6f&&panels[0].top.alpha==.4f&&
          panels[1].top.red==1&&panels[2].stroke_width==1,"Encoded colors or outline were not converted correctly");
    CanvasFrame frame{.viewport={0,0,100,6},.panels=panels};
    check(valid_canvas(frame),"A successful progress drawing was rejected by the GPU canvas contract");
    input.stroke_width=0;result=progress_drawing(input);
    check(result&&(*result)->panels.size()==2,"A zero-width outline painted over the fill");
    input.stroke_width=1;input.track_radius=4;
    check(!progress_drawing(input),"A radius outside the track was accepted");
    input.track_radius=3;input.fill_radius=4;
    check(!progress_drawing(input),"A radius outside the fill was accepted");
    input.fill_radius=3;
    input.fill_width=0;result=progress_drawing(input);
    check(result&&(*result)->panels.size()==2,"An empty fill emitted a panel");
    input.width=0;result=progress_drawing(input);
    check(result&&(*result)->panels.empty(),"An empty extent emitted drawing commands");
    input.width=100;input.height=std::numeric_limits<double>::quiet_NaN();
    check(!progress_drawing(input),"Non-finite geometry was accepted");
    input.height=6;input.track.red=1.1;
    check(!progress_drawing(input),"Out-of-range encoded color was accepted");
    input.track.red=.5;input.stroke_width=-1;
    check(!progress_drawing(input),"Negative geometry was accepted");
}
}
int main(){try{run();std::cout<<"PROGRESS_DRAWING_OK\n";}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
