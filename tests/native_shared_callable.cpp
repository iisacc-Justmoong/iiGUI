#include "NativeSharedCallableRuntime.generated.h"
#include "NativeIndependentCallableRuntime.generated.h"
#include "NativeRootSharedCallableRuntime.generated.h"
#include "NativeReferenceCallableRuntime.generated.h"
#include "../src/Widgets/WidgetState.h"
#include <iostream>
#include <stdexcept>
using namespace ii;
namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void good(core::Result<void> result){if(!result){if(result.error().cause)std::rethrow_exception(result.error().cause);throw std::runtime_error(result.error().message);}}
template<class T>T take(core::Result<T> result){if(!result){if(result.error().cause)std::rethrow_exception(result.error().cause);throw std::runtime_error(result.error().message);}return std::move(*result);}
template<class View=lvui_generated::NativeSharedCallableRuntime>struct Fixture final {
    std::shared_ptr<gui::detail::WidgetState> state=std::make_shared<gui::detail::WidgetState>([](runtime::Task task){task();});
    gui::Widget host;std::shared_ptr<typename View::Scope> scope;
    Fixture(){NativeSharedCallableBackend::during={};NativeSharedCallableBackend::resources.clear();host=take(state->content_host({},"root",{640,420}));good(host.set_content([&](gui::WidgetBuilder& widgets){return View::build(widgets,{}, {},&scope);}));}
    ~Fixture(){NativeSharedCallableBackend::during={};state->close();}
    std::vector<gui::Widget> find(std::string id){std::vector<gui::Widget> found;for(const auto& node:take(state->snapshots()))if(node.id==id||node.id.ends_with("/"+id))found.push_back(take(state->find(node.id)));return found;}
    gui::Widget one(std::string id){auto found=find(id);check(found.size()==1,"Expected one shared-value widget");return found.front();}
};
void identity(){
    Fixture f;check(f.scope->backend_0.factories==1,"A component input created one handler per consumer");
    NativeSharedCallableBackend::during=[&]{good(f.one("again").activate());};good(f.one("first").activate());
    check(f.scope->backend_0.seen==std::vector<int>{3,2},"Forwarded/default consumers did not share one active mutable handler");
    const auto before=f.scope->backend_0.factories;f.scope->backend_0.amount.set(4);
    check(f.scope->backend_0.factories==before+1,"A dependency reevaluated a shared factory more than once");
    good(f.one("nested").activate());check(f.scope->backend_0.seen.back()==5,"A nested consumer retained an obsolete shared value");
}
void lifecycle(){
    Fixture f;auto resource=NativeSharedCallableBackend::resources.back();
    f.scope->backend_0.child_visible.set(false);check(f.find("nested").empty()&&!resource.expired(),"Removing one consumer retired the component's shared value");
    f.scope->backend_0.visible.set(false);check(resource.expired()&&f.find("first").empty(),"Removing the component retained its callable resource");
    const auto before=f.scope->backend_0.factories;f.scope->backend_0.visible.set(true);
    check(f.scope->backend_0.factories==before+1,"Reactivation did not create exactly one fresh callable value");
}
void rollback(){
    Fixture f;auto resource=NativeSharedCallableBackend::resources.back();auto original=f.one("first");
    auto failed=f.host.set_content([](gui::WidgetBuilder& widgets){return lvui_generated::NativeSharedCallableRuntime::build(widgets,{},[](auto& scope){scope.backend_0.amount.set(-1);});});
    check(!failed&&bool(original.snapshot())&&!resource.expired(),"An initial shared factory failure replaced or retired the previous view");
    f.scope->backend_0.amount.set(-1);
    check(!resource.expired(),"A failed factory discarded the last valid shared value");
    good(f.one("first").activate());check(f.scope->backend_0.seen.back()==2,"A failed replacement made every consumer unusable");
    f.scope->backend_0.amount.set(5);check(resource.expired(),"A successful replacement retained the old shared resource");
    good(f.one("again").activate());check(f.scope->backend_0.seen.back()==6,"A shared value did not recover after a factory error");
}
void self(){
    Fixture f;auto resource=NativeSharedCallableBackend::resources.back();
    NativeSharedCallableBackend::during=[&]{f.scope->backend_0.visible.set(false);check(!resource.expired(),"A self-removing component destroyed its active handler");};
    good(f.one("first").activate());check(resource.expired()&&f.scope->backend_0.seen.back()==2,"An active shared handler failed to retire after returning");
    f.scope->backend_0.visible.set(true);std::weak_ptr<lvui_generated::NativeSharedCallableRuntime::Scope> retired=f.scope;
    NativeSharedCallableBackend::during=[&]{f.scope.reset();f.state->close();check(!retired.expired(),"A shared callable lost its root receiver while active");};
    good(f.one("first").activate());check(retired.expired(),"A shared callable retained a retired root receiver");
}
void independent(){
    Fixture<lvui_generated::NativeIndependentCallableRuntime> f;
    const auto& resources=NativeSharedCallableBackend::resources;
    check(f.scope->backend_0.factories==2&&resources.size()==2&&resources[0].lock()!=resources[1].lock(),
          "Independent component activations shared a callable instance or duplicated its consumers");
}
void root(){
    Fixture<lvui_generated::NativeRootSharedCallableRuntime> f;
    check(f.scope->backend_0.factories==1,"A root alias chain duplicated its callback input");
    NativeSharedCallableBackend::during=[&]{good(f.one("again").activate());};good(f.one("first").activate());
    check(f.scope->backend_0.seen==std::vector<int>{3,2},"A window-root callback lost shared mutable state");
    auto resource=NativeSharedCallableBackend::resources.back();const auto before=f.scope->backend_0.factories;
    f.scope->backend_0.child_visible.set(false);check(f.find("first").empty()&&!resource.expired(),"Removing root content retired its window-owned value");
    f.scope->backend_0.child_visible.set(true);check(f.scope->backend_0.factories==before&&f.find("first").size()==1,"Restoring a root consumer recreated its owner's callback value");
}
void reference(){
    Fixture<lvui_generated::NativeReferenceCallableRuntime> f;
    check(f.scope->backend_0.factories==1,"A borrowed native callable was evaluated per consumer");
    good(f.one("first").activate());good(f.one("again").activate());
    check(f.scope->backend_0.seen==std::vector<int>{1,2}&&f.scope->backend_0.resident.calls==2,
          "The generated factory copied a nonmovable C++ callable reference or lost its state");
}
}
int main(int argc,char** argv){try{
    const std::string name=argc>1?argv[1]:"identity";
    if(name=="identity")identity();else if(name=="lifecycle")lifecycle();else if(name=="rollback")rollback();else if(name=="self")self();else if(name=="independent")independent();else if(name=="root")root();else if(name=="reference")reference();else throw std::runtime_error("Unknown shared callback widget test");
    check(NativeSharedCallableBackend::constructions==NativeSharedCallableBackend::destructions,"A shared callable retained a retired backend");
    std::cout<<"Shared callable widgets "<<name<<" passed without a window\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
