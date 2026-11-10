#include <iiGUI/ComboBoxModel.h>
#include <limits>
#include <stdexcept>
namespace {void require(bool v,const char* m){if(!v)throw std::runtime_error(m);}}
int main(){ii::gui::ComboBoxModel box;auto c=box.configuration();auto r=box.geometry(97,20);
    require(r&&r->label.x==8&&r->label.width==70&&r->indicator.x==78&&r->indicator.width==18,"Combo geometry changed");
    require(box.visual({.pressed=true}).background==c.palette.pressed,"Pressed color changed");
    c.tone=ii::gui::ButtonTone::borderless;c.arrow=ii::gui::StepperArrow::down;
    require(box.set_configuration(c)&&box.visual({}).background.alpha==0&&box.glyph()=="⌄","Borderless combo changed");
    auto bad=c;bad.width=18;require(!box.set_configuration(bad)&&box.configuration()==c,"Invalid width changed state");
    bad=c;bad.arrow=static_cast<ii::gui::StepperArrow>(99);require(!box.set_configuration(bad)&&!box.geometry(-1,20),"Invalid input accepted");
}
