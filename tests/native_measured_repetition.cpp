#include "NativeMeasuredRepetitionRuntime.generated.h"
#include "../src/Widgets/WidgetState.h"
#include <iostream>
#include <stdexcept>
using namespace ii;
namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void good(core::Result<void> value){if(!value){if(value.error().cause)std::rethrow_exception(value.error().cause);throw std::runtime_error(value.error().message);}}
template<class T>T take(core::Result<T> value){if(!value){if(value.error().cause)std::rethrow_exception(value.error().cause);throw std::runtime_error(value.error().message);}return std::move(*value);}
struct Fixture final {
    using View=lvui_generated::NativeMeasuredRepetitionRuntime;
    std::shared_ptr<gui::detail::WidgetState> state=std::make_shared<gui::detail::WidgetState>([](runtime::Task task){task();});
    gui::Widget host,list,scroll;std::shared_ptr<View::Scope> scope;
    explicit Fixture(bool blank=false){
        NativeRowActivation::constructions=NativeRowActivation::destructions=0;NativeRowActivation::fail_next=false;NativeRowActivation::on_construct={};
        host=take(state->content_host({},"root",{640,420}));
        good(host.set_content([&](gui::WidgetBuilder& builder){return View::build(builder,{}, [blank](View::Scope& owner){
            if(blank){auto values=owner.backend_0.items.get();values.resize(200);for(auto& item:values)item->title.set("");owner.backend_0.items.set(values);owner.backend_0.spacing.set(0);}
        },&scope);}));
        list=take(host.content_item("results"));scroll=take(host.content_item("results/viewport"));(void)take(scroll.snapshot());
    }
    ~Fixture(){state->close();}
    gui::Widget root(std::string_view key){return take(list.keyed_content_item(key));}
    gui::Widget row(std::string_view key){return take(root(key).content_item("row"));}
    gui::Widget action(std::string_view key){return take(root(key).content_item("hit"));}
};
void measured(){
    Fixture f;
    auto a=f.row("0"),b=f.row("1");
    const auto first=take(a.snapshot()).bounds,second=take(b.snapshot()).bounds;
    check(first.height>second.height&&first.height==take(a.measure(first.width)).height&&second.y==first.y+first.height+2,
          "Native rows did not use their actual wrapped content height");
    check(take(f.list.keyed_content_keys()).size()<20&&take(f.state->snapshots()).size()<160,"Measured virtualization instantiated the entire model");
}
void changing(){
    Fixture f;
    auto first=f.row("0"),next=f.row("1"),action=f.action("0");
    const auto identity=take(action.snapshot()).id;good(action.activate());
    const auto before=take(first.snapshot()).bounds.height;
    const auto revision=f.scope->backend_0.items.revision();
    f.scope->backend_0.items.get()[0]->title.set(std::string(400,'W'));
    (void)take(f.scroll.snapshot());
    const auto height=take(first.snapshot()).bounds.height;
    check(height>before&&take(next.snapshot()).bounds.y==height+2&&take(action.snapshot()).id==identity&&take(action.snapshot()).text=="1",
          "A live text change failed to resize retained rows or lost local state");
    check(f.scope->backend_0.items.revision()==revision&&f.scope->revision_0.get()==0,"Natural-size feedback changed the document/model revision");
}
void anchor(){
    Fixture f;good(f.scroll.set_scroll_offset(600));
    auto keys=take(f.list.keyed_content_keys());std::string leading;gui::Rect before;
    for(const auto& key:keys){auto bounds=take(f.row(key).snapshot()).bounds;if(bounds.y<=0&&bounds.y+bounds.height>0){leading=key;before=bounds;break;}}
    check(!leading.empty()&&keys.front()!=leading,"Anchor fixture did not retain an overscan row above the visible content");
    auto kept=f.action(leading);const auto identity=take(kept.snapshot()).id;good(kept.activate());
    const auto offset=take(f.scroll.snapshot()).scroll_offset;
    f.scope->backend_0.items.get()[std::stoul(keys.front())]->title.set(std::string(400,'W'));
    (void)take(f.scroll.snapshot());
    const auto after=take(f.row(leading).snapshot()).bounds;
    check(after.y==before.y&&take(f.scroll.snapshot()).scroll_offset>offset&&take(kept.snapshot()).id==identity&&take(kept.snapshot()).text=="1","Measuring a larger overscan row shifted the review anchor or failed to update its logical offset");
}
void resize(){
    Fixture f;const auto initial=take(f.row("0").snapshot()).bounds.height;
    f.scope->backend_0.width.set(120);
    check(take(f.row("0").snapshot()).bounds.height>initial,"Narrowing did not remeasure wrapped content");
    f.scope->backend_0.width.set(240);f.scope->backend_0.cell_width.set(100);
    const auto first=take(f.row("0").snapshot()).bounds,second=take(f.row("1").snapshot()).bounds;
    check(first.y==second.y&&first.height==second.height&&first.width==119,"A measured grid did not align cells to its tallest content");
    f.scope->backend_0.measured.set(false);
    check(take(f.row("0").snapshot()).bounds.height==80,"Disabling measurement kept stale natural-size feedback");
    f.scope->backend_0.measured.set(true);
    check(take(f.row("0").snapshot()).bounds.height>80,"Re-enabling measurement did not restore natural grid geometry");
}
void zero(){
    Fixture f;
    auto values=f.scope->backend_0.items.get();values.resize(200);f.scope->backend_0.items.set(values);
    f.scope->backend_0.spacing.set(0);f.scope->backend_0.empty.set(true);
    check(take(f.scroll.snapshot()).content_height==0&&take(f.list.keyed_content_keys()).size()<=1,"Zero-height rows did not settle or kept phantom scroll space");
    f.scope->backend_0.empty.set(false);
    check(take(f.scroll.snapshot()).content_height>0&&take(f.list.keyed_content_keys()).front()=="0","A zero-height row could not reactivate all of its content at the original viewport");
}
void lifetime(){
    Fixture f;good(f.scroll.set_scroll_offset(600));
    auto actor=f.scope->backend_0.items.get()[10];std::weak_ptr<Fixture::View::Scope> retired=f.scope;
    f.state->close();f.scope.reset();actor->title.set("Late");
    check(retired.expired()&&NativeRowActivation::constructions==NativeRowActivation::destructions,"Measurement subscriptions retained a retired owner or local backend");
}
void offscreen(){
    Fixture f(true);
    check(take(f.scroll.snapshot()).content_height==0&&take(f.list.keyed_content_keys()).size()<=1,"An empty model retained all invisible row activations");
    f.scope->backend_0.items.get()[0]->title.set("Returned");
    f.scope->backend_0.geometry_revision.set(1);
    check(take(f.scroll.snapshot()).content_height>0&&take(f.list.keyed_content_keys())==std::vector<std::string>{"0"}&&
          take(f.row("0").snapshot()).bounds.height>0,"A native layout revision failed to revive an offscreen shared item or retained zero-height rows");
}
void navigation(){
    Fixture f;
    f.scope->backend_1.set_current("901");
    auto current=take(f.row("901").snapshot()).bounds;
    check(current.height>0&&current.y>=0&&current.y+current.height<=180,
          "Measured feedback moved a programmatically revealed short row outside the viewport");
    good(f.list.focus());
    good(f.state->input({.kind=gui::WidgetInputKind::key_down,.key=gui::WidgetKey::end}));
    check(f.scope->backend_1.state.get().current_key=="999"&&f.list.keyed_content_item("999").has_value(),
          "End could not reveal a measured row through a conditional content generation");
    check(take(f.state->snapshots()).size()<160&&f.scope->backend_0.geometry_revision.get()==0,
          "Measured navigation instantiated the full source or wrote model geometry state");
}
}
int main(int argc,char** argv){try{
    const std::string name=argc>1?argv[1]:"measured";
    if(name=="measured")measured();else if(name=="changing")changing();else if(name=="anchor")anchor();else if(name=="resize")resize();else if(name=="zero")zero();else if(name=="lifetime")lifetime();else if(name=="offscreen")offscreen();else if(name=="navigation")navigation();else throw std::runtime_error("Unknown native measured repetition test");
    std::cout<<"Native measured repetition "<<name<<" passed without a window\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
