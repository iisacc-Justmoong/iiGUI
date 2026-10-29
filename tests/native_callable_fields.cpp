#include "NativeCallableFieldsRuntime.generated.h"
#include "NativeCallableFieldPostfixRuntime.generated.h"
#include "../src/Widgets/WidgetState.h"
#include <iostream>
using namespace ii;
namespace {
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
template<class T>T take(core::Result<T> result){if(!result)throw std::runtime_error(result.error().message);return std::move(*result);}
void good(core::Result<void> result){if(!result)throw std::runtime_error(result.error().message);}
using Main=lvui_generated::NativeCallableFieldsRuntime;
using Postfix=lvui_generated::NativeCallableFieldPostfixRuntime;
template<class View>struct Mounted final {
    std::shared_ptr<gui::detail::WidgetState> state=std::make_shared<gui::detail::WidgetState>([](runtime::Task task){task();});
    gui::Widget host=take(state->content_host({},"root",{640,480}));std::shared_ptr<typename View::Scope> scope;
    Mounted(){good(host.set_content([&](gui::WidgetBuilder& widgets){return View::build(widgets,{}, {},&scope);}));}
    ~Mounted(){if(state)state->close();scope.reset();}
    NativeCallableFieldBackend& model(){return scope->backend_0;}
    gui::Widget widget(std::string_view name){std::string found;for(const auto& node:take(state->snapshots()))if(node.id==name||node.id.ends_with("/"+std::string(name))){check(found.empty(),"A callable field widget id was ambiguous");found=node.id;}check(!found.empty(),"Callable field widget not found");return take(state->find(found));}
    void click(std::string_view name){good(widget(name).activate());}
    std::string text(std::string_view name){return take(widget(name).snapshot()).text;}
};
void initial(){Mounted<Main> m;check(*m.model().run.get()->count==0&&m.text("description")=="First 4","Selecting a callable field invoked it or lost its typed result");m.click("direct");m.click("typed");check(*m.model().run.get()->count==2,"Direct and typed fields did not use the native callable");}
void updates(){Mounted<Main> m;auto old=m.model().run.get();auto next=std::make_shared<CallableFieldDomain::Handler>();m.model().run.set(next);m.click("direct");m.click("typed");check(*old->count==0&&*next->count==2,"Callable consumers kept the previous published handle");m.model().format.set([](auto value){return "Second "+std::to_string(value);});check(m.text("description")=="Second 4"&&m.text("frozen")=="First 4"&&m.text("manual")=="First 4","Reactive, Static and variable field selections were confused");}
void payload(){Mounted<Main> m;auto field=m.widget("payload");good(field.focus());good(m.state->input({.kind=gui::WidgetInputKind::text,.text="First"}));check(m.model().last=="First","A typed callable field lost the String payload");m.model().edited.set([&](auto value){m.model().last="New "+value;});good(field.set_text(""));good(m.state->input({.kind=gui::WidgetInputKind::text,.text="Second"}));check(m.model().last=="New Second","A field payload consumer did not select a replacement");}
void preserve(){Mounted<Main> m;auto edit=m.widget("edit");good(edit.set_text("Working"));good(edit.focus());const auto count=take(m.state->snapshots()).size();auto id=take(m.widget("direct").snapshot()).id;m.model().run.set(std::make_shared<CallableFieldDomain::Handler>());m.click("direct");auto snapshot=take(edit.snapshot());check(snapshot.text=="Working"&&snapshot.focused&&take(m.state->snapshots()).size()==count&&take(m.widget("direct").snapshot()).id==id&&m.model().file_revision.get()==0,"Callable publication rebuilt the UI or changed file state");}
void errors(){Mounted<Main> m;auto old=m.model().run.get();m.model().run.set({});m.click("direct");check(*old->count==1&&!take(m.scope->native_lifetime.lock()->failures()).empty(),"An empty handle discarded the previous widget callback");m.model().run.set(std::make_shared<CallableFieldDomain::Handler>());m.click("direct");check(*m.model().run.get()->count==1&&take(m.scope->native_lifetime.lock()->failures()).empty(),"A callable widget did not recover");m.model().format.set({});check(m.text("description")=="First 4","An empty function removed the last valid display result");m.model().format.set([](auto value){return std::to_string(value);});check(m.text("description")=="4","A function field did not recover its displayed result");}
void inflight(){Mounted<Main> m;std::weak_ptr<CallableFieldDomain::Handler> old=m.model().run.get();auto next=std::make_shared<CallableFieldDomain::Handler>();m.model().run.get()->during=[&]{m.model().run.set(next);check(!old.expired(),"Publishing a new field destroyed an active widget callback");m.click("typed");};m.click("direct");check(old.expired()&&*next->count==1,"Widget reentry confused callable field versions");}
void borrowed(){Mounted<Main> m;m.click("borrowed");m.click("borrowed");check(m.model().borrowed.count.get()==2&&m.text("borrowed_count")=="2","A noncopyable field was cloned or lost its native value publication");}
void publication(){Mounted<Main> m;m.click("readable");check(m.model().last=="First","A readable widget lost its initial callable");m.click("change_readable");m.click("readable");check(m.model().last=="Second"&&m.model().file_revision.get()==0,"A non-observable readable widget ignored the backend publication");}
void conditional(){Mounted<Main> m;auto previous=m.widget("conditional");m.model().visible.set(false);check(!previous.snapshot(),"A callable field consumer failed to retire");m.model().run.set(std::make_shared<CallableFieldDomain::Handler>());m.model().visible.set(true);m.click("conditional");check(*m.model().run.get()->count==1,"A reactivated field selected an obsolete version");}
void postfix(){Mounted<Postfix> m;auto previous=m.model().selected.get();auto next=std::make_shared<CallableFieldDomain::Node>();m.model().selected.set(next);m.click("nested");check(*previous->run.get()->count==0&&*next->run.get()->count==1,"A nested callable field retained an obsolete parent");check(CallableFieldDomain::Parent::live==1&&m.model().temporary_calls==0,"An owned temporary callable parent was destroyed or invoked during selection");m.click("temporary");m.click("temporary");check(m.model().temporary_calls==2&&m.text("count")=="2","An owned parent field lost its identity or publication");good(m.host.clear_content());check(CallableFieldDomain::Parent::live==0,"Retired fields retained an owned temporary parent");}
void retirement(){Mounted<Main> m;auto action=m.widget("direct");std::weak_ptr<Main::Scope> old=m.scope;std::weak_ptr<CallableFieldDomain::Handler> handler=m.model().run.get();m.model().run.get()->during=[&]{m.scope.reset();m.state->close();check(!old.expired()&&!handler.expired(),"Self-removal destroyed the admitted field owner");};good(action.activate());check(old.expired()&&handler.expired()&&!action.snapshot(),"An outside field consumer retained a retired activation");}
}
int main(int argc,char** argv){try{
    const std::string name=argc>1?argv[1]:"initial";
    if(name=="initial")initial();else if(name=="updates")updates();else if(name=="payload")payload();else if(name=="preserve")preserve();else if(name=="errors")errors();
    else if(name=="inflight")inflight();else if(name=="borrowed")borrowed();else if(name=="publication")publication();else if(name=="conditional")conditional();else if(name=="postfix")postfix();else if(name=="retirement")retirement();
    else throw std::runtime_error("Unknown generated callable field test");
    check(NativeCallableFieldBackend::live==0&&CallableFieldDomain::Handler::live==0&&CallableFieldDomain::Parent::live==0,"Generated callable fields leaked native resources");std::cout<<"Generated callable field "<<name<<" passed\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
