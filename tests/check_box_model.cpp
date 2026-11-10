#include <iiGUI/CheckBoxModel.h>
#include <chrono>
#include <cmath>
#include <limits>
#include <stdexcept>
namespace {
void require(bool v,const char* m){if(!v)throw std::runtime_error(m);}
bool near(double a,double b){return std::abs(a-b)<1e-8;}
}
int main(){
    using namespace std::chrono_literals;using namespace ii::gui;
    CheckBoxModel box;CheckBoxConfiguration c;c.text="Choose";
    c.palette.unchecked={0,0,0,1};c.palette.checked={1,.5,.25,1};c.motion.curve=WidgetMotionCurve::linear;
    const auto t=CheckBoxModel::TimePoint{}+1s;
    require(box.set_configuration(c,t)&&!box.checked()&&!box.animating(),"initial checkbox");
    const auto g=box.geometry(80,18,40,13);
    require(g&&near(g->box.x,.5)&&near(g->box.width,17)&&near(g->text.x,23.5),"checkbox geometry");
    require(box.mark_coverage(.44,.67)>0&&box.mark_coverage(.05,.05)==0,"mark geometry");
    require(box.activate(t)==true&&box.checked()&&box.animating()&&!box.visual({}).authored_mark,"activation");
    require(box.advance(t+65ms)&&near(box.visual({}).fill.red,.875),"fill transition");
    require(box.activate(t+65ms)==false&&!box.checked(),"in-flight retarget");
    require(box.advance(t+130ms)&&near(box.visual({}).fill.red,.109375),"retarget preserves current color");
    c=box.configuration();c.enabled=false;
    require(box.set_configuration(c,t+130ms)&&!box.animating()&&!box.activate(t+131ms),"disabled behavior");
    const auto before=box.configuration();auto bad=before;bad.box_size=0;
    require(!box.set_configuration(bad,t+131ms)&&box.configuration()==before,"invalid configuration changed state");
    bad=before;bad.palette.mark.red=std::numeric_limits<double>::quiet_NaN();
    require(!box.set_configuration(bad,t+131ms)&&!box.geometry(-1,18,40,13),"invalid values accepted");
    c=before;c.enabled=true;c.checked=true;c.motion_enabled=false;c.use_authored_checked_assets=true;
    require(box.set_configuration(c,t+132ms)&&box.visual({}).authored_mark,"explicit authored mark policy");
    c.shape_style=CheckBoxShape::cylinder;
    require(box.set_configuration(c,t+132ms)&&!box.visual({}).authored_mark&&near(box.geometry(80,18,40,13)->radius,8.5),"cylinder policy");
}
