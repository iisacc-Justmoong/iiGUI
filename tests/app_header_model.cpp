#include <iiGUI/AppHeaderModel.h>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
bool near(double a,double b){return std::abs(a-b)<1e-8;}
void defaults_and_layout(){
    ii::gui::AppHeaderModel header;auto normal=header.resolved();
    require(near(normal.horizontal_padding,16)&&near(normal.vertical_padding,16)&&near(normal.row_spacing,12)&&
        near(normal.action_spacing,8)&&near(normal.menu_padding,10)&&near(normal.menu_height,32),"Default header metrics changed");
    auto size=header.natural_size({});require(size&&near(size->height,56),"Empty header lost its minimum height");
    auto config=header.configuration();config.title="Title";config.subtitle="Subtitle";config.menu_visible=true;config.compact=true;
    require(header.set_configuration(config),"Valid compact header rejected");auto compact=header.resolved();
    require(near(compact.horizontal_padding,10)&&near(compact.vertical_padding,10)&&near(compact.row_spacing,8)&&
        near(compact.action_spacing,6)&&near(compact.menu_padding,8)&&near(compact.menu_height,28),"Compact header metrics changed");
    size=header.natural_size({.title_width=100,.subtitle_width=80,.actions_width=40,.actions_height=20});
    require(size&&near(size->height,70)&&size->width>100,"Header natural size lost its title, menu or actions");
    config.content_horizontal_padding=20;config.row_spacing=3;config.width=500;config.height=90;
    require(header.set_configuration(config),"Explicit header metrics rejected");compact=header.resolved();size=header.natural_size({});
    require(near(compact.horizontal_padding,20)&&near(compact.row_spacing,3)&&size&&near(size->width,500)&&near(size->height,90),
        "Explicit header inputs did not override defaults");
}
void validation(){
    ii::gui::AppHeaderModel header;auto initial=header.configuration(),bad=initial;bad.action_spacing=-1;
    require(!header.set_configuration(bad)&&header.configuration()==initial,"Invalid spacing changed the header");
    bad=initial;bad.width=std::numeric_limits<double>::quiet_NaN();require(!header.set_configuration(bad),"NaN header width accepted");
    bad=initial;bad.title=std::string("a\0b",3);require(!header.set_configuration(bad),"Invalid header title accepted");
    bad=initial;bad.background.alpha=2;require(!header.set_configuration(bad),"Invalid header color accepted");
    bad=initial;bad.menu_button_padding=4091;require(!header.set_configuration(bad),"Oversized menu width accepted");
    require(!header.natural_size({.actions_height=-1}),"Invalid action measurement accepted");
}
void supplied_metrics(){
    ii::gui::AppHeaderModel header;auto config=header.configuration();
    config.normal_metrics={20,21,13,9,11,35};config.compact_metrics={7,8,9,10,11,12};
    require(header.set_configuration(config),"Application header metrics were rejected");
    auto metrics=header.resolved();
    require(near(metrics.horizontal_padding,20)&&near(metrics.vertical_padding,21)&&near(metrics.menu_height,35),
        "Normal header did not use supplied metrics");
    config.compact=true;config.row_spacing=2;require(header.set_configuration(config),"Compact metrics were rejected");
    metrics=header.resolved();require(near(metrics.horizontal_padding,7)&&near(metrics.vertical_padding,8)&&
        near(metrics.row_spacing,2)&&near(metrics.menu_height,12),"Compact header did not use its supplied metrics and overrides");
    config.normal_metrics.menu_height=std::numeric_limits<double>::infinity();
    require(!header.set_configuration(config),"Invalid unused header metrics were accepted");
}
}
int main(){defaults_and_layout();validation();supplied_metrics();}
