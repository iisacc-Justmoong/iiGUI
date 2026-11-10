#include <iiGUI/DropdownButtonModel.h>
#include <limits>
#include <stdexcept>
namespace { void require(bool value,const char* message){if(!value)throw std::runtime_error(message);} }
int main(){
    using namespace ii::gui;
    DropdownButtonModel button; auto initial=button.configuration();
    require(initial.height==22 && initial.left_padding==8 && initial.right_padding==2 && initial.indicator_size==18 &&
            initial.corner_radius==8 && button.preferred_width(32)==60, "Dropdown defaults changed");
    auto label=button.geometry(60,22,32,13);
    require(label && label->label.x==8 && label->label.width==32 && label->indicator.x==40 && label->indicator.width==18,
            "Dropdown label and indicator geometry changed");
    auto icon=initial; icon.icon_mode=true; icon.left_padding=4; icon.spacing=-2;
    require(button.set_configuration(icon) && button.preferred_width(0)==40, "Dropdown icon sizing changed");
    auto shape=button.geometry(40,22,0,0);
    require(shape && shape->icon.x==4 && shape->icon.width==18 && shape->indicator.x==20, "Dropdown icon geometry changed");
    require(!button.geometry(-1,22,0,0) && !button.preferred_width(std::numeric_limits<double>::infinity()), "Invalid geometry was accepted");
    auto invalid=icon; invalid.height=0;
    require(!button.set_configuration(invalid) && button.configuration()==icon, "Invalid options replaced valid state");
    invalid=icon; invalid.spacing=-257; require(!button.set_configuration(invalid), "Invalid spacing was accepted");
    invalid=icon; invalid.left_padding=8192; require(!button.set_configuration(invalid), "Overflowing padding was accepted");
}
