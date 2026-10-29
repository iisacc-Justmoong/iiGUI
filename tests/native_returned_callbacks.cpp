#include "NativeReturnedCallbacksRuntime.generated.h"
#include "../src/iiGUI/WidgetState.h"
#include <iostream>
#include <stdexcept>
using namespace ii;
namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void good(core::Result<void> result){if(!result){if(result.error().cause)std::rethrow_exception(result.error().cause);throw std::runtime_error(result.error().message);}}
template<class T>T take(core::Result<T> result){if(!result){if(result.error().cause)std::rethrow_exception(result.error().cause);throw std::runtime_error(result.error().message);}return std::move(*result);}
struct Fixture final {
    using View=lvui_generated::NativeReturnedCallbacksRuntime;
    std::shared_ptr<gui::detail::WidgetState> state=std::make_shared<gui::detail::WidgetState>([](runtime::Task task){task();});
    gui::Widget host;std::shared_ptr<View::Scope> scope;
    Fixture(){NativeReturnedBackend::during={};host=take(state->content_host({},"root",{640,420}));good(host.set_content([&](gui::WidgetBuilder& widgets){return View::build(widgets,{}, {},&scope);}));}
    ~Fixture(){NativeReturnedBackend::during={};state->close();}
    gui::Widget find(std::string id){for(const auto& node:take(state->snapshots()))if(node.id==id||node.id.ends_with("/"+id))return take(state->find(node.id));throw std::runtime_error("Missing returned-callback widget: "+id);}
};
void behavior(){
    Fixture f;auto direct=f.find("direct");const auto identity=take(direct.snapshot()).id;
    check(f.scope->backend_0.factories==3&&f.scope->backend_0.total==0,"Creating widgets invoked their returned callbacks or duplicated factories");
    f.scope->backend_0.amount.set(4);
    check(f.scope->backend_0.factories==6&&take(direct.snapshot()).id==identity,"A factory dependency did not update in place");
    good(direct.activate());good(f.find("supplied").activate());good(f.find("nested").activate());
    check(f.scope->backend_0.total==15,"A supplied or nested returned callback lost its original receiver or arguments");
    good(f.find("text").focus());good(f.state->input({.kind=gui::WidgetInputKind::text,.text="Native"}));
    check(f.scope->backend_0.text=="Native","A typed native event did not reach its returned callable");
}
void branch(){
    Fixture f;auto nested=f.find("nested");
    NativeReturnedBackend::during=[&]{f.scope->backend_0.visible.set(false);};good(nested.activate());
    check(f.scope->backend_0.total==3&&!nested.snapshot(),"An active returned callback was destroyed while removing its own branch");
    f.scope->backend_0.visible.set(true);good(f.find("nested").activate());
    check(f.scope->backend_0.total==6,"A reactivated returned callback reused a retired slot");
}
void lifetime(){
    Fixture f;auto direct=f.find("direct");std::weak_ptr<Fixture::View::Scope> retired=f.scope;
    NativeReturnedBackend::during=[&]{f.scope.reset();f.state->close();};good(direct.activate());
    check(retired.expired()&&!direct.snapshot(),"A returned callback retained its closed native activation");
}
}
int main(int argc,char** argv){try{
    const std::string name=argc>1?argv[1]:"behavior";
    if(name=="behavior")behavior();else if(name=="branch")branch();else if(name=="lifetime")lifetime();else throw std::runtime_error("Unknown returned callback widget test");
    std::cout<<"Native returned callback widgets "<<name<<" passed without a window\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
