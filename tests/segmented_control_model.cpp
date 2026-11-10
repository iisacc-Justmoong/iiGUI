#include <iiGUI/SegmentedControlModel.h>
#include <limits>
#include <stdexcept>
namespace { void require(bool value,const char* message){if(!value)throw std::runtime_error(message);} }
int main(){
    using namespace ii::gui;
    SegmentedControlModel row;auto original=row.configuration();
    require(original.horizontal_padding==0&&original.vertical_padding==0&&original.spacing==2&&
            original.border_width==0&&original.corner_radius==8&&original.corner_radius==WidgetAppearance{}.button_radius&&
            original.force_borderless_tone,"Default geometry is not borderless and tight-fitting");
    auto rect=row.geometry(100,30);require(rect&&rect->radius==8&&rect->border_width==0,"Rounded surface geometry changed");
    auto capsule=original;capsule.shape=SegmentedShape::cylinder;capsule.border_width=2;
    require(row.set_configuration(capsule),"Capsule configuration was rejected");
    auto cylinder=row.geometry(100,30);require(cylinder&&cylinder->radius==15,"Capsule radius changed");
    auto narrow=row.geometry(8,30);require(narrow&&narrow->radius==4&&narrow->border_width==2,"Capsule exceeded its width");
    require(!row.geometry(-1,30)&&!row.geometry(std::numeric_limits<double>::infinity(),30),"Invalid extent accepted");
    auto invalid=capsule;invalid.spacing=-1;require(!row.set_configuration(invalid)&&row.configuration()==capsule,"Invalid state replaced good state");
    invalid=capsule;invalid.shape=static_cast<SegmentedShape>(8);require(!row.set_configuration(invalid),"Unknown shape accepted");
    invalid=capsule;invalid.orientation=static_cast<SegmentedOrientation>(8);require(!row.set_configuration(invalid),"Unknown orientation accepted");
    invalid=capsule;invalid.border.alpha=-.1;require(!row.set_configuration(invalid),"Invalid color accepted");
}
