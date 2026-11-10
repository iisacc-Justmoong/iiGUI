#include <iiGUI/PopoverModel.h>
#include <limits>
#include <stdexcept>
namespace {void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}}
int main(){ii::gui::PopoverModel model;auto c=model.configuration();auto size=model.natural_size({100,50});
    require(size&&size->width==124&&size->height==74,"Natural popup size changed");
    c.width=200;c.height=100;c.placement=ii::gui::PopoverPlacement::centered;
    require(model.set_configuration(c)&&model.natural_size({100,50})->width==200,"Explicit size changed");
    auto bad=c;bad.surface_opacity=std::numeric_limits<double>::quiet_NaN();
    require(!model.set_configuration(bad)&&model.configuration()==c,"Invalid input replaced popup state");
    bad=c;bad.accessibility_name=std::string("bad\0name",8);require(!model.set_configuration(bad),"Invalid name accepted");
    require(!model.natural_size({-1,50}),"Invalid content size accepted");
}
