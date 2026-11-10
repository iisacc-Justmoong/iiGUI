#include "../src/Platform/AndroidKeyboardDispatch.h"
#include <stdexcept>
#include <vector>
using namespace ii;
using namespace ii::gui;
using namespace ii::gui::detail;
namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
struct FakeHost {
    std::optional<WidgetSnapshot> focused;
    std::vector<WidgetInput> events;
    ClipboardContent clipboard{.text="paste"};
    bool write_fails=false,read_fails=false,change_focus_on_send=false,remove_editor_on_send=false,change_selection_on_write=false;
    std::optional<WidgetSnapshot> focus(){return focused;}
    bool send(WidgetInput event){
        events.push_back(std::move(event));
        if(change_focus_on_send&&focused)focused->id="other";
        if(remove_editor_on_send&&focused)focused->text_input=false;
        return true;
    }
    core::Result<TextContent> read(std::string_view id,TextRange range){
        if(read_fails)return core::fail(core::ErrorCode::failed,"Read rejected");
        check(id=="editor"&&range==TextRange{1,2},"Clipboard selection range changed");
        return TextContent{range,"😀"};
    }
    core::Result<ClipboardContent> read_clipboard(){return clipboard;}
    core::Result<void> write_clipboard(const ClipboardContent& content){
        if(write_fails)return core::fail(core::ErrorCode::failed,"Write rejected");
        clipboard=content;if(change_selection_on_write&&focused)focused->text_selection={0,0};return {};
    }
};
std::uint32_t combine(std::uint32_t accent,std::uint32_t value){return accent=='\''&&value=='e'?0xe9:0;}
AndroidKeyboardFrame key(char value){AndroidKeyboardFrame f;f.down=true;f.key=WidgetKey::character;f.shortcut=value;f.character=value;return f;}
WidgetSnapshot editor(WidgetKind kind){WidgetSnapshot result{.kind=kind,.id="editor"};result.text_input=true;result.focused=true;result.text_selection={1,2};return result;}
}
int main(){
    AndroidKeyboardDispatcher dispatch;FakeHost host;host.focused=editor(WidgetKind::input_field);
    auto space=key(' ');space.key=WidgetKey::space;
    check(dispatch.input(space,host,combine)&&host.events.size()==1&&host.events[0].key==WidgetKey::space,
          "Legacy input field received Space twice");
    host.events.clear();host.focused=editor(WidgetKind::absolute_stack);
    check(dispatch.input(space,host,combine)&&host.events.size()==2&&host.events[1].text==" ",
          "Custom editor did not receive Space text");
    host.events.clear();auto copy=key('c');copy.control=true;
    check(dispatch.input(copy,host,combine)&&host.events.size()==1&&host.events[0].key==WidgetKey::copy,
          "Custom editor did not receive Copy");
    host.events.clear();auto paste=key('v');paste.control=true;
    check(dispatch.input(paste,host,combine)&&host.events.size()==1&&host.events[0].content&&
          host.events[0].content->text=="paste","Custom editor did not receive clipboard content");
    host.events.clear();host.focused=editor(WidgetKind::input_field);host.write_fails=true;
    auto cut=key('x');cut.control=true;
    check(dispatch.input(cut,host,combine)&&host.events.empty(),"Failed clipboard write deleted selected text");
    host.write_fails=false;check(dispatch.input(cut,host,combine)&&host.clipboard.text=="😀"&&
        host.events.size()==1&&host.events[0].key==WidgetKey::cut,"Legacy cut lost its Unicode selection");
    host.events.clear();host.change_selection_on_write=true;
    check(dispatch.input(cut,host,combine)&&host.events.empty(),"Clipboard reentry cut a changed selection");
    host.change_selection_on_write=false;host.focused=editor(WidgetKind::input_field);
    host.events.clear();host.focused->password=true;host.clipboard.text="unchanged";
    check(dispatch.input(cut,host,combine)&&host.events.empty()&&host.clipboard.text=="unchanged",
          "Password cut disclosed or deleted a secret");
    host.focused=editor(WidgetKind::absolute_stack);host.events.clear();host.change_focus_on_send=true;
    check(dispatch.input(key('q'),host,combine)&&host.events.size()==1,
          "A key press typed into the editor that gained focus during its callback");
    host.change_focus_on_send=false;host.focused=editor(WidgetKind::absolute_stack);host.events.clear();host.remove_editor_on_send=true;
    check(dispatch.input(key('q'),host,combine)&&host.events.size()==1,
          "A key press typed into a surface whose text provider was retired during its callback");
    host.remove_editor_on_send=false;host.focused.reset();auto dedicated=key(0);dedicated.key=WidgetKey::none;dedicated.paste=true;
    check(!dispatch.input(dedicated,host,combine),"Paste without a focused editor was consumed");
    host.focused=editor(WidgetKind::absolute_stack);host.events.clear();host.clipboard.text="data";
    check(dispatch.input(dedicated,host,combine)&&host.events.size()==1&&host.events[0].content->text=="data",
          "Dedicated paste key did not deliver the clipboard");
    host.focused.reset();dedicated.down=false;
    check(dispatch.input(dedicated,host,combine),"A consumed paste key lost its release after a focus change");
}
