#include <iiGUI/TabModel.h>
#include <stdexcept>
namespace {void check(bool v,const char* m){if(!v)throw std::runtime_error(m);}}
int main(){using namespace ii::gui;TabModel tab;TabConfiguration c;c.text="Overview";c.icon_name="home";c.badge="3";
 check(tab.set_configuration(c),"Valid tab rejected");auto g=tab.geometry(140,32,52,13,8,13);
 check(g&&g->icon.width==18&&g->label.width==52&&g->badge.width==8,"Tab geometry changed");
 c.selected=true;check(tab.set_configuration(c)&&tab.visual({}).underline,"Selected underline missing");auto bad=c;bad.icon_name="";
 check(!tab.set_configuration(bad)&&tab.configuration()==c,"Invalid icon changed state");TabBarModel bar;check(bar.set_items({true,false,true}),"Tab items rejected");
 check(bar.focus_target(0,TabFocusMove::next)==2&&bar.activate(2)==TabActivation::selection_changed,"Tab bar navigation changed");}
