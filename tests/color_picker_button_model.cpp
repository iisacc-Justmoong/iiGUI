#include <iiGUI/ColorPickerButtonModel.h>
#include <cmath>
#include <limits>
#include <stdexcept>
namespace {void require(bool v,const char* m){if(!v)throw std::runtime_error(m);}}
int main(){ii::gui::ColorPickerButtonModel model;auto c=model.configuration();auto r=model.geometry(28,28);
    require(r&&r->ring.width==20&&std::abs(r->swatch.width-20*18./28)<1e-10&&r->radius==4,"Color button geometry changed");
    c.circular=true;c.enabled=false;c.border_opacity=.5;require(model.set_configuration(c),"Valid color button rejected");
    r=model.geometry(36,22);require(r&&r->radius==11&&model.visual({}).opacity==.32,"Disabled or circular geometry changed");
    const auto before=model.configuration();auto invalid=before;invalid.current_color.blue=std::numeric_limits<double>::quiet_NaN();
    require(!model.set_configuration(invalid)&&model.configuration()==before,"Invalid color changed state");
    invalid=before;invalid.accessibility_name=std::string("bad\0name",8);require(!model.set_configuration(invalid)&&!model.geometry(-1,3),"Invalid input accepted");
}
