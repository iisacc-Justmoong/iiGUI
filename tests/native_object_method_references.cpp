#include "NativeObjectMethodReferencesRuntime.generated.h"
#include "NativeObjectMethodPostfixRuntime.generated.h"
#include "../src/Widgets/WidgetState.h"
#include <iostream>
#include <stdexcept>
using namespace ii;
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
template<class T>T take(core::Result<T> result){if(!result){if(result.error().cause)std::rethrow_exception(result.error().cause);throw std::runtime_error(result.error().message);}return std::move(*result);}
void good(core::Result<void> result){if(!result){if(result.error().cause)std::rethrow_exception(result.error().cause);throw std::runtime_error(result.error().message);}}
template<class View>struct Mounted final {
    std::shared_ptr<gui::detail::WidgetState> state=std::make_shared<gui::detail::WidgetState>([](runtime::Task task){task();});
    gui::Widget host=take(state->content_host({},"root",{640,480}));std::shared_ptr<typename View::Scope> scope;
    Mounted(){good(host.set_content([&](gui::WidgetBuilder& widgets){return View::build(widgets,{}, {},&scope);}));}
    ~Mounted(){state->close();scope.reset();}
    auto& model(){return scope->backend_0;}
    gui::Widget widget(std::string_view id){std::string found;for(const auto& node:take(state->snapshots()))if(node.id==id||node.id.ends_with("/"+std::string(id))){check(found.empty(),"A method widget id was ambiguous");found=node.id;}check(!found.empty(),"Method widget not found");return take(state->find(found));}
    void click(std::string_view id){good(widget(id).activate());}
    std::string text(std::string_view id){return take(widget(id).snapshot()).text;}
};
using Main=lvui_generated::NativeObjectMethodReferencesRuntime;
using Postfix=lvui_generated::NativeObjectMethodPostfixRuntime;
void initial(){Mounted<Main> m;check(m.model().first->calls()==0&&m.model().first->factories==1&&m.text("description")=="First 0","Mounting invoked a deferred method or duplicated its factory");}
void selection(){
    Mounted<Main> m;auto made=NativeObjectMethodReferenceBackend::created;
    for(auto id:{"direct","alias","grouped","indexed","getter","typed","factory"})m.click(id);
    check(m.model().first->calls()==7&&m.text("description")=="First 7","Initial method receivers or typed forwarding were wrong");
    m.model().selected.set(m.model().second);
    for(auto id:{"direct","alias","grouped","getter","typed","factory"})m.click(id);
    m.click("indexed");check(m.model().second->calls()==6&&m.model().first->calls()==8&&m.text("description")=="Second 6"&&NativeObjectMethodReferenceBackend::created==made,"Receiver replacement changed backend identity or reused an obsolete callable");
}
void payload(){Mounted<Main> m;auto field=m.widget("payload");good(field.focus());good(m.state->input({.kind=gui::WidgetInputKind::text,.text="First edit"}));check(m.model().first->last=="First edit","A nested String method lost its payload");m.model().selected.set(m.model().second);good(field.set_text(""));good(m.state->input({.kind=gui::WidgetInputKind::text,.text="Second edit"}));check(m.model().second->last=="Second edit","A typed component callback retained its old item");}
void preserve(){Mounted<Main> m;auto edit=m.widget("edit");good(edit.set_text("Working"));good(edit.focus());const auto count=take(m.state->snapshots()).size();auto action=m.widget("direct");auto id=take(action.snapshot()).id;m.model().selected.set(m.model().second);m.click("direct");auto value=take(edit.snapshot());check(value.text=="Working"&&value.focused&&take(m.state->snapshots()).size()==count&&take(action.snapshot()).id==id&&m.model().file_revision.get()==0,"Method selection rebuilt the editing UI or changed file state");}
void failures(){Mounted<Main> m;auto action=m.widget("direct");m.model().selected.set({});good(action.activate());check(m.model().first->calls()==1&&!take(m.scope->native_lifetime.lock()->failures()).empty(),"A failed method selection lost the last valid action");m.model().selected.set(m.model().second);good(action.activate());check(m.model().second->calls()==1&&m.text("description")=="Second 1"&&take(m.scope->native_lifetime.lock()->failures()).empty(),"A nested method selection did not recover");}
void fixed(){Mounted<Main> m;m.model().selected.set(m.model().second);m.click("pinned");m.click("manual");m.click("direct");check(m.model().first->calls()==2&&m.model().second->calls()==1,"Static and mutable initial method selections followed a reactive receiver");}
void conditional(){Mounted<Main> m;auto old=m.widget("conditional");m.model().visible.set(false);check(!old.snapshot(),"A conditional method consumer did not retire");m.model().selected.set(m.model().second);m.model().visible.set(true);m.click("conditional");check(m.model().second->calls()==1,"A reactivated method consumer retained its old receiver");}
void parent(){Mounted<Postfix> m;std::weak_ptr<DeferredDomain::Parent> old=m.model().parent.get();auto next=std::make_shared<DeferredDomain::Parent>();m.model().parent.get()->resident.during=[&]{m.model().parent.set(next);check(!old.expired(),"An active subobject method lost its shared parent");};m.click("parent");check(old.expired(),"A replaced method subobject retained its old parent");m.click("parent");check(next->resident.calls()==1,"A subobject method did not select the new parent");}
void temporary(){Mounted<Postfix> m;check(DeferredDomain::Temporary::live==1&&m.model().temporary_calls==0,"Selecting a temporary receiver invoked or destroyed it");m.click("temporary");m.click("temporary");check(m.model().temporary_calls==2&&m.text("count")=="2"&&DeferredDomain::Temporary::live==1,"An immovable temporary method lost its value category or resource lifetime");good(m.host.clear_content());check(DeferredDomain::Temporary::live==0,"A removed view retained its temporary method receiver");}
void retirement(){Mounted<Main> m;auto action=m.widget("direct");std::weak_ptr<Main::Scope> old=m.scope;m.model().first->during=[&]{m.scope.reset();m.state->close();check(!old.expired(),"Self-removal destroyed the admitted method owner");};good(action.activate());check(old.expired()&&!action.snapshot(),"An outside method consumer retained a retired activation");}
void owned_parent(){Mounted<Postfix> m;check(DeferredDomain::OwnedParent::live==2,"A deferred subobject lost its temporary parent before invocation");m.click("owned_parent");m.click("owned_getter");check(m.model().owned_parent_calls==2&&DeferredDomain::OwnedParent::live==2,"Temporary-parent member/getter references lost their ownership");good(m.host.clear_content());check(DeferredDomain::OwnedParent::live==0,"A closed method reference retained a temporary parent");}
void indexed_value(){Mounted<Postfix> m;m.click("indexed_value");m.click("indexed_value");check(m.model().indexed[0].calls==2,"A deferred indexed method cloned its noncopyable source element");}
void receiver_condition(){Mounted<Main> m;m.click("receiver_condition");m.model().visible.set(false);m.click("receiver_condition");check(m.model().first->calls()==1&&m.model().second->calls()==1,"A conditional method reference ignored its selected receiver");}
int main(int argc,char** argv){try{std::string name=argc>1?argv[1]:"initial";if(name=="initial")initial();else if(name=="selection")selection();else if(name=="payload")payload();else if(name=="preserve")preserve();else if(name=="failures")failures();else if(name=="fixed")fixed();else if(name=="conditional")conditional();else if(name=="parent")parent();else if(name=="temporary")temporary();else if(name=="retirement")retirement();else if(name=="owned_parent")owned_parent();else if(name=="indexed_value")indexed_value();else if(name=="receiver_condition")receiver_condition();else throw std::runtime_error("Unknown generated method reference test");check(NativeObjectMethodReferenceBackend::live==0&&DeferredDomain::Item::live==0&&DeferredDomain::Temporary::live==0&&DeferredDomain::OwnedParent::live==0,"Generated method references leaked an activation or receiver");std::cout<<"Generated object method reference "<<name<<" passed\n";}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
