#include "NativeVirtualRepetitionRuntime.generated.h"
#include "../src/iiGUI/WidgetState.h"
#include <iostream>
#include <stdexcept>
using namespace ii;
namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void good(core::Result<void> value){if(!value){if(value.error().cause)std::rethrow_exception(value.error().cause);throw std::runtime_error(value.error().message);}}
template<class T>T take(core::Result<T> value){if(!value){if(value.error().cause)std::rethrow_exception(value.error().cause);throw std::runtime_error(value.error().message);}return std::move(*value);}
struct Fixture final {
    using View=lvui_generated::NativeVirtualRepetitionRuntime;
    std::shared_ptr<gui::detail::WidgetState> state=std::make_shared<gui::detail::WidgetState>([](runtime::Task task){task();});
    gui::Widget host,list,scroll;
    std::shared_ptr<View::Scope> scope;
    Fixture(){
        NativeRowActivation::constructions=NativeRowActivation::destructions=0;NativeRowActivation::fail_next=false;NativeRowActivation::on_construct={};
        host=take(state->content_host({},"root",{640,480}));
        good(host.set_content([&](gui::WidgetBuilder& builder){return View::build(builder,{}, {},&scope);}));
        list=take(host.content_item("results"));scroll=take(host.content_item("results/viewport"));
        (void)take(scroll.snapshot());
    }
    ~Fixture(){NativeRowActivation::on_construct={};state->close();}
    gui::Widget item(std::string_view key,std::string_view id){return take(list.keyed_content_item(key,id));}
};
void viewport(){
    Fixture f;
    auto initial=take(f.scroll.snapshot());
    check(initial.bounds.width==480&&initial.bounds.height==170&&initial.content_height==30050,"Generated viewport lost its specified dimensions or full logical range");
    check(take(f.list.keyed_content_keys()).size()==9&&NativeRowActivation::constructions==9,"Initial virtualization constructed the whole model or speculative row backends");
    const auto revision=f.scope->backend_0.items.revision();
    auto action=f.item("3","hit");const auto identity=take(action.snapshot()).id;good(action.activate());
    good(f.scroll.set_scroll_offset(90));
    check(take(f.list.keyed_content_keys()).size()==12&&take(action.snapshot()).id==identity&&take(action.snapshot()).text=="1",
          "Scrolling lost overlapping row identity or local state");
    const auto epoch=take(f.list.snapshot()).content_generation;const auto created=NativeRowActivation::constructions;
    good(f.scroll.set_scroll_offset(91));
    check(take(f.list.snapshot()).content_generation==epoch&&NativeRowActivation::constructions==created,
          "One pixel of scrolling rebuilt the native collection wrapper or row backends");
    check(take(f.item("3","row").snapshot()).bounds.y==-1&&f.scope->backend_0.items.revision()==revision&&f.scope->revision_0.get()==0,
          "Viewport movement used document revisions or failed to reposition retained cells");
    good(f.scroll.set_scroll_offset(9000));
    check(!action.snapshot()&&take(f.list.keyed_content_keys()).size()<=12&&take(f.state->snapshots()).size()<80,
          "Offscreen row activations accumulated during scrolling");
}
void anchor(){
    Fixture f;good(f.scroll.set_scroll_offset(901));
    auto action=f.item("30","hit");const auto identity=take(action.snapshot()).id;good(action.activate());
    auto values=f.scope->backend_0.items.get();
    values.insert(values.begin(),{{"new-a","New A",{}},{"new-b","New B",{}},{"new-c","New C",{}}});
    f.scope->backend_0.items.set(values);
    check(take(f.scroll.snapshot()).scroll_offset==991&&take(action.snapshot()).id==identity&&take(action.snapshot()).text=="1"&&
          take(f.item("30","ordinal").snapshot()).text=="33","New results changed the review anchor, selection state or source ordinal");
    good(f.scroll.set_scroll_offset(0));
    check(take(f.scroll.snapshot()).scroll_offset==0,"A stale anchor overrode deliberate user scrolling");
}
void resize(){
    Fixture f;good(f.scroll.set_scroll_offset(901));
    auto action=f.item("30","hit");const auto identity=take(action.snapshot()).id;
    f.scope->backend_0.width.set(320);
    check(take(f.scroll.snapshot()).bounds.width==320&&take(f.scroll.snapshot()).scroll_offset==1351&&
          take(action.snapshot()).id==identity&&take(f.item("30","row").snapshot()).bounds.y==-1,
          "Responsive two-column layout failed to preserve the leading image and its pixel offset");
    check(take(f.list.keyed_content_keys()).size()<=8,"Narrowing retained unnecessary grid columns");
    f.scope->backend_0.height.set(350);
    check(take(f.list.keyed_content_keys()).size()<=12&&take(f.scroll.snapshot()).bounds.height==350,"A taller viewport did not update its visible range");
}
void rollback(){
    Fixture f;
    auto old=f.item("0","hit");const auto identity=take(old.snapshot()).id;
    auto values=f.scope->backend_0.items.get();values[999].id="0";
    f.scope->backend_0.items.set(values);
    check(take(old.snapshot()).id==identity&&take(f.list.keyed_content_keys()).size()==9,"An offscreen duplicate partially replaced visible content");
    values[999].id="999";f.scope->backend_0.items.set(values);
    NativeRowActivation::fail_next=true;good(f.scroll.set_scroll_offset(900));
    check(old.snapshot().has_value()&&take(f.list.keyed_content_keys()).front()=="0","A failed row constructor discarded the working viewport");
    good(f.scroll.set_scroll_offset(901));
    check(!old.snapshot()&&take(f.list.keyed_content_keys()).front()=="27","A virtual collection did not recover after a failed row constructor");
}
void lifetime(){
    Fixture f;
    std::weak_ptr<Fixture::View::Scope> retired=f.scope;
    auto external=f.scope->backend_0.items.get();
    good(f.scroll.set_scroll_offset(29900));
    f.state->close();f.scope.reset();
    check(retired.expired()&&NativeRowActivation::constructions==NativeRowActivation::destructions&&!f.scroll.snapshot(),
          "Viewport subscriptions retained the retired owner or an offscreen backend");
}
void horizontal(){
    Fixture f;
    f.scope->backend_0.horizontal.set(true);
    auto initial=take(f.scroll.snapshot());
    check(initial.content_width==159990&&initial.content_height==170,"A generated horizontal layout kept grid extents");
    good(f.scroll.set_horizontal_scroll_offset(320));
    check(take(f.list.keyed_content_keys()).front()=="1"&&take(f.list.keyed_content_keys()).size()==5&&
          take(f.item("2","row").snapshot()).bounds.x==initial.bounds.x,"Horizontal cells did not use viewport-relative positions");
}
void empty(){
    Fixture f;good(f.scroll.set_scroll_offset(900));
    auto old=f.item("30","hit");
    f.scope->backend_0.items.set({});
    check(take(f.list.keyed_content_keys()).empty()&&!old.snapshot()&&take(f.scroll.snapshot()).content_height==0&&
          take(f.scroll.snapshot()).scroll_offset==0,"An empty source retained rows, a stale anchor or a phantom scroll range");
    f.scope->backend_0.items.set({{"new","New",{}}});
    check(take(f.list.keyed_content_keys())==std::vector<std::string>{"new"}&&take(f.item("new","title").snapshot()).text=="New",
          "The empty viewport failed to accept a later result");
}
void reentry(){
    Fixture f;
    NativeRowActivation::on_construct=[&]{good(f.scroll.set_scroll_offset(1800));};
    auto values=f.scope->backend_0.items.get();
    values.insert(values.begin(),{{"new-a","New A",{}},{"new-b","New B",{}},{"new-c","New C",{}}});
    f.scope->backend_0.items.set(values);
    check(take(f.scroll.snapshot()).scroll_offset==1800&&take(f.item("57","row").snapshot()).bounds.y==0,
          "Row activation overwrote newer native scroll input with a stale frame");
}
void autoheight(){
    Fixture f;
    f.scope->backend_0.height.set(0);
    check(take(f.scroll.snapshot()).bounds.height==take(f.host.snapshot()).bounds.height&&take(f.list.keyed_content_keys()).size()>9,
          "A zero-height virtual viewport collapsed instead of using the surrounding layout");
    f.scope->backend_0.height.set(170);
    check(take(f.scroll.snapshot()).bounds.height==170&&take(f.list.keyed_content_keys()).size()==9,
          "Restoring an explicit height retained the automatic flex allocation");
}
}
int main(int argc,char** argv){try{
    const std::string name=argc>1?argv[1]:"viewport";
    if(name=="viewport")viewport();else if(name=="anchor")anchor();else if(name=="resize")resize();else if(name=="rollback")rollback();else if(name=="lifetime")lifetime();else if(name=="horizontal")horizontal();else if(name=="empty")empty();else if(name=="reentry")reentry();else if(name=="autoheight")autoheight();
    else throw std::runtime_error("Unknown native virtual repetition test");
    std::cout<<"Native virtual repetition "<<name<<" passed without a window\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
