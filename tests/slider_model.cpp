#include <iiGUI/SliderModel.h>
#include <cmath>
#include <limits>
#include <stdexcept>
namespace {void check(bool v,const char* m){if(!v)throw std::runtime_error(m);}bool near(double a,double b){return std::abs(a-b)<1e-9;}}
int main(){using namespace ii::gui;SliderModel slider;auto c=slider.configuration();c.minimum=-10;c.maximum=10;c.value=1;c.step=2;
 check(slider.set_configuration(c)&&slider.value()==2,"Initial value was not snapped");auto g=slider.geometry(100,32);
 check(g&&near(g->track.x,9)&&near(g->track.width,82)&&near(g->fill.width,49.2),"Slider geometry changed");
 check(slider.set_from_position(9,100)&&slider.value()==-10&&slider.nudge(1)&&slider.value()==-8,"Pointer or keyboard mapping failed");
 auto invalid=slider.configuration();invalid.maximum=invalid.minimum;check(!slider.set_configuration(invalid)&&slider.value()==-8,"Invalid range replaced state");
 c=slider.configuration();c.type=SliderType::segmented;c.segment_count=5;c.step=0;c.value=.3;
 check(slider.set_configuration(c)&&slider.value()==0,"Segmented value failed to snap");c.value=std::numeric_limits<double>::quiet_NaN();
 check(!slider.set_configuration(c),"Nonfinite value was accepted");}
