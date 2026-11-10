#include <iiGUI/EditorViewports.h>
#include <cmath>
#include <limits>
#include <stdexcept>

void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
bool near(double a,double b){return std::abs(a-b)<1e-8;}
int main(){
    using namespace ii::gui;
    EditorCanvasViewport canvas;check(canvas.configure({.scale=2,.offset_x=10,.offset_y=-20}),"canvas setup");
    auto document=canvas.document_point({30,20});check(near(document.x,10)&&near(document.y,20),"canvas inverse transform");
    auto screen=canvas.screen_point(document);check(near(screen.x,30)&&near(screen.y,20),"canvas forward transform");
    check(canvas.zoom_at(2,{30,20}),"canvas zoom");screen=canvas.screen_point(document);
    check(near(screen.x,30)&&near(screen.y,20),"zoom shifted pointer anchor");
    auto before=canvas.configuration();check(!canvas.zoom_at(std::numeric_limits<double>::infinity(),{0,0})&&canvas.configuration()==before,"bad zoom changed canvas");
    check(canvas.pan(5,-3)&&near(canvas.configuration().offset_x,before.offset_x+5),"canvas pan");
    check(canvas.configure({.scale=2,.offset_x=10,.offset_y=20,.rotation_radians=1.5707963267948966,.flip_x=true}),"canvas rotation setup");
    screen=canvas.screen_point({3,4});document=canvas.document_point(screen);
    check(near(document.x,3)&&near(document.y,4)&&near(screen.x,2)&&near(screen.y,14),"rotated and mirrored canvas transform");
    check(canvas.zoom_at(2,screen),"rotated canvas zoom");auto anchored=canvas.screen_point(document);
    check(near(anchored.x,screen.x)&&near(anchored.y,screen.y),"rotated canvas lost zoom anchor");
    EditorTimelineViewport timeline;check(timeline.configure({.frame_count=1'000'000'000,.pixels_per_frame=2,.scroll=100,.width=200}),"timeline setup");
    auto range=timeline.visible_frames();check(range.first==50&&range.end==150,"visible timeline query");
    check(timeline.frame_at(40)==70,"timeline seek");
    auto anchor=timeline.frame_at(80);check(timeline.zoom_at(2,80)&&timeline.frame_at(80)==anchor,"timeline zoom anchor");
    auto old=timeline.configuration();check(!timeline.configure({.frame_count=10,.pixels_per_frame=0,.width=100})&&timeline.configuration()==old,"bad timeline update");
    check(timeline.scroll_by(1e15)&&timeline.frame_at(200)==999'999'999,"timeline end clamp");
    check(timeline.configure({.frame_count=0,.pixels_per_frame=1,.width=200})&&timeline.visible_frames().end==0&&!timeline.frame_at(0),"empty timeline");
}
