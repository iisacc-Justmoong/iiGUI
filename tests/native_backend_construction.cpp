#include "NativeBackendConstructionRuntime.generated.h"
#include "../src/Widgets/WidgetState.h"
#include <iostream>
#include <stdexcept>
#include <atomic>
using namespace ii;
namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void good(core::Result<void> result){if(!result){if(result.error().cause)std::rethrow_exception(result.error().cause);throw std::runtime_error(result.error().message);}}
template<class T>T take(core::Result<T> result){if(!result){if(result.error().cause)std::rethrow_exception(result.error().cause);throw std::runtime_error(result.error().message);}return std::move(*result);}
struct Fixture final {
    using View=lvui_generated::NativeBackendConstructionRuntime;
    std::shared_ptr<gui::detail::WidgetState> state=std::make_shared<gui::detail::WidgetState>([](runtime::Task task){task();});
    std::shared_ptr<NativeConstructionResource> resource=std::make_shared<NativeConstructionResource>();
    lvui::native::BackendFactories factories;
    gui::Widget host;std::shared_ptr<View::Scope> scope;
    Fixture(){
        NativeConstructedBackend::constructions=NativeConstructedBackend::destructions=NativeConstructedBackend::bad_resource_order=0;
        NativeConstructionResource::destructions=0;NativeConstructedLater::fail=false;resource->value=7;
        factories=factories.with<NativeConstructedBackend>([resource=resource](const lvui::native::BackendRequest& request){
            return NativeConstructedBackend(resource,resource->value+(request.binding=="second"?2:0));
        });
        host=take(state->content_host({},"root",{640,420}));good(rebuild(factories));
    }
    ~Fixture(){NativeConstructedLater::fail=false;state->close();}
    core::Result<void> rebuild(const lvui::native::BackendFactories& current){return host.set_content([&](gui::WidgetBuilder& widgets){return View::build(widgets,{}, {},&scope,current);});}
    gui::Widget find(std::string id){for(const auto& node:take(state->snapshots()))if(node.id==id||node.id.ends_with("/"+id))return take(state->find(node.id));throw std::runtime_error("Missing construction widget: "+id);}
};
void configured(){
    Fixture f;check(NativeConstructedBackend::constructions==3&&take(f.find("total").snapshot()).text=="24", "Native factories failed in prefix storage, derived values or nested construction");
    auto before=std::addressof(f.scope->backend_0);good(f.find("increment").activate());
    check(std::addressof(f.scope->backend_0)==before&&NativeConstructedBackend::constructions==3&&f.scope->backend_0.value.get()==8&&f.scope->backend_1.value.get()==9,
          "A borrowed method reconstructed a backend or conflated independent imports");
    check(take(f.find("total").snapshot()).text=="26","Injected nonmovable backend values did not remain reactive");
    f.scope->backend_0.visible.set(false);check(NativeConstructedBackend::destructions==1,"Removing a child destroyed the wrong construction owner");
    f.scope->backend_0.visible.set(true);check(NativeConstructedBackend::constructions==4&&take(f.find("local").snapshot()).text=="7","Reactivation failed to inherit construction inputs");
}
void snapshots(){
    Fixture f;auto next=std::make_shared<NativeConstructionResource>();next->value=9;
    f.factories=f.factories.with<NativeConstructedBackend>([next]{return NativeConstructedBackend(next);});
    f.scope->backend_0.visible.set(false);f.scope->backend_0.visible.set(true);
    check(take(f.find("local").snapshot()).text=="7","A changed application factory replaced an existing activation's inputs");
    good(f.rebuild(f.factories));check(f.scope->backend_0.value.get()==9,"A new activation reused obsolete construction inputs");
}
void rollback(){
    Fixture f;auto previous=f.find("total");const auto identity=take(previous.snapshot()).id;
    check(!f.rebuild({})&&take(f.find("total").snapshot()).id==identity,"Missing constructor inputs replaced the working view");
    const auto before=NativeConstructedBackend::constructions,retired=NativeConstructedBackend::destructions;
    NativeConstructedLater::fail=true;check(!f.rebuild(f.factories),"A failing later constructor was silently accepted");NativeConstructedLater::fail=false;
    check(NativeConstructedBackend::constructions==before+2&&NativeConstructedBackend::destructions==retired+2&&previous.snapshot(),
          "Partial construction leaked an earlier nonmovable backend or discarded the working view");
    good(f.rebuild(f.factories));check(!previous.snapshot(),"A valid construction did not recover after rollback");
}
void lifetime(){
    Fixture f;std::weak_ptr<NativeConstructionResource> retired=f.resource;
    f.resource.reset();f.factories={};check(!retired.expired(),"Constructor inputs did not stay owned by their native activation");
    f.state->close();f.scope.reset();
    check(retired.expired()&&NativeConstructedBackend::constructions==NativeConstructedBackend::destructions&&NativeConstructedBackend::bad_resource_order==0,
          "Factory resources were destroyed before their backends or retained after final retirement");
}
void runtime_injection(){
    ii::runtime::Runtime workers({.workers=2,.capacity=8});Fixture f;
    auto factories=f.factories.with<NativeConstructedBackend>([&workers,resource=f.resource]{return NativeConstructedBackend(workers,resource);});
    good(f.rebuild(factories));
    check(f.scope->backend_0.runtime==&workers&&f.scope->backend_1.runtime==&workers&&f.scope->backend_0.value.get()==2,
          "Native construction did not inject the application's existing Core Runtime");
    std::atomic_int result=0;const auto ui=std::this_thread::get_id();
    auto submitted=f.scope->backend_0.submit([&]{result=std::this_thread::get_id()!=ui?42:-1;});
    check(bool(submitted)&&bool(workers.wait_idle())&&result==42,"An injected backend failed to use Core's persistent native workers");
}
}
int main(int argc,char** argv){try{
    const std::string name=argc>1?argv[1]:"configured";
    if(name=="configured")configured();else if(name=="snapshots")snapshots();else if(name=="rollback")rollback();else if(name=="lifetime")lifetime();else if(name=="runtime")runtime_injection();else throw std::runtime_error("Unknown backend construction widget test");
    std::cout<<"Native backend construction widgets "<<name<<" passed without a window\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
