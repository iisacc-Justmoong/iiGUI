#include "../src/Platform/WaylandImeSession.h"
#include "../src/Platform/WaylandInputQueue.h"
#include "../src/Widgets/WidgetState.h"
#include <memory>
#include <stdexcept>
#include <variant>

using namespace ii;
using namespace ii::gui;
using namespace ii::gui::detail;
namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(core::Result<T> value){if(!value)throw std::runtime_error(value.error().message);return std::move(*value);}
void good(core::Result<void> value){if(!value)throw std::runtime_error(value.error().message);}
struct Host final {
    std::shared_ptr<WidgetState> tree=std::make_shared<WidgetState>([](runtime::Task task){task();});
    std::optional<WidgetSnapshot> focus(){auto snapshots=tree->accessibility_snapshots();if(!snapshots)return {};
        for(auto& value:*snapshots)if(value.focused&&value.text_input&&value.visible&&value.enabled&&!value.input_blocked)return value;return {};}
    core::Result<TextContent> read(std::string_view id,TextRange range){auto widget=tree->find(id);if(!widget)return std::unexpected(widget.error());return widget->text_content(range);}
    core::Result<void> send(WidgetInput event){return tree->input(std::move(event));}
};
struct Wire final {
    std::optional<std::uint64_t> focused=1;std::uint32_t serial=0;
    std::vector<WaylandImePublication> sent;unsigned disabled=0;
    std::optional<std::uint64_t> focus_epoch()const{return focused;}
    std::optional<std::uint32_t> publish(std::uint64_t epoch,const WaylandImePublication& value){
        if(!focused||*focused!=epoch)return {};sent.push_back(value);return ++serial;}
    std::optional<std::uint32_t> disable(std::uint64_t epoch){if(!focused||*focused!=epoch)return {};++disabled;return ++serial;}
};
void selection(Host& host,TextRange range){good(host.tree->input({.kind=WidgetInputKind::text_selection,.replacement_range=range}));}
void ordered_queue(){
    WaylandInputQueue queue(3);WidgetInput motion{.kind=WidgetInputKind::pointer_move,.x=1};
    check(queue.push(motion),"Cannot queue motion");motion.x=2;check(queue.push(motion),"Cannot coalesce motion");
    WaylandImeEvent enter{.kind=WaylandImeEventKind::enter,.epoch=7};check(queue.push(enter),"Cannot queue IME enter");
    motion.x=3;check(queue.push(motion),"Cannot queue motion after IME event");
    check(!queue.push(WidgetInput{.kind=WidgetInputKind::pointer_up}),"Wayland input queue ignored its capacity");
    auto a=queue.pop(),b=queue.pop(),c=queue.pop();
    check(a&&b&&c&&std::get<WidgetInput>(*a).x==2&&std::get<WaylandImeEvent>(*b).epoch==7&&
          std::get<WidgetInput>(*c).x==3&&queue.empty(),"Wayland IME and pointer ordering was not preserved");
    check(queue.push(WidgetInput{.kind=WidgetInputKind::pointer_down,.buttons=1},42),
          "Cannot queue a Wayland window gesture");
    auto press=queue.pop();
    check(press && std::get<WaylandPointerEvent>(*press).serial==42 &&
          std::get<WaylandPointerEvent>(*press).input.kind==WidgetInputKind::pointer_down,
          "Wayland must retain the exact initiating input serial");
    check(queue.push(WidgetInput{.kind=WidgetInputKind::pointer_down,.buttons=1},0),
          "The compositor may provide a zero input serial");
    auto zero=queue.pop();
    check(zero && std::get<WaylandPointerEvent>(*zero).serial==0,
          "A zero Wayland serial was confused with an absent serial");
}
void wire_batch(){
    WaylandImeCapture capture;good(capture.preedit("ㅎ",3,3));good(capture.preedit("한",3,3));
    good(capture.commit("A"));good(capture.commit("B"));capture.erase(3,4);capture.erase(1,2);
    auto batch=capture.done();
    check(batch.preedit=="한"&&batch.committed=="B"&&batch.cursor_begin==3&&batch.before==1&&batch.after==2,
          "Wayland pending events were not replaced before done");
    auto empty=capture.done();check(!empty.preedit&&!empty.committed&&!empty.before&&!empty.after,
                                    "Wayland done did not reset its pending state");
    good(capture.preedit(nullptr,0,0));good(capture.commit(nullptr));batch=capture.done();
    check(batch.preedit==""&&!batch.committed,"Wayland nullable strings were not handled safely");
    const std::string too_long(65537,'x');check(!capture.commit(too_long.c_str()),"Wayland captured an unbounded string");
    capture.reset();check(!capture.done().committed,"Wayland focus reset kept a pending string");
}
void basic_session(){
    Host host;Wire wire;auto field=take(host.tree->input_field({},"field",{.text="A한😀Z"},{},{}));
    (void)take(host.tree->drawing_snapshot({300,40}));good(field.focus());selection(host,{2,0});
    WaylandImeSession session;session.enter(host,wire,1);
    check(wire.serial==1&&wire.sent.size()==1&&wire.sent[0].enable&&wire.sent[0].surrounding&&
          wire.sent[0].surrounding->cursor==4,"Wayland session did not publish the focused editor");
    WaylandImeBatch batch;batch.preedit="글";batch.cursor_begin=3;batch.cursor_end=3;
    session.done(host,wire,{.kind=WaylandImeEventKind::done,.epoch=1,.serial=1,.batch=batch});
    check(host.focus()->marked_range==TextRange{2,1},"Wayland done did not reach the widget");
    batch={};batch.committed="한";
    session.done(host,wire,{.kind=WaylandImeEventKind::done,.epoch=1,.serial=wire.serial,.batch=batch});
    check(take(field.snapshot()).text=="A한한😀Z"&&!host.focus()->marked_range,"Wayland result was not committed");
    session.leave(host,wire,1);session.done(host,wire,{.kind=WaylandImeEventKind::done,.epoch=1,.serial=wire.serial,.batch=batch});
    check(take(field.snapshot()).text=="A한한😀Z","Wayland input survived surface leave");host.tree->close();
}
void focus_and_serial(){
    Host host;Wire wire;auto root=take(host.tree->absolute_stack({},"root",{300,80}));
    auto first=take(host.tree->input_field(root,"first",{.text="A"},{},{}));
    auto second=take(host.tree->input_field(root,"second",{.text="B"},{},{}));
    (void)take(host.tree->drawing_snapshot({300,80}));good(first.focus());WaylandImeSession session;session.enter(host,wire,1);
    const auto first_serial=wire.serial;good(second.focus());session.refresh(host,wire);
    const auto second_serial=wire.serial;check(second_serial>first_serial&&wire.sent.back().enable,"Editor focus did not restart the protocol");
    WaylandImeBatch old;old.committed="bad";session.done(host,wire,{.kind=WaylandImeEventKind::done,.epoch=1,.serial=first_serial,.batch=old});
    check(take(second.snapshot()).text=="B","A stale batch reached another editor");
    good(first.focus());session.refresh(host,wire);const auto aba_serial=wire.serial;
    session.done(host,wire,{.kind=WaylandImeEventKind::done,.epoch=1,.serial=first_serial,.batch=old});
    check(take(first.snapshot()).text=="A","A stale batch survived focus ABA");
    selection(host,{0,0});session.refresh(host,wire);const auto newest=wire.serial;
    check(newest>aba_serial,"Changed selection was not sent");
    WaylandImeBatch older;older.committed="x";const auto sent=wire.sent.size();
    session.done(host,wire,{.kind=WaylandImeEventKind::done,.epoch=1,.serial=aba_serial,.batch=older});
    check(take(first.snapshot()).text=="xA"&&wire.sent.size()==sent,
          "A mismatched serial was dropped or published pending state too early");
    session.done(host,wire,{.kind=WaylandImeEventKind::done,.epoch=1,.serial=newest});
    check(wire.sent.size()>sent,"Matching serial did not publish pending editor state");
    host.tree->close();
}
void password_and_retirement(){
    Host host;Wire wire;auto field=take(host.tree->input_field({},"secret",{.text="secret",.password=true},{},{}));
    (void)take(host.tree->drawing_snapshot({300,40}));good(field.focus());WaylandImeSession session;session.enter(host,wire,1);
    check(wire.sent.size()==1&&wire.sent[0].password&&!wire.sent[0].surrounding,"Wayland session disclosed password context");
    WaylandImeBatch batch;batch.preedit="한";batch.cursor_begin=3;batch.cursor_end=3;
    session.done(host,wire,{.kind=WaylandImeEventKind::done,.epoch=1,.serial=wire.serial,.batch=batch});
    check(host.focus()->marked_range.has_value(),"Password preedit missing");session.retire(host,wire);
    check(wire.disabled==1&&!host.focus()->marked_range,"Retired surface kept IME state or candidate");
    host.tree->close();
}
void context_support_changes(){
    Host host;Wire wire;auto field=take(host.tree->input_field({},"field",{.text=std::string(5000,'a')},{},{}));
    (void)take(host.tree->drawing_snapshot({300,40}));good(field.focus());selection(host,{0,5000});
    WaylandImeSession session;session.enter(host,wire,1);
    check(wire.sent.size()==1&&wire.sent.back().enable&&!wire.sent.back().surrounding,
          "An oversized selection unexpectedly provided surrounding text");
    const auto old=wire.serial;
    selection(host,{2500,0});session.refresh(host,wire);
    check(wire.sent.size()==2&&wire.sent.back().enable&&wire.sent.back().surrounding,
          "Wayland did not re-enable newly supported surrounding text");
    WaylandImeBatch stale;stale.committed="bad";
    session.done(host,wire,{.kind=WaylandImeEventKind::done,.epoch=1,.serial=old,.batch=stale});
    check(take(field.snapshot()).text==std::string(5000,'a'),"A batch survived a reset of the Wayland input context");
    selection(host,{0,5000});session.refresh(host,wire);
    check(wire.sent.size()==3&&wire.sent.back().enable&&!wire.sent.back().surrounding,
          "Wayland retained old surrounding text after support became unavailable");
    host.tree->close();
}
void sensitive_change_while_waiting(){
    Host host;Wire wire;auto field=take(host.tree->input_field({},"field",{.text="AB"},{},{}));
    (void)take(host.tree->drawing_snapshot({300,40}));good(field.focus());WaylandImeSession session;session.enter(host,wire,1);
    const auto old=wire.serial;selection(host,{0,0});session.refresh(host,wire);const auto pending=wire.serial;
    WaylandImeBatch batch;batch.committed="x";session.done(host,wire,{.kind=WaylandImeEventKind::done,.epoch=1,.serial=old,.batch=batch});
    const auto count=wire.sent.size();good(field.set_input_options({.text="xAB",.password=true}));session.refresh(host,wire);
    check(wire.sent.size()==count+1&&wire.sent.back().enable&&wire.sent.back().password&&!wire.sent.back().surrounding,
          "Wayland deferred a transition to sensitive input");
    batch.committed="bad";session.done(host,wire,{.kind=WaylandImeEventKind::done,.epoch=1,.serial=pending,.batch=batch});
    check(take(field.snapshot()).text=="xAB","A pre-password batch reached the sensitive editor");
    host.tree->close();
}
void context_change_during_preedit(){
    Host host;Wire wire;auto field=take(host.tree->input_field({},"field",{.text=std::string(5000,'a')},{},{}));
    (void)take(host.tree->drawing_snapshot({300,40}));good(field.focus());selection(host,{0,5000});
    WaylandImeSession session;session.enter(host,wire,1);const auto serial=wire.serial;
    WaylandImeBatch batch;batch.preedit="한";batch.cursor_begin=3;batch.cursor_end=3;
    session.done(host,wire,{.kind=WaylandImeEventKind::done,.epoch=1,.serial=serial,.batch=batch});
    check(host.focus()->marked_range&&wire.sent.size()==1,"Wayland restarted input while preedit was active");
    batch={};batch.committed="한";session.done(host,wire,{.kind=WaylandImeEventKind::done,.epoch=1,.serial=serial,.batch=batch});
    check(take(field.snapshot()).text=="한"&&wire.sent.size()==2&&wire.sent.back().enable&&wire.sent.back().surrounding,
          "Wayland did not publish newly available context after the preedit committed");
    host.tree->close();
}
}
int main(){ordered_queue();wire_batch();basic_session();focus_and_serial();password_and_retirement();context_support_changes();sensitive_change_while_waiting();context_change_during_preedit();}
