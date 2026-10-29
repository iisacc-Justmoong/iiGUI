#include "NativeInferredValuesRuntime.generated.h"
#include "NativeInferredFieldsRuntime.generated.h"
#include "../src/Widgets/WidgetState.h"
#include <iostream>
using namespace ii;
namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(core::Result<T> result){if(!result){if(result.error().cause)std::rethrow_exception(result.error().cause);throw std::runtime_error(result.error().message);}return std::move(*result);}
void good(core::Result<void> result){if(!result){if(result.error().cause)std::rethrow_exception(result.error().cause);throw std::runtime_error(result.error().message);}}
using Main=lvui_generated::NativeInferredValuesRuntime;
using Fields=lvui_generated::NativeInferredFieldsRuntime;
template<class View>struct Mounted final {
    std::shared_ptr<gui::detail::WidgetState> state=std::make_shared<gui::detail::WidgetState>([](runtime::Task task){task();});
    gui::Widget host=take(state->content_host({},"root",{640,480}));std::shared_ptr<typename View::Scope> scope;
    Mounted(){good(host.set_content([&](gui::WidgetBuilder& widgets){return View::build(widgets,{}, {},&scope);}));}
    ~Mounted(){state->close();scope.reset();}
    auto& model(){return scope->backend_0;}
    gui::Widget widget(std::string_view id){std::string found;for(const auto& node:take(state->snapshots()))if(node.id==id||node.id.ends_with("/"+std::string(id))){check(found.empty(),"An inferred widget id was ambiguous");found=node.id;}check(!found.empty(),"Inferred widget not found");return take(state->find(found));}
    void click(std::string_view id){good(widget(id).activate());}
    std::string text(std::string_view id){return take(widget(id).snapshot()).text;}
};
void selection(){Mounted<Main> m;check(m.model().first->count.get()==0,"Selecting an inferred method executed it");m.model().selected.set(m.model().second);m.click("method");m.click("action");m.click("typed");check(m.model().second->count.get()==3&&m.text("description")=="Second 3"&&m.text("forwarded")=="Second 3","Inferred calls or typed forwarding used an obsolete receiver");}
void temporary(){Mounted<Main> m;check(FactoryCaptureDomain::Node::live==3,"An inferred factory lost its temporary receiver");m.click("temporary");good(m.host.clear_content());check(FactoryCaptureDomain::Node::live==2,"An inferred temporary survived graph retirement");}
void argument(){Mounted<Main> m;check(FactoryCaptureDomain::Argument::live==2,"An inferred factory lost borrowed temporary arguments");m.click("argument");check(m.model().last=="Argument","Borrowed argument was damaged");m.click("mixed");check(m.model().last=="Argument/Argument","Mixed borrowed and immovable arguments were damaged");}
void frozen(){Mounted<Main> m;m.model().selected.set(m.model().second);m.click("pinned");m.click("manual");check(m.model().first->count.get()==2&&m.model().second->count.get()==0,"Inferred Static or var selection followed its source");good(m.scope->inferred_manual->set(m.scope->inferred_action->get()));m.click("manual");check(m.model().second->count.get()==1,"Inferred function assignment failed");}
void fields(){Mounted<Fields> m;m.click("run");m.click("bump");m.click("factory");check(m.text("count")=="3"&&m.model().file_revision.get()==0,"Inferred Void operations did not publish plain fields or changed document state");m.model().format.set([](std::int64_t n){return "Second "+std::to_string(n);});check(m.text("format")=="Second 2"&&m.text("alias")=="Second 2"&&m.text("frozen")=="First 2"&&m.text("manual")=="First 2","Inferred field versions, aliases or fixed selections were confused");}
void variable(){Mounted<Fields> m;good(m.scope->inferred_number->set(7));check(m.text("count")=="7","An inferred scalar var failed to publish");good(m.scope->inferred_manual->replace_native([]{return [](std::int64_t n){return "Native "+std::to_string(n);};}));check(m.text("manual")=="Native 2","Inferred native function replacement failed");}
void preserve(){Mounted<Main> m;auto edit=m.widget("edit");good(edit.set_text("Working"));good(edit.focus());const auto before=take(m.state->snapshots()).size();auto id=take(m.widget("action").snapshot()).id;m.model().selected.set(m.model().second);m.click("action");auto after=take(edit.snapshot());check(after.text=="Working"&&after.focused&&before==take(m.state->snapshots()).size()&&id==take(m.widget("action").snapshot()).id&&m.model().file_revision.get()==0,"Inferred selection rebuilt widgets or changed document state");}
void failures(){Mounted<Main> m;m.model().selected.set({});m.click("action");check(m.model().first->count.get()==1&&!take(m.scope->native_lifetime.lock()->failures()).empty(),"Failed inference discarded its valid function");m.model().selected.set(m.model().second);m.click("action");check(m.model().second->count.get()==1&&take(m.scope->native_lifetime.lock()->failures()).empty(),"Inferred functions failed to recover");}
void reentry(){Mounted<Main> m;auto previous=std::make_shared<FactoryCaptureDomain::Node>("Independent");m.model().selected.set(previous);std::weak_ptr<FactoryCaptureDomain::Node> old=previous;auto next=std::make_shared<FactoryCaptureDomain::Node>("Next");previous->during=[&]{m.model().selected.set(next);check(!old.expired(),"Inferred replacement destroyed an active receiver");m.click("typed");};previous.reset();m.click("action");check(old.expired()&&next->count.get()==1,"Inferred reentry leaked or reused a retired receiver");}
void conditional(){Mounted<Main> m;auto previous=m.widget("conditional");m.model().visible.set(false);check(!previous.snapshot(),"An inferred conditional consumer did not retire");m.model().selected.set(m.model().second);m.model().visible.set(true);m.click("conditional");check(m.model().second->count.get()==1&&m.model().first->count.get()==0,"A new inferred consumer reused the old receiver or retired its source");}
void retirement(){Mounted<Main> m;auto action=m.widget("action");std::weak_ptr<Main::Scope> old=m.scope;m.model().first->during=[&]{m.scope.reset();m.state->close();check(!old.expired(),"Inferred self-removal destroyed the admitted owner");};good(action.activate());check(old.expired()&&!action.snapshot(),"Inferred functions retained a retired scope");}
}
int main(int argc,char** argv){try{const std::string name=argc>1?argv[1]:"selection";
    if(name=="selection")selection();else if(name=="temporary")temporary();else if(name=="argument")argument();else if(name=="frozen")frozen();else if(name=="fields")fields();else if(name=="variable")variable();else if(name=="preserve")preserve();else if(name=="failures")failures();else if(name=="reentry")reentry();else if(name=="conditional")conditional();else if(name=="retirement")retirement();else throw std::runtime_error("Unknown inferred value test");
    check(NativeFactoryCaptureBackend::live==0&&FactoryCaptureDomain::Node::live==0&&FactoryCaptureDomain::Argument::live==0,"Inferred values leaked their native captures");std::cout<<"Generated inferred values "<<name<<" passed\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
