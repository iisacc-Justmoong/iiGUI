#include "NativeNavigatedRepetitionRuntime.generated.h"
#include "../src/Widgets/WidgetState.h"
#include <iostream>
#include <stdexcept>
using namespace ii;
namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void good(core::Result<void> value){if(!value){if(value.error().cause)std::rethrow_exception(value.error().cause);throw std::runtime_error(value.error().message);}}
template<class T>T take(core::Result<T> value){if(!value){if(value.error().cause)std::rethrow_exception(value.error().cause);throw std::runtime_error(value.error().message);}return std::move(*value);}
struct Fixture final {
    using View=lvui_generated::NativeNavigatedRepetitionRuntime;
    std::shared_ptr<gui::detail::WidgetState> state=std::make_shared<gui::detail::WidgetState>([](runtime::Task task){task();});
    gui::Widget host,list,scroll;std::shared_ptr<View::Scope> scope;
    explicit Fixture(std::string initial={}){
        NativeNavigationBackend::on_activate={};host=take(state->content_host({},"root",{640,420}));
        good(host.set_content([&](gui::WidgetBuilder& builder){return View::build(builder,{}, [&](View::Scope& owner){if(!initial.empty())owner.backend_1.set_current(initial);},&scope);}));
        list=take(host.content_item("results"));scroll=take(host.content_item("results/viewport"));(void)take(scroll.snapshot());good(list.focus());
    }
    ~Fixture(){NativeNavigationBackend::on_activate={};state->close();}
    auto& nav(){return scope->backend_1;}
    auto& model(){return scope->backend_0;}
    gui::Widget item(std::string_view key,std::string_view id){return take(list.keyed_content_item(key,id));}
    void key(gui::WidgetKey key,bool shift=false,bool control=false){good(state->input({.kind=gui::WidgetInputKind::key_down,.key=key,.shift=shift,.control=control}));}
};
void navigation(){
    Fixture f;const auto revision=f.model().items.revision();f.key(gui::WidgetKey::end);
    check(f.nav().state.get().current_key=="999"&&f.nav().is_selected("999")&&f.list.keyed_content_item("999","row").has_value(),
          "End did not activate and reveal an offscreen logical item");
    check(take(f.scroll.snapshot()).scroll_offset>0&&take(f.item("999","selected").snapshot()).checked&&
          take(take(f.host.content_item("current")).snapshot()).text=="999","Logical navigation failed to reach reactive row and inspector views");
    f.key(gui::WidgetKey::left);check(f.nav().state.get().current_key=="997","Keyboard navigation did not skip an ineligible item");
    f.key(gui::WidgetKey::home);check(f.nav().state.get().current_key=="0"&&take(f.scroll.snapshot()).scroll_offset==0,"Home did not return to the first logical row");
    check(f.model().items.revision()==revision&&f.model().file_revision.get()==0,"Keyboard navigation changed the file/model revision");
    check(take(f.state->snapshots()).size()<140,"Offscreen navigation instantiated the entire model");
}
void selection(){
    Fixture f;f.key(gui::WidgetKey::end);f.key(gui::WidgetKey::up,true);
    check(f.nav().state.get().selected_keys==std::vector<std::string>{"996","997","999"},"Shift range selected an ineligible key or lost its anchor");
    auto original=f.model().items.get();f.model().items.set({original[0],original[1],original[2]});
    f.key(gui::WidgetKey::select_all,false,true);
    check(f.nav().is_selected("999")&&f.nav().is_selected("0")&&f.nav().state.get().selected_keys.size()==6,
          "Filtering and selecting the current projection discarded hidden selections");
    f.key(gui::WidgetKey::escape);check(f.nav().state.get().selected_keys.empty(),"Escape did not clear collection selection");
}
void programmatic(){
    Fixture f("500");
    check(f.list.keyed_content_item("500","row").has_value()&&take(f.scroll.snapshot()).scroll_offset>0,"A C++ focus publication did not reveal its logical item");
    f.key(gui::WidgetKey::enter);
    check(f.model().activations==std::vector<std::string>{"500"},"Enter did not call the C++ activation endpoint exactly once");
    f.model().focus_events.clear();f.key(gui::WidgetKey::right);
    check(f.model().focus_events==std::vector<std::string>{"501"},"A focus command published duplicate model callbacks");
}
void controls(){
    Fixture f;good(f.item("0","text").focus());f.key(gui::WidgetKey::right);
    check(f.nav().state.get().current_key.empty(),"Collection navigation intercepted a nested text field's arrow key");
    good(f.item("3","action").focus());f.key(gui::WidgetKey::down);
    check(f.nav().state.get().current_key=="6","Navigation ignored the row owning the focused native control");
    good(f.item("6","action").focus());f.key(gui::WidgetKey::enter);
    good(f.state->input({.kind=gui::WidgetInputKind::key_up,.key=gui::WidgetKey::enter}));
    check(f.model().button_actions==1&&f.model().activations.empty(),"A collection command stole native button activation");
    const auto selected=f.nav().state.get().selected_keys;
    good(f.item("6","text").focus());f.key(gui::WidgetKey::escape);
    check(f.nav().state.get().selected_keys==selected,"Collection Escape cleared selection while a text field owned input");
}
void source(){
    Fixture f;
    const auto identity=take(f.item("0","row").snapshot()).id;
    auto values=f.model().items.get();std::swap(values[500],values[999]);f.model().items.set(values);
    check(take(f.item("0","row").snapshot()).id==identity,"An offscreen source reorder remounted an unchanged visible row");
    f.key(gui::WidgetKey::end);
    check(f.nav().state.get().current_key=="500"&&f.list.keyed_content_item("500","row").has_value(),
          "Navigation reused a stale source when only offscreen items changed");
    f.nav().set_current("999");
    check(f.list.keyed_content_item("999","row").has_value(),"A key moved in an offscreen reorder could not be revealed");
}
void pointer(){
    Fixture f;const auto bounds=take(f.item("1","row").snapshot()).bounds;
    good(f.state->input({.kind=gui::WidgetInputKind::pointer_down,.x=bounds.x+10,.y=bounds.y+5,.buttons=1}));
    good(f.state->input({.kind=gui::WidgetInputKind::pointer_up,.x=bounds.x+10,.y=bounds.y+5,.buttons=1}));
    check(f.nav().state.get().current_key=="1"&&f.nav().is_selected("1"),"A row background click did not select and focus its key");
    f.key(gui::WidgetKey::escape);f.model().select_on_click.set(false);
    const auto first=take(f.item("0","row").snapshot()).bounds;
    good(f.state->input({.kind=gui::WidgetInputKind::pointer_down,.x=first.x+10,.y=first.y+5,.buttons=1}));
    good(f.state->input({.kind=gui::WidgetInputKind::pointer_up,.x=first.x+10,.y=first.y+5,.buttons=1}));
    check(f.nav().state.get().current_key=="0"&&f.nav().state.get().selected_keys.empty(),"Focus-only click selected an item");
    good(f.state->input({.kind=gui::WidgetInputKind::pointer_down,.x=bounds.x+10,.y=bounds.y+5,.meta=true,.buttons=1}));
    good(f.state->input({.kind=gui::WidgetInputKind::pointer_up,.x=bounds.x+10,.y=bounds.y+5,.meta=true,.buttons=1}));
    check(f.nav().is_selected("1"),"Command-click did not add a selection when ordinary clicks only focus");
    const auto third=take(f.item("3","row").snapshot()).bounds;
    good(f.state->input({.kind=gui::WidgetInputKind::pointer_down,.x=third.x+10,.y=third.y+5,.shift=true,.buttons=1}));
    good(f.state->input({.kind=gui::WidgetInputKind::pointer_up,.x=third.x+10,.y=third.y+5,.shift=true,.buttons=1}));
    check(f.nav().state.get().selected_keys==std::vector<std::string>{"1","2","3"},"Shift-click did not select a range in focus-only mode");
    check(f.model().file_revision.get()==0,"Native collection interaction modified the document");
}
void lifetime(){
    Fixture f;f.key(gui::WidgetKey::end);std::weak_ptr<Fixture::View::Scope> retired=f.scope;
    NativeNavigationBackend::on_activate=[&]{f.state->close();};f.key(gui::WidgetKey::enter);f.scope.reset();
    check(retired.expired()&&!f.list.snapshot(),"An activation callback or selection observation retained its retired owner");
}
}
int main(int argc,char** argv){try{
    const std::string name=argc>1?argv[1]:"navigation";
    if(name=="navigation")navigation();else if(name=="selection")selection();else if(name=="programmatic")programmatic();else if(name=="controls")controls();else if(name=="pointer")pointer();else if(name=="lifetime")lifetime();else if(name=="source")source();else throw std::runtime_error("Unknown native navigated repetition test");
    std::cout<<"Native navigated repetition "<<name<<" passed without a window\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
