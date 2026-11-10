#include <iiGUI/WaylandConnection.h>
#include <iiGUI/WidgetState.h>
#include <iiGUI/WidgetSurfaceNative.h>
#include <memory>
#include <stdexcept>
#include <vector>

using namespace ii;
using namespace ii::gui;
using namespace ii::gui::detail;

void fake_wayland_text_enter(WaylandConnection&);
std::uint32_t fake_wayland_text_serial(const WaylandConnection&);
std::uint64_t fake_wayland_text_epoch(const WaylandConnection&);
unsigned fake_wayland_text_disabled(const WaylandConnection&);
std::vector<WaylandImePublication> fake_wayland_text_publications(const WaylandConnection&);

namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(core::Result<T> value){if(!value)throw std::runtime_error(value.error().message);return std::move(*value);}
void good(core::Result<void> value){if(!value)throw std::runtime_error(value.error().message);}
void deliver(const std::shared_ptr<WidgetState>& tree,WaylandImeEvent event){
    const WaylandSurfaceEvent native{WaylandEventKind::ime,nullptr,&event};check(tree->native_event(&native),"Wayland surface rejected IME event");
}
void integration(){
    WaylandConnection connection;connection.open({},{});connection.dispatch();fake_wayland_text_enter(connection);
    auto first=std::make_shared<WidgetState>([](runtime::Task task){task();});
    auto field=take(first->input_field({},"first",{.text="A"},{},{}));good(first->attach(&connection, make_native_widget_surface));good(field.focus());first->present();
    auto published=fake_wayland_text_publications(connection);
    check(published.size()==1&&published.back().enable&&published.back().surrounding&&published.back().surrounding->text=="A",
          "Wayland surface did not publish its focused input field");
    const auto original_serial=fake_wayland_text_serial(connection);
    const auto original_epoch=fake_wayland_text_epoch(connection);
    WaylandImeBatch preedit;preedit.preedit="한";preedit.cursor_begin=3;preedit.cursor_end=3;
    deliver(first,{.kind=WaylandImeEventKind::done,.epoch=original_epoch,.serial=original_serial,.batch=preedit});
    check(take(field.snapshot()).marked_range.has_value(),"Wayland surface did not deliver preedit");
    WaylandImeBatch committed;committed.committed="글";
    deliver(first,{.kind=WaylandImeEventKind::done,.epoch=original_epoch,.serial=fake_wayland_text_serial(connection),.batch=committed});
    check(take(field.snapshot()).text=="A글","Wayland surface did not commit input");
    auto next=std::make_shared<WidgetState>([](runtime::Task task){task();});
    auto replacement=take(next->input_field({},"second",{.text="B"},{},{}));good(next->attach(&connection, make_native_widget_surface));good(replacement.focus());next->present();
    const auto disabled=fake_wayland_text_disabled(connection);first->close();
    check(fake_wayland_text_disabled(connection)==disabled,"A retired surface disabled its replacement");
    deliver(next,{.kind=WaylandImeEventKind::done,.epoch=original_epoch,.serial=original_serial,.batch=committed});
    check(take(replacement.snapshot()).text=="B","An old surface batch reached the replacement");
    deliver(next,{.kind=WaylandImeEventKind::done,.epoch=fake_wayland_text_epoch(connection),.serial=fake_wayland_text_serial(connection),.batch=committed});
    check(take(replacement.snapshot()).text=="B글","The replacement surface did not receive its own input");
    next->close();
}
void updated_geometry(){
    WaylandConnection connection;connection.open({},{});connection.dispatch();fake_wayland_text_enter(connection);
    WidgetSnapshot editor{};editor.id="field";editor.focused=true;editor.text_input=true;
    editor.text_revision=TextInputRevision{.identity=std::make_shared<int>(1),.focus=1};
    editor.caret_bounds={1,2,1,10};
    NativeWidgetSurface surface(&connection,{
        .frame=[&](Size){editor.caret_bounds={10,20,1,10};return CanvasFrame{};},
        .input=[](WidgetInput){},.accessibility=[&]{return std::vector<WidgetSnapshot>{editor};},
        .text_content=[](std::string_view,TextRange range)->core::Result<TextContent>{return TextContent{range,{}};},
        .text_service=[](WidgetInput)->core::Result<void>{return {};}});
    surface.flush();auto sent=fake_wayland_text_publications(connection);
    check(!sent.empty()&&sent.back().caret.x==10&&sent.back().caret.y==20,
          "Wayland IME kept the caret position from before layout");
}
void replacement_rollback(){
    WaylandConnection connection;connection.open({},{});connection.dispatch();fake_wayland_text_enter(connection);
    auto original=std::make_shared<WidgetState>([](runtime::Task task){task();});
    auto field=take(original->input_field({},"old",{.text="A"},{},{}));good(original->attach(&connection, make_native_widget_surface));good(field.focus());original->present();
    auto candidate=std::make_shared<WidgetState>([](runtime::Task task){task();});
    auto other=take(candidate->input_field({},"new",{.text="B"},{},{}));good(candidate->attach(&connection, make_native_widget_surface));good(other.focus());candidate->present();
    const auto before=fake_wayland_text_publications(connection).size();candidate->close();
    const WaylandSurfaceEvent repaint{WaylandEventKind::resize};check(original->native_event(&repaint),"Cannot restore original Wayland surface");original->present();
    const auto sent=fake_wayland_text_publications(connection);
    check(sent.size()>before&&sent.back().enable&&sent.back().surrounding&&sent.back().surrounding->text=="A",
          "Wayland failed to restore the old editor after mount rollback");
    WaylandImeBatch batch;batch.committed="Z";
    deliver(original,{.kind=WaylandImeEventKind::done,.epoch=fake_wayland_text_epoch(connection),
                      .serial=fake_wayland_text_serial(connection),.batch=batch});
    check(take(field.snapshot()).text=="AZ","Restored Wayland editor did not accept input");
    original->close();
}
void ordinary_keyboard_without_text_protocol(){
    WaylandConnection connection;connection.open({},{});connection.dispatch();
    auto tree=std::make_shared<WidgetState>([](runtime::Task task){task();});
    auto field=take(tree->input_field({},"field",{.text="A"},{},{}));good(tree->attach(&connection, make_native_widget_surface));good(field.focus());tree->present();
    WidgetInput input{.kind=WidgetInputKind::text,.text="x"};const WaylandSurfaceEvent event{WaylandEventKind::input,&input};
    check(tree->native_event(&event)&&take(field.snapshot()).text=="Ax"&&fake_wayland_text_publications(connection).empty(),
          "Wayland without text-input-v3 lost ordinary keyboard text");tree->close();
}
}
int main(){integration();updated_geometry();replacement_rollback();ordinary_keyboard_without_text_protocol();}
