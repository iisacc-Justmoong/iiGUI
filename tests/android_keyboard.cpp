#include "../src/Platform/AndroidKeyboard.h"
#include <stdexcept>
#include <string>

using namespace ii::gui;
using namespace ii::gui::detail;
namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
std::uint32_t combine(std::uint32_t accent,std::uint32_t value){
    if(accent=='\''&&value=='e')return 0xe9;
    if(accent=='\''&&value==' ')return '\'';
    return 0;
}
AndroidKeyboardFrame character(std::uint32_t value){
    AndroidKeyboardFrame event;event.down=true;event.key=WidgetKey::character;
    event.character=value;event.shortcut='a';event.device=7;event.timestamp=1.5;return event;
}
}
int main(){
    AndroidKeyboardState state;
    auto basic=state.translate(character(0xd55c),combine);
    check(basic.handled&&basic.events.size()==2&&basic.events[0].kind==WidgetInputKind::key_down&&
          basic.events[0].text=="a"&&basic.events[1].kind==WidgetInputKind::text&&
          basic.events[1].text=="한"&&basic.events[1].timestamp==1.5,"Keyboard Unicode or ordering changed");
    auto up=character('a');up.down=false;auto released=state.translate(up,combine);
    check(released.events.size()==1&&released.events[0].kind==WidgetInputKind::key_up,"Key release inserted text");
    auto emoji=state.translate(character(0x1f600),combine);
    check(emoji.events.size()==2&&emoji.events[1].text=="😀","Supplementary Unicode was not encoded as UTF-8");
    auto dead=character(0x80000000u|'\'');
    check(state.translate(dead,combine).events.size()==1,"Dead key inserted text immediately");
    auto e=character('e');e.shortcut='e';auto joined=state.translate(e,combine);
    check(joined.events.size()==2&&joined.events[1].text=="é","Dead key composition was lost");
    state.translate(dead,combine);auto other=character('x');auto separate=state.translate(other,combine);
    check(separate.events.size()==2&&separate.events[1].text=="'x","Uncombinable dead key was dropped");
    state.translate(dead,combine);other.device=8;auto device_change=state.translate(other,combine);
    check(device_change.events.size()==2&&device_change.events[1].text=="x","Dead key crossed keyboard devices");
    state.translate(dead,combine);state.reset();auto focus_change=state.translate(e,combine);
    check(focus_change.events.size()==2&&focus_change.events[1].text=="e","Dead key crossed focus loss");
    auto shortcut=character('z');shortcut.control=true;auto command=state.translate(shortcut,combine);
    check(command.events.size()==1&&command.events[0].control&&command.events[0].text=="a","Control shortcut inserted text");
    state.translate(dead,combine);state.translate(shortcut,combine);
    check(state.translate(e,combine).events[1].text=="e","A command retained a stale dead key");
    auto alt=character('@');alt.control=true;alt.alt=true;alt.alt_graph=true;
    auto alt_graph=state.translate(alt,combine);
    check(alt_graph.events.size()==2&&alt_graph.events[1].text=="@","AltGr character was suppressed");
    auto canceled=character(0);canceled.down=false;canceled.key=WidgetKey::space;canceled.canceled=true;
    auto cancel=state.translate(canceled,combine);
    check(cancel.events.size()==1&&cancel.events[0].kind==WidgetInputKind::key_cancel&&cancel.events[0].key==WidgetKey::space,
          "Canceled release could activate a control");
    auto invalid=character(0x110000);auto rejected=state.translate(invalid,combine);
    check(rejected.events.size()==1,"Invalid Unicode was emitted as text");
    auto unknown=character(0);unknown.key=WidgetKey::none;unknown.shortcut=0;
    check(!state.translate(unknown,combine).handled,"An unknown Android key was consumed");
}
