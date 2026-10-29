#include "NativeCallableInputsRuntime.generated.h"
#include "../src/iiGUI/WidgetState.h"
#include <iostream>
#include <stdexcept>
using namespace ii;
namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void good(core::Result<void> result){if(!result){if(result.error().cause)std::rethrow_exception(result.error().cause);throw std::runtime_error(result.error().message);}}
template<class T>T take(core::Result<T> result){if(!result){if(result.error().cause)std::rethrow_exception(result.error().cause);throw std::runtime_error(result.error().message);}return std::move(*result);}
struct Fixture final {
    using View=lvui_generated::NativeCallableInputsRuntime;
    std::shared_ptr<gui::detail::WidgetState> state=std::make_shared<gui::detail::WidgetState>([](runtime::Task task){task();});
    gui::Widget host;std::shared_ptr<View::Scope> scope;
    Fixture(){NativeCallableBackend::during={};NativeCallableBackend::last_text_receiver=nullptr;host=take(state->content_host({},"root",{640,420}));good(host.set_content([&](gui::WidgetBuilder& widgets){return View::build(widgets,{}, {},&scope);}));}
    ~Fixture(){NativeCallableBackend::during={};state->close();}
    std::vector<gui::Widget> find(std::string id){std::vector<gui::Widget> found;for(const auto& node:take(state->snapshots()))if(node.id==id||node.id.ends_with("/"+id))found.push_back(take(state->find(node.id)));return found;}
    gui::Widget one(std::string id){auto found=find(id);check(found.size()==1,"Expected exactly one typed-callback widget");return found.front();}
    void type(gui::Widget widget,std::string value){good(widget.focus());good(state->input({.kind=gui::WidgetInputKind::text,.text=std::move(value)}));}
};
void direct(){
    Fixture f;const auto made=NativeCallableBackend::constructions;
    check(f.scope->backend_0.calls==0,"Mounting invoked a declared callback");
    good(f.one("action").activate());good(f.one("again").activate());
    check(f.scope->backend_0.calls==2&&NativeCallableBackend::constructions==made,
          "A forwarded/default callback lost its original receiver or created another backend");
}
void text(){
    Fixture f;const auto fields=f.find("edit");check(fields.size()==2,"Typed callback content was lost");
    f.type(fields[0],"Caller");check(f.scope->backend_0.text=="Caller","A definition-site default captured a shadowing backend");
    f.type(fields[1],"Local");auto* receiver=NativeCallableBackend::last_text_receiver;
    check(receiver&&receiver!=&f.scope->backend_0&&receiver->text=="Local"&&f.scope->backend_0.text=="Caller",
          "A returned String callback lost its local receiver or native payload");
}
void factory(){
    Fixture f;auto action=f.one("factory");const auto id=take(action.snapshot()).id;
    check(f.scope->backend_0.factories==1&&f.scope->backend_0.calls==0,"A typed factory was duplicated or invoked while mounting");
    f.scope->backend_0.amount.set(4);
    check(f.scope->backend_0.factories==2&&take(action.snapshot()).id==id,"A typed callback did not refresh in place");
    good(action.activate());check(f.scope->backend_0.calls==4,"A declared returned callback used stale input");
}
void retirement(){
    Fixture f;auto action=f.one("factory");const auto before=NativeCallableBackend::destructions;
    NativeCallableBackend::during=[&]{f.scope->backend_0.visible.set(false);check(!NativeCallableBackend::latest_resource.expired(),"An active typed callback lost its capture");};
    good(action.activate());
    check(f.scope->backend_0.calls==1&&!action.snapshot()&&NativeCallableBackend::latest_resource.expired()&&NativeCallableBackend::destructions==before+1,
          "A typed callback removing its branch did not retire its local backend and resource");
    f.scope->backend_0.visible.set(true);good(f.one("factory").activate());
    check(f.scope->backend_0.calls==2,"Reactivation reused a retired typed callback");
}
void root(){
    Fixture f;auto action=f.one("action");std::weak_ptr<Fixture::View::Scope> retired=f.scope;
    NativeCallableBackend::during=[&]{f.scope.reset();f.state->close();check(!retired.expired(),"A typed method destroyed its receiver before returning");};
    good(action.activate());check(retired.expired()&&!action.snapshot(),"A saved typed event retained a retired root");
}
}
int main(int argc,char** argv){try{
    const std::string name=argc>1?argv[1]:"direct";
    if(name=="direct")direct();else if(name=="text")text();else if(name=="factory")factory();else if(name=="retirement")retirement();else if(name=="root")root();else throw std::runtime_error("Unknown typed callback widget test");
    check(NativeCallableBackend::constructions==NativeCallableBackend::destructions,"Typed callbacks retained a closed backend");
    std::cout<<"Typed native callback widgets "<<name<<" passed without a window\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
