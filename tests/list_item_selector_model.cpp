#include <iiGUI/ListItemSelectorModel.h>
#include <stdexcept>
namespace {void check(bool v,const char* m){if(!v)throw std::runtime_error(m);}}
int main(){using namespace ii::gui;ListItemSelectorModel model;check(model.display_text()=="Option"&&!model.choose(0),"Empty placeholder changed");
 SelectorConfiguration c;c.options={{"A",true},{"B",false},{"C",true}};check(model.set_configuration(c),"Valid choices rejected");
 check(!model.choose(1)&&model.navigate(1)==2&&model.display_text()=="C", "Navigation did not skip disabled choice");
 auto bad=model.configuration();bad.selected_index=3;check(!model.set_configuration(bad)&&model.configuration().selected_index==2,"Invalid input replaced selection");
 bad=model.configuration();bad.options[0].text=std::string("\xc0\x80",2);check(!model.set_configuration(bad),"Invalid UTF-8 accepted");}
