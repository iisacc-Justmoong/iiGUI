#include "NativeMethodQueriesRuntime.generated.h"
#include "NativeMethodPostfixRuntime.generated.h"
#include "../src/Widgets/WidgetState.h"
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
    gui::Widget widget(std::string_view suffix){for(const auto& item:take(state->snapshots()))if(item.id==suffix||item.id.ends_with("/"+std::string(suffix)))return take(state->find(item.id));throw std::runtime_error("Method query widget not found");}
    std::string text(std::string_view suffix){return take(widget(suffix).snapshot()).text;}
    bool has(std::string_view suffix){for(const auto& item:take(state->snapshots()))if(item.id==suffix||item.id.ends_with("/"+std::string(suffix)))return true;return false;}
};
using View=lvui_generated::NativeMethodQueriesRuntime;
void initial(){Mounted<View> m;for(auto name:{"direct","nested","alias","typed","parameter","conditional","pinned"})check(m.text(name)=="First 5","An initial C++ method receiver or parameter was wrong");}
void updates(){
    Mounted<View> m;m.model().first->set(7);for(auto name:{"direct","nested","alias","typed","parameter","conditional"})check(m.text(name)=="First 9","A private Observable read inside a C++ getter did not update its view");
    check(m.text("pinned")=="First 5"&&m.model().file_revision.get()==0&&m.scope->revision_0.get()==0,"A method query changed Static state or document/backend revisions");
}
void arguments(){Mounted<View> m;m.model().amount.set(4);check(m.text("typed")=="First 7"&&m.text("parameter")=="First 7"&&m.text("pinned")=="First 5","Native method arguments lost their reactive or Static contract");}
void selection(){Mounted<View> m;auto previous=m.model().first;m.model().selected.set(m.model().second);m.model().second->set(10);previous->set(99);for(auto name:{"direct","nested","alias","typed","parameter"})check(m.text(name)=="Second 12","Native method selection retained the old receiver or dependencies");}
void branch(){Mounted<View> m;m.model().first->alternate(true);check(m.text("parameter")=="First 10","A C++ getter branch did not become active");const auto calls=m.model().first->queries;m.model().first->set(11);check(m.model().first->queries==calls,"An inactive private C++ property still recomputed the view");m.model().first->other(12);check(m.text("typed")=="First 14","The new C++ getter branch lost its publication");}
void failures(){Mounted<View> m;m.model().first->set(-1);check(m.text("direct")=="First 5"&&!take(m.scope->native_lifetime.lock()->failures()).empty(),"A failed C++ getter replaced valid content or hid its error");m.model().first->set(6);check(m.text("direct")=="First 8"&&take(m.scope->native_lifetime.lock()->failures()).empty(),"A failed C++ getter could not recover");}
void local_state(){Mounted<View> m;auto field=m.widget("edit");good(field.set_text("Working"));good(field.focus());auto nodes=take(m.state->snapshots()).size();m.model().first->set(6);auto snapshot=take(field.snapshot());check(snapshot.text=="Working"&&snapshot.focused&&take(m.state->snapshots()).size()==nodes,"A native method update rebuilt the editing view");}
void reentry(){Mounted<View> m;std::weak_ptr<MethodDomain::Item> previous=m.model().first;auto first=m.model().first;first->on_query=[&]{m.model().rows.set({m.model().second});m.model().selected.set(m.model().second);m.model().first.reset();};first->set(4);first.reset();check(previous.expired()&&m.text("direct")=="Second 5"&&m.text("parameter")=="Second 5","Reentrant native method evaluation used an invalid receiver or kept an obsolete one");}
void conditional(){Mounted<View> m;m.model().visible.set(false);check(!m.has("conditional"),"Native method conditional content failed to retire");m.model().selected.set(m.model().second);m.model().second->set(7);m.model().visible.set(true);check(m.text("conditional")=="Second 9","Native method reactivation read an old receiver");}
void postfix(){Mounted<lvui_generated::NativeMethodPostfixRuntime> m;m.model().first->set(7);check(m.text("grouped")=="First 9"&&m.text("indexed")=="First 9"&&m.text("result")=="7"&&m.text("rvalue")=="First 7","Grouped, indexed or ref-qualified returned-value C++ method paths did not observe changes");}
int main(int argc,char** argv){try{std::string name=argc>1?argv[1]:"initial";if(name=="initial")initial();else if(name=="updates")updates();else if(name=="arguments")arguments();else if(name=="selection")selection();else if(name=="branch")branch();else if(name=="failures")failures();else if(name=="local_state")local_state();else if(name=="reentry")reentry();else if(name=="conditional")conditional();else if(name=="postfix")postfix();else throw std::runtime_error("Unknown native method query test");check(NativeMethodQueryModel::live==0&&MethodDomain::Item::live==0,"Native method queries leaked a receiver");std::cout<<"Native method queries "<<name<<" passed\n";}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
