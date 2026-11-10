#include <iiGUI/MenuItemModel.h>
#include <limits>
#include <stdexcept>
namespace {void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}}
int main(){
    ii::gui::MenuItemModel item;auto value=item.configuration();value.label="Open";value.shortcut="Ctrl+O";
    require(item.set_configuration(value)&&item.width()==161&&item.height()==24,"Regular geometry changed");
    auto rect=item.geometry(161,24,40,13,18,13);
    require(rect&&rect->icon.width==16&&rect->chevron.width==16&&rect->label.width==40&&rect->shortcut.width==18,"Item layout changed");
    require(item.visual({.hovered=true,.pressed=true}).background==value.palette.pressed,"Pressed state lost precedence");
    value.compact=true;value.state=ii::gui::MenuItemState::selected;value.expanded=true;
    require(item.set_configuration(value)&&item.width()==141&&item.height()==18&&item.direction()==ii::gui::MenuDirection::down&&item.visual({}).background==value.palette.selected,"Selection or direction changed");
    value.state=ii::gui::MenuItemState::inactive;require(item.set_configuration(value)&&!item.visual({.focused=true}).enabled&&!item.visual({.focused=true}).focused,"Inactive item remained interactive");
    value.key_visible=false;require(item.set_configuration(value)&&item.shortcut().empty(),"Hidden shortcut remained visible");
    value.key_visible=true;value.shortcut.clear();value.key_placeholder="Key";require(item.set_configuration(value)&&item.shortcut()=="Key","Shortcut fallback changed");
    auto narrow=item.geometry(10,10,40,13,18,13);require(narrow&&narrow->icon.width<=10&&narrow->label.width>=0&&narrow->chevron.width>=0,"Narrow geometry invalid");
    auto bad=value;bad.icon_size=std::numeric_limits<double>::quiet_NaN();require(!item.set_configuration(bad)&&item.configuration()==value,"Invalid candidate replaced state");
    require(!item.geometry(-1,24,40,13,18,13),"Invalid extent accepted");
}
