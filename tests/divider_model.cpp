#include <iiGUI/Divider.h>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace {
void require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
bool near(double a,double b){return std::abs(a-b)<1e-9;}
}
int main(){
    ii::gui::Divider divider;
    ii::gui::DividerConfiguration configuration;
    require(divider.set_configuration(configuration),"Default divider rejected");
    auto size=divider.extent();
    require(near(size.width,220)&&near(size.height,3),"Horizontal natural size is incorrect");
    auto line=divider.geometry(size.width,size.height);
    require(line&&near(line->x,0)&&near(line->y,1)&&near(line->width,220)&&near(line->height,1),
            "Horizontal divider geometry is incorrect");
    configuration.axis=ii::gui::DividerAxis::vertical;
    configuration.length=80;configuration.thickness=2;configuration.cross_padding=2;configuration.line_padding=4;
    require(divider.set_configuration(configuration),"Vertical divider rejected");
    size=divider.extent();line=divider.geometry(size.width,size.height);
    require(near(size.width,6)&&near(size.height,80)&&line&&near(line->x,2)&&near(line->y,4)&&
            near(line->width,2)&&near(line->height,72),"Vertical divider geometry is incorrect");
    line=divider.geometry(2,4);
    require(line&&near(line->width,2)&&near(line->height,0),"Constrained divider did not clamp its line");
    const auto previous=divider.configuration();
    auto invalid=configuration;invalid.thickness=-1;
    require(!divider.set_configuration(invalid)&&divider.configuration()==previous,"Invalid divider replaced valid state");
    invalid=configuration;invalid.cross_padding=8192;
    require(!divider.set_configuration(invalid),"Unbounded natural size accepted");
    invalid=configuration;invalid.color.alpha=std::numeric_limits<double>::quiet_NaN();
    require(!divider.set_configuration(invalid),"Invalid divider color accepted");
    invalid=configuration;invalid.axis=static_cast<ii::gui::DividerAxis>(99);
    require(!divider.set_configuration(invalid),"Invalid divider axis accepted");
    require(!divider.geometry(-1,4),"Invalid allocation accepted");
}
