#include "NativeFactoryCapturesRuntime.generated.h"
#include "NativeFactoryCapturePostfixRuntime.generated.h"
#include "../src/iiGUI/WidgetState.h"
#include <iostream>
using namespace ii;
namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(core::Result<T> result){if(!result){if(result.error().cause)std::rethrow_exception(result.error().cause);throw std::runtime_error(result.error().message);}return std::move(*result);}
void good(core::Result<void> result){if(!result){if(result.error().cause)std::rethrow_exception(result.error().cause);throw std::runtime_error(result.error().message);}}
using Main=lvui_generated::NativeFactoryCapturesRuntime;
using Postfix=lvui_generated::NativeFactoryCapturePostfixRuntime;
template<class View>struct Mounted final {
    std::shared_ptr<gui::detail::WidgetState> state=std::make_shared<gui::detail::WidgetState>([](runtime::Task task){task();});
    gui::Widget host=take(state->content_host({},"root",{640,480}));std::shared_ptr<typename View::Scope> scope;
    Mounted(){good(host.set_content([&](gui::WidgetBuilder& widgets){return View::build(widgets,{}, {},&scope);}));}
    ~Mounted(){state->close();scope.reset();}
    auto& model(){return scope->backend_0;}
    gui::Widget widget(std::string_view id){std::string found;for(const auto& node:take(state->snapshots()))if(node.id==id||node.id.ends_with("/"+std::string(id))){check(found.empty(),"A factory widget id was ambiguous");found=node.id;}check(!found.empty(),"Factory widget not found");return take(state->find(found));}
    void click(std::string_view id){good(widget(id).activate());}
    std::string text(std::string_view id){return take(widget(id).snapshot()).text;}
};
void temporary(){Mounted<Postfix> m;check(FactoryCaptureDomain::Node::live==4,"A returned callable lost its temporary receiver before invocation");m.click("temporary");m.click("owned_temporary");good(m.host.clear_content());check(FactoryCaptureDomain::Node::live==2,"Retired factory versions retained their temporary receivers");}
void argument(){Mounted<Postfix> m;check(FactoryCaptureDomain::Argument::live==2,"A returned callable lost its temporary argument before invocation");m.click("argument");check(m.model().last=="Argument","A callable factory lost its borrowed argument contents");good(m.host.clear_content());check(FactoryCaptureDomain::Argument::live==0,"A retired factory retained its temporary argument");}
void adaptive(){Mounted<Postfix> m;check(FactoryCaptureDomain::Argument::live==2,"A by-value immovable argument was retained or a borrowed argument was discarded");m.click("mixed");check(m.model().last=="Argument/Argument","Mixed native argument passing lost mandatory copy elision or borrowed storage");good(m.host.clear_content());check(FactoryCaptureDomain::Argument::live==0,"A mixed argument factory retained temporary storage");}
void selection(){Mounted<Main> m;check(m.model().first->count.get()==0,"Selecting a returned callable invoked its operation");m.model().selected.set(m.model().second);m.click("direct");m.click("value");m.click("typed");check(m.model().second->count.get()==3&&m.text("description")=="Second 3","Returned borrowed/value factories did not follow receiver selection");}
void inflight(){Mounted<Main> m;auto previous=std::make_shared<FactoryCaptureDomain::Node>("Independent");m.model().selected.set(previous);std::weak_ptr<FactoryCaptureDomain::Node> old=previous;auto next=std::make_shared<FactoryCaptureDomain::Node>("Next");previous->during=[&]{m.model().selected.set(next);check(!old.expired(),"Replacement destroyed an active callable factory receiver");m.click("typed");};previous.reset();m.click("direct");check(old.expired()&&next->count.get()==1,"An old factory receiver leaked or reentry used the wrong version");}
void preserve(){Mounted<Main> m;auto edit=m.widget("edit");good(edit.set_text("Working"));good(edit.focus());const auto count=take(m.state->snapshots()).size();auto id=take(m.widget("direct").snapshot()).id;m.model().selected.set(m.model().second);m.click("direct");auto value=take(edit.snapshot());check(value.text=="Working"&&value.focused&&take(m.state->snapshots()).size()==count&&take(m.widget("direct").snapshot()).id==id&&m.model().file_revision.get()==0,"Factory selection changed editing state, widget identity or file revision");}
void failures(){Mounted<Main> m;m.model().selected.set({});m.click("direct");check(m.model().first->count.get()==1&&!take(m.scope->native_lifetime.lock()->failures()).empty(),"A failed factory selection discarded its last valid version");m.model().selected.set(m.model().second);m.click("direct");check(m.model().second->count.get()==1&&take(m.scope->native_lifetime.lock()->failures()).empty(),"A factory did not recover after receiver selection returned");}
void fixed(){Mounted<Main> m;m.model().selected.set(m.model().second);m.click("pinned");m.click("manual");m.click("direct");check(m.model().first->count.get()==2&&m.model().second->count.get()==1,"Static or variable factory selections followed a new receiver");}
void payload(){Mounted<Main> m;m.model().selected.set(m.model().second);auto edit=m.widget("payload");good(edit.focus());good(m.state->input({.kind=gui::WidgetInputKind::text,.text="Native"}));check(m.model().second->last=="Native"&&m.model().first->last.empty(),"A returned String handler lost its selected receiver");}
void indexed(){Mounted<Main> m;m.model().rows.set({m.model().second});m.click("indexed");check(m.model().second->count.get()==1,"An indexed factory retained its old receiver");}
void conditional(){Mounted<Main> m;auto old=m.widget("conditional");m.model().visible.set(false);check(!old.snapshot(),"A factory consumer failed to retire");m.model().selected.set(m.model().second);m.model().visible.set(true);m.click("conditional");check(m.model().second->count.get()==1,"A reactivated factory consumer selected an obsolete receiver");}
void retirement(){Mounted<Main> m;auto action=m.widget("direct");std::weak_ptr<Main::Scope> old=m.scope;m.model().first->during=[&]{m.scope.reset();m.state->close();check(!old.expired(),"Self-removal destroyed the admitted factory owner");};good(action.activate());check(old.expired()&&!action.snapshot(),"A returned factory consumer retained a retired activation");}
}
int main(int argc,char** argv){try{
    const std::string name=argc>1?argv[1]:"temporary";
    if(name=="temporary")temporary();else if(name=="argument")argument();else if(name=="adaptive")adaptive();else if(name=="selection")selection();else if(name=="inflight")inflight();else if(name=="preserve")preserve();else if(name=="failures")failures();else if(name=="fixed")fixed();else if(name=="payload")payload();else if(name=="indexed")indexed();else if(name=="conditional")conditional();else if(name=="retirement")retirement();else throw std::runtime_error("Unknown generated factory capture test");
    check(NativeFactoryCaptureBackend::live==0&&FactoryCaptureDomain::Node::live==0&&FactoryCaptureDomain::Argument::live==0,"Returned callable factories leaked native resources");std::cout<<"Generated factory capture "<<name<<" passed\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
