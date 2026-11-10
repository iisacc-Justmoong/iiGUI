#include <iiGUI/LinkModel.h>
#include <stdexcept>
namespace {void require(bool v,const char* m){if(!v)throw std::runtime_error(m);}}
int main(){ii::gui::LinkModel link;auto c=link.configuration();c.text="Reports";c.palette.link={.6,.3,.2,1};
 require(link.set_configuration(c),"Valid link rejected");auto g=link.geometry(120,36,42,12,false);
 require(g&&g->draw_text&&g->text.x==14&&g->text.width==42,"Link geometry changed");
 require(!link.geometry(120,36,42,12,true)->draw_text,"Child content did not suppress fallback");
 require(link.visual({.pressed=true}).text.red==.5,"Pressed color changed");auto bad=c;bad.horizontal_padding=-1;
 require(!link.set_configuration(bad)&&link.configuration().horizontal_padding==c.horizontal_padding,"Invalid input replaced state");}
