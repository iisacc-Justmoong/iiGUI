#include "NativeObjectInputsRuntime.generated.h"
#include "NativeObjectHandlesRuntime.generated.h"
#include "NativeObjectRootRuntime.generated.h"
#include "NativeObjectRowsRuntime.generated.h"
#include "../src/iiGUI/WidgetState.h"
#include <iostream>
#include <stdexcept>
using namespace ii;
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
template<class T>T take(core::Result<T> value){if(!value){if(value.error().cause)std::rethrow_exception(value.error().cause);throw std::runtime_error(value.error().message);}return std::move(*value);}
void good(core::Result<void> value){if(!value){if(value.error().cause)std::rethrow_exception(value.error().cause);throw std::runtime_error(value.error().message);}}
template<class View>struct Mounted final {
    std::shared_ptr<gui::detail::WidgetState> state=std::make_shared<gui::detail::WidgetState>([](runtime::Task task){task();});
    gui::Widget host=take(state->content_host({},"root",{640,480}));
    std::shared_ptr<typename View::Scope> scope;
    Mounted(){good(host.set_content([&](gui::WidgetBuilder& builder){return View::build(builder,{}, {},&scope);}));}
    ~Mounted(){state->close();scope.reset();}
    auto& model(){return scope->backend_0;}
    gui::Widget widget(std::string_view suffix){for(const auto& item:take(state->snapshots()))if(item.id==suffix||item.id.ends_with("/"+std::string(suffix)))return take(state->find(item.id));throw std::runtime_error("Object input widget not found");}
    std::string text(std::string_view suffix){return take(widget(suffix).snapshot()).text;}
    bool has(std::string_view suffix){for(const auto& item:take(state->snapshots()))if(item.id==suffix||item.id.ends_with("/"+std::string(suffix)))return true;return false;}
};
using Values=lvui_generated::NativeObjectInputsRuntime;
using Handles=lvui_generated::NativeObjectHandlesRuntime;
void values(){
    Mounted<Values> m;check(m.text("title")=="Computed 3"&&m.text("count")=="3"&&m.text("default")=="Computed 3"&&m.model().computations==1,"A typed native value was recomputed per consumer or lost its dependent default");
    m.model().count.set(6);
    check(m.text("title")=="Computed 6"&&m.text("count")=="6"&&m.text("default")=="Computed 6"&&m.text("pinned")=="Computed 3"&&m.model().computations==2,"A forwarded native value did not share one reevaluation or changed its Static projection");
}
void records(){
    Mounted<lvui_generated::NativeObjectRootRuntime> m;check(m.text("root-title")=="Record","A typed input could not compose the root Window");
    m.model().record.set({"Changed",8});check(m.text("root-title")=="Changed","A noncomparable record input did not publish at the root");
}
void handles(){
    Mounted<Handles> m;m.model().first->title.set("Changed");m.model().first->count.set(9);m.model().editStable(7);
    check(m.text("title")=="Changed"&&m.text("default")=="Changed"&&m.text("count")=="9"&&m.text("local")=="9"&&m.text("borrowed")=="7"&&m.text("pointer")=="7","Typed owning, borrowed or pointer inputs lost member observation");
    check(m.model().file_revision.get()==0&&m.scope->revision_0.get()==0,"A native input changed document or fallback backend state");
}
void selection(){
    Mounted<Handles> m;auto previous=m.model().first;m.model().selected.set(m.model().second);m.model().second->count.set(10);previous->title.set("Obsolete");
    check(m.text("title")=="Second"&&m.text("default")=="Second"&&m.text("count")=="10"&&m.text("local")=="10","A typed input failed to replace its member subscriptions");
}
void nulls(){
    Mounted<Handles> m;m.model().selected.set({});
    check(m.text("title")=="First"&&m.text("default")=="First","A null typed input erased valid content");
    m.model().selected.set(m.model().second);check(m.text("title")=="Second"&&m.text("default")=="Second","A typed input did not recover after invalid replacement");
}
void invalid_initial(){
    Mounted<Handles> m;auto before=take(m.state->snapshots()).size();
    auto factories=lvui::native::BackendFactories{}.with<NativeObjectInputModel>([](const auto&){return NativeObjectInputModel(true);});
    auto replacement=m.host.set_content([&](gui::WidgetBuilder& builder){return Handles::build(builder,{}, {},nullptr,factories);});
    check(!replacement&&m.text("title")=="First"&&take(m.state->snapshots()).size()==before&&m.scope->active(),"An invalid initial typed input replaced the previous valid activation");
    check(NativeObjectInputModel::live==1,"Failed native input candidate leaked its backend");
}
void local_state(){
    Mounted<Values> m;auto field=m.widget("edit");good(field.set_text("Working"));good(field.focus());auto nodes=take(m.state->snapshots()).size();m.model().count.set(5);auto snapshot=take(field.snapshot());
    check(snapshot.text=="Working"&&snapshot.focused&&take(m.state->snapshots()).size()==nodes&&m.model().file_revision.get()==0,"A typed input update rebuilt local state or changed the document");
}
void retirement(){
    Mounted<Handles> m;std::weak_ptr<const NativeInput::Item> previous=m.model().first;m.model().rows.set({});m.model().first.reset();m.model().selected.set(m.model().second);
    check(previous.expired(),"A replaced typed input retained the obsolete owner");auto owner=m.scope;good(m.host.clear_content());check(!owner->active(),"A typed input activation remained live after retirement");
}
void conditional(){
    Mounted<Handles> m;check(m.has("conditional"),"Typed conditional input was missing");m.model().visible.set(false);check(!m.has("conditional"),"Typed conditional input failed to retire");
    m.model().selected.set(m.model().second);m.model().visible.set(true);check(m.text("conditional")=="Second","Typed conditional reactivation used obsolete data");
}
void rows(){
    Mounted<lvui_generated::NativeObjectRowsRuntime> m;std::size_t count=0;for(const auto& item:take(m.state->snapshots()))if(item.id.ends_with("/row-title"))++count;check(count==2,"Typed row inputs lost keyed composition");
    m.model().first->title.set("Changed");m.model().rows.set({m.model().second,m.model().first});bool found=false;for(const auto& item:take(m.state->snapshots()))if(item.id.ends_with("/row-title")&&item.text=="Changed")found=true;check(found,"Typed row inputs lost member observation after reorder");
}
int main(int argc,char** argv){try{std::string name=argc>1?argv[1]:"values";
    if(name=="values")values();else if(name=="records")records();else if(name=="handles")handles();else if(name=="selection")selection();else if(name=="nulls")nulls();else if(name=="invalid_initial")invalid_initial();else if(name=="local_state")local_state();else if(name=="retirement")retirement();else if(name=="conditional")conditional();else if(name=="rows")rows();else throw std::runtime_error("Unknown typed native input test");
    check(NativeObjectInputModel::live==0&&NativeInput::Item::live==0,"Typed native inputs leaked their owner");std::cout<<"Native object inputs "<<name<<" passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
