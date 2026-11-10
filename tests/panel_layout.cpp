#include <iiGUI/PanelLayout.h>
#include <limits>
#include <stdexcept>
namespace {
void check(bool v,const char* m){if(!v)throw std::runtime_error(m);}
struct Color{double red=0,green=0,blue=0,alpha=1;};struct Configuration{double width=200,height=100,toolbar_height=26,footer_height=26;bool toolbar_visible=true,footer_visible=true;Color background;};
struct Rect{double x=0,y=0,width=0,height=0;};struct Regions{Rect toolbar,list,footer;bool toolbar_visible=false,list_visible=false,footer_visible=false;};
struct Policy{using Configuration=::Configuration;using Rect=::Rect;using Regions=::Regions;};
}
int main(){using Layout=ii::gui::BasicPanelLayout<Policy>;Configuration c;auto normal=Layout::geometry(c,200,100);check(normal&&normal->list.height==48&&normal->footer.y==74,"panel layout failed");auto small=Layout::geometry(c,100,30);check(small&&small->toolbar.height==26&&small->footer.height==4&&!small->list_visible,"panel clipping failed");c.background.red=std::numeric_limits<double>::quiet_NaN();check(!Layout::validate(c)&&!Layout::geometry(c,100,30),"invalid color accepted");c={};check(!Layout::geometry(c,-1,30),"negative panel extent accepted");}
