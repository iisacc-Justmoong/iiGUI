#include <iiGUI/ColorSurfaceModel.h>
#include <limits>
#include <stdexcept>
namespace {void check(bool v,const char* m){if(!v)throw std::runtime_error(m);}}
int main(){using namespace ii::gui;ColorSurfaceModel m;auto c=m.configuration();
 check(m.position(-10,50,100,100)==ColorSurfacePoint{0,.5},"Pointer clipping changed");
 c.enabled=false;check(m.set_configuration(c)&&!m.position(1,1,100,100),"Disabled surface accepted input");
 auto bad=c;bad.width=0;check(!m.set_configuration(bad)&&m.configuration().width==192,"Invalid configuration replaced state");
 ColorSurfaceFrame f;f.width=f.height=2;f.rgba.resize(16);f.color={1,0,0,1};f.marker={.5,.5};check(m.valid_frame(f),"Valid frame rejected");
 f.rgba.pop_back();check(!m.valid_frame(f),"Malformed frame accepted");}
