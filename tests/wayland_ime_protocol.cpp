#include "../src/Platform/WaylandImeProtocol.h"
#include "../src/Widgets/WidgetState.h"
#include <memory>
#include <stdexcept>

using namespace ii;
using namespace ii::gui;
using namespace ii::gui::detail;

namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(core::Result<T> value){if(!value)throw std::runtime_error(value.error().message);return std::move(*value);}
void good(core::Result<void> value){if(!value)throw std::runtime_error(value.error().message);}
struct Host final {
    std::shared_ptr<WidgetState> tree=std::make_shared<WidgetState>([](runtime::Task task){task();});
    unsigned reads=0,focus_calls=0,change_on_focus=0;bool change_during_read=false;
    std::optional<WidgetSnapshot> focus(){
        if(++focus_calls==change_on_focus)(void)tree->input({.kind=WidgetInputKind::text,.text="x"});
        auto nodes=tree->accessibility_snapshots();if(!nodes)return {};
        for(auto& node:*nodes)if(node.focused&&node.text_input&&node.visible&&node.enabled&&!node.input_blocked)return node;
        return {};
    }
    core::Result<TextContent> read(std::string_view id,TextRange range){
        ++reads;auto widget=tree->find(id);if(!widget)return std::unexpected(widget.error());
        auto content=widget->text_content(range);
        if(change_during_read){change_during_read=false;(void)tree->input({.kind=WidgetInputKind::text,.text="x"});}
        return content;
    }
    core::Result<void> send(WidgetInput input){return tree->input(std::move(input));}
};
void select(Host& host,TextRange range,bool reversed=false){
    good(host.tree->input({.kind=WidgetInputKind::text_selection,.replacement_range=range,.selection_reversed=reversed}));
}
void composition(){
    Host host;auto field=take(host.tree->input_field({},"field",{.text="A한😀Z"},{},{}));
    (void)take(host.tree->drawing_snapshot({300,40}));good(field.focus());select(host,{2,0});
    WaylandImeProtocol ime;good(ime.start(host));
    WaylandImeBatch batch;batch.preedit="글😀";batch.cursor_begin=3;batch.cursor_end=7;
    good(ime.apply(host,batch));auto current=host.focus();
    check(current->marked_range==TextRange{2,3}&&current->text_selection==TextRange{3,2},
          "Wayland preedit byte indices did not become UTF-16 selection");
    batch={};batch.committed="가";batch.preedit="나";batch.cursor_begin=3;batch.cursor_end=3;
    good(ime.apply(host,batch));current=host.focus();
    check(take(field.snapshot()).text=="A한가😀Z"&&current->marked_range==TextRange{3,1},
          "Wayland commit and next preedit were not applied in order");
    batch={};good(ime.apply(host,batch));
    check(!host.focus()->marked_range&&take(field.snapshot()).text=="A한가😀Z",
          "Wayland empty done did not clear the previous preedit");
    host.tree->close();
}
void surrounding_delete(){
    Host host;auto field=take(host.tree->input_field({},"field",{.text="A한😀Z"},{},{}));
    (void)take(host.tree->drawing_snapshot({300,40}));good(field.focus());select(host,{2,0});
    WaylandImeProtocol ime;good(ime.start(host));
    WaylandImeBatch invalid;invalid.before=2;invalid.committed="x";
    check(!ime.apply(host,invalid)&&take(field.snapshot()).text=="A한😀Z",
          "Wayland accepted a deletion splitting a UTF-8 scalar or partially committed it");
    WaylandImeBatch batch;batch.before=3;batch.after=4;batch.committed="가";
    batch.preedit="😀";batch.cursor_begin=4;batch.cursor_end=4;
    good(ime.apply(host,batch));
    check(take(field.snapshot()).text=="A가Z"&&host.focus()->marked_range==TextRange{2,2},
          "Wayland surrounding deletion used byte counts as UTF-16 positions");
    host.tree->close();
}
void selected_text(){
    Host host;auto field=take(host.tree->input_field({},"field",{.text="A한😀Z"},{},{}));
    (void)take(host.tree->drawing_snapshot({300,40}));good(field.focus());select(host,{2,2},true);
    WaylandImeProtocol ime;good(ime.start(host));
    auto surrounding=take(ime.surrounding(host));
    check(surrounding&&surrounding->text=="A한😀Z"&&surrounding->cursor==4&&surrounding->anchor==8,
          "Wayland surrounding text lost the reversed UTF-8 selection");
    WaylandImeBatch batch;batch.before=3;batch.after=1;good(ime.apply(host,batch));
    check(take(field.snapshot()).text=="A😀"&&host.focus()->text_selection==TextRange{1,2}&&host.focus()->text_selection_reversed,
          "Wayland deletion did not preserve the selected text and direction");
    host.tree->close();
}
void privacy_and_bounds(){
    Host host;auto field=take(host.tree->input_field({},"field",{.text=std::string(5000,'x')},{},{}));
    (void)take(host.tree->drawing_snapshot({300,40}));good(field.focus());select(host,{2500,0});
    WaylandImeProtocol ime;good(ime.start(host));auto value=take(ime.surrounding(host));
    check(value&&value->text.size()<=4000&&value->cursor==value->anchor&&value->cursor>0,
          "Wayland surrounding text exceeded its budget or omitted the cursor");
    select(host,{0,5000});check(!take(ime.surrounding(host)),"Wayland exported an oversized selection");
    host.tree->close();
    Host secret;auto password=take(secret.tree->input_field({},"secret",{.text="secret",.password=true},{},{}));
    (void)take(secret.tree->drawing_snapshot({300,40}));good(password.focus());good(ime.start(secret));
    check(!take(ime.surrounding(secret))&&secret.reads==0,"Wayland read or exported password surrounding text");
    secret.tree->close();
}
void generation(){
    Host host;Widget first,other;bool bounce=false;
    auto root=take(host.tree->absolute_stack({},"root",{300,40}));
    first=take(host.tree->input_field(root,"first",{.text="A"},[&](std::string){good(other.focus());if(bounce)good(first.focus());},{}));
    other=take(host.tree->input_field(root,"other",{.text="B"},{},{}));
    (void)take(host.tree->drawing_snapshot({300,40}));good(first.focus());
    WaylandImeProtocol ime;good(ime.start(host));WaylandImeBatch batch;
    batch.committed="한";batch.preedit="글";batch.cursor_begin=3;batch.cursor_end=3;
    check(!ime.apply(host,batch)&&take(other.snapshot()).text=="B"&&!take(other.snapshot()).marked_range,
          "Wayland continuation reached another editor");
    good(first.focus());bounce=true;good(ime.start(host));
    check(!ime.apply(host,batch)&&!take(first.snapshot()).marked_range,
          "Wayland continuation survived a focus ABA");
    host.tree->close();
}
void malformed(){
    Host host;auto field=take(host.tree->input_field({},"field",{.text="A"},{},{}));
    (void)take(host.tree->drawing_snapshot({300,40}));good(field.focus());WaylandImeProtocol ime;good(ime.start(host));
    WaylandImeBatch batch;batch.committed="한";batch.preedit="😀";batch.cursor_begin=1;batch.cursor_end=1;
    check(!ime.apply(host,batch)&&take(field.snapshot()).text=="A","Malformed preedit partially committed text");
    batch.preedit=std::string("\xf0\x80\x80\x80",4);batch.cursor_begin=0;batch.cursor_end=0;
    check(!ime.apply(host,batch)&&take(field.snapshot()).text=="A","Invalid UTF-8 reached Wayland editor");
    host.tree->close();
}
void session_and_selection(){
    Host host;auto field=take(host.tree->input_field({},"field",{.text="aBCd"},{},{}));
    (void)take(host.tree->drawing_snapshot({300,40}));good(field.focus());select(host,{1,2});
    WaylandImeProtocol ime;WaylandImeBatch batch;batch.preedit="한";batch.cursor_begin=3;batch.cursor_end=3;
    check(!ime.apply(host,batch)&&take(field.snapshot()).text=="aBCd",
          "Wayland accepted a batch before its session was started");
    good(ime.start(host));good(ime.apply(host,batch));
    check(take(host.read("field",{0,3})).text=="a한d"&&host.focus()->marked_range==TextRange{1,1},
          "Wayland preedit did not replace the previously selected text");
    batch={};good(ime.apply(host,batch));
    check(take(field.snapshot()).text=="ad"&&!host.focus()->marked_range,
          "Wayland cancellation restored text removed by preedit");
    ime.abandon();batch.committed="x";
    check(!ime.apply(host,batch)&&take(field.snapshot()).text=="ad",
          "An abandoned Wayland session accepted a late batch");
    host.tree->close();
}
void marked_deletion(){
    Host host;auto field=take(host.tree->input_field({},"field",{.text="A한😀Z"},{},{}));
    (void)take(host.tree->drawing_snapshot({300,40}));good(field.focus());select(host,{2,0});
    WaylandImeProtocol ime;good(ime.start(host));WaylandImeBatch batch;
    batch.preedit="글";batch.cursor_begin=3;batch.cursor_end=3;good(ime.apply(host,batch));
    auto surrounding=take(ime.surrounding(host));
    check(surrounding&&surrounding->text=="A한😀Z"&&surrounding->cursor==4&&surrounding->anchor==4,
          "Wayland surrounding text contained the preedit");
    batch={};batch.before=3;batch.after=4;batch.committed="B";good(ime.apply(host,batch));
    check(take(field.snapshot()).text=="ABZ"&&!host.focus()->marked_range,
          "Wayland deletion around preedit was applied to the candidate");
    host.tree->close();
}
void unicode_window_and_read_race(){
    std::string text;for(int i=0;i<3000;++i)text+="😀";
    Host host;auto field=take(host.tree->input_field({},"field",{.text=text},{},{}));
    (void)take(host.tree->drawing_snapshot({300,40}));good(field.focus());select(host,{3000,0});
    WaylandImeProtocol ime;good(ime.start(host));auto value=take(ime.surrounding(host));
    check(value&&value->text.size()==4000&&value->cursor==2000&&value->anchor==2000&&
          take(TextIndex::create(value->text)).utf16_size()==2000,
          "Wayland context cropping split a multibyte scalar");
    select(host,{1000,2200});check(!take(ime.surrounding(host)),"Wayland exported a selection larger than 4000 bytes");
    select(host,{3000,0});host.change_during_read=true;
    auto stale=ime.surrounding(host);
    check(!stale&&stale.error().code==core::ErrorCode::busy,
          "Wayland published a surrounding snapshot changed during its read");
    host.tree->close();
}
void invalid_sessions(){
    Host host;auto field=take(host.tree->absolute_stack({},"legacy",{100,40}));
    auto provider=std::make_shared<TextInputProvider>();
    provider->state=[]()->core::Result<TextInputState>{return TextInputState{.length=0,.selection={0,0}};};
    provider->read=[](TextRange range)->core::Result<TextContent>{return TextContent{range,{}};};
    good(field.set_text_input_provider(provider));(void)take(host.tree->drawing_snapshot({100,40}));good(field.focus());
    WaylandImeProtocol ime;check(!ime.start(host),"Wayland accepted an unversioned asynchronous editor");
    host.tree->close();
}
void change_before_delivery(){
    Host host;auto field=take(host.tree->input_field({},"field",{.text="AB"},{},{}));
    (void)take(host.tree->drawing_snapshot({100,40}));good(field.focus());select(host,{2,0});
    WaylandImeProtocol ime;good(ime.start(host));host.change_on_focus=host.focus_calls+3;
    WaylandImeBatch batch;batch.before=1;
    auto result=ime.apply(host,batch);
    check(!result&&take(field.snapshot()).text=="ABx",
          "Wayland applied a deletion range from an obsolete text revision");
    host.tree->close();
    Host next;auto other=take(next.tree->input_field({},"other",{.text="AB"},{},{}));
    (void)take(next.tree->drawing_snapshot({100,40}));good(other.focus());select(next,{2,0});
    WaylandImeProtocol second;good(second.start(next));next.change_on_focus=next.focus_calls+2;
    batch={};batch.committed="Z";result=second.apply(next,batch);
    check(!result&&take(other.snapshot()).text=="ABx",
          "Wayland committed a batch after its selection changed");
    next.tree->close();
}
std::size_t panels(const CanvasFrame& frame){std::size_t result=frame.panels.size();for(const auto& layer:frame.overlays)result+=layer.panels.size();return result;}
std::size_t vertical_lines(const CanvasFrame& frame){std::size_t result=0;const auto count=[&](const auto& values){for(const auto& panel:values)if(panel.bounds.width==1&&panel.bounds.height>2)++result;};count(frame.panels);for(const auto& layer:frame.overlays)count(layer.panels);return result;}
void preedit_presentation(){
    Host host;auto field=take(host.tree->input_field({},"field",{.text="A"},{},{}));
    (void)take(host.tree->drawing_snapshot({200,40}));good(field.focus());select(host,{1,0});WaylandImeProtocol ime;good(ime.start(host));
    WaylandImeBatch batch;batch.preedit="한😀";batch.cursor_begin=7;batch.cursor_end=3;good(ime.apply(host,batch));
    auto state=host.focus();check(state->text_selection==TextRange{2,2}&&state->text_selection_reversed,
                                  "Wayland preedit lost its selection direction");
    check(!host.tree->input({.kind=WidgetInputKind::text,.text="x",.composition_cursor_hidden=true}),
          "A non-composition event accepted a hidden composition caret");
    check(!host.tree->input({.kind=WidgetInputKind::composition,.text="x",.selection_reversed=true}),
          "A reversed composition without a selection was accepted");
    check(host.focus()->marked_range==state->marked_range&&host.focus()->text_selection==state->text_selection&&
          host.focus()->text_selection_reversed&&take(field.snapshot()).text=="A",
          "Invalid presentation events changed the active composition");
    batch.cursor_begin=-1;batch.cursor_end=-1;good(ime.apply(host,batch));auto hidden=take(host.tree->drawing_snapshot({200,40}));
    batch.cursor_begin=7;batch.cursor_end=7;good(ime.apply(host,batch));auto shown=take(host.tree->drawing_snapshot({200,40}));
    check(panels(shown)==panels(hidden)+1,"Wayland hidden caret was painted or its underline disappeared");
    batch={};batch.committed="글";good(ime.apply(host,batch));auto committed=take(host.tree->drawing_snapshot({200,40}));
    check(!host.focus()->marked_range&&vertical_lines(committed)==vertical_lines(hidden)+1,"Wayland caret remained hidden after commit");
    host.tree->close();
}
}
int main(){composition();surrounding_delete();selected_text();privacy_and_bounds();generation();malformed();session_and_selection();marked_deletion();unicode_window_and_read_race();invalid_sessions();change_before_delivery();preedit_presentation();}
