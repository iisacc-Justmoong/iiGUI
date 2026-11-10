#include <iiGUI/RadioButtonModel.h>
#include <chrono>
#include <cmath>
#include <limits>
#include <stdexcept>
void require(bool v,const char* m){if(!v)throw std::runtime_error(m);}
bool near(double a,double b){return std::abs(a-b)<1e-8;}
int main(){
    using namespace ii::gui;using namespace std::chrono_literals;
    RadioButtonModel radio;RadioButtonConfiguration c;c.text="Option";c.motion.curve=WidgetMotionCurve::linear;c.motion.duration=180ms;
    c.palette.off={0,0,0,1};c.palette.on={1,.5,.25,1};const auto t=RadioButtonModel::TimePoint{}+1s;
    require(radio.set_configuration(c,t)&&!radio.checked(),"initial radio");
    auto g=radio.geometry(60,18,30,13);require(g&&near(g->indicator.width,18)&&near(g->text.x,26),"radio geometry");
    require(radio.activate(t)==true&&radio.animating()&&radio.visual({}).indicator==c.palette.off,"radio activation");
    require(radio.advance(t+65ms)&&near(radio.visual({}).indicator.red,.875),"indicator interpolation");
    require(radio.advance(t+90ms)&&near(radio.dot_scale(),.5),"dot interpolation");
    require(radio.set_checked(false,t+90ms)&&!radio.visual({}).draw_dot,"deselected radio drew dot");
    require(radio.set_checked(true,t+90ms)&&radio.visual({}).draw_dot,"radio did not retarget");
    auto invalid=radio.configuration();invalid.dot_size=19;const auto previous=radio.configuration();
    require(!radio.set_configuration(invalid,t)&&radio.configuration()==previous,"invalid geometry changed state");
    invalid=previous;invalid.group=std::string("x\0y",3);require(!radio.set_configuration(invalid,t),"invalid group");
    invalid=previous;invalid.palette.on.red=std::numeric_limits<double>::quiet_NaN();require(!radio.set_configuration(invalid,t),"invalid color");
    c=previous;c.enabled=false;require(radio.set_configuration(c,t+91ms)&&!radio.activate(t+92ms)&&!radio.animating(),"disabled behavior");
}
