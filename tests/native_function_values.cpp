#include "NativeFunctionValuesRuntime.generated.h"
#include "../src/Widgets/WidgetState.h"
#include <iostream>
#include <stdexcept>
using namespace ii;
namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void good(core::Result<void> result){if(!result){if(result.error().cause)std::rethrow_exception(result.error().cause);throw std::runtime_error(result.error().message);}}
template<class T>T take(core::Result<T> result){if(!result){if(result.error().cause)std::rethrow_exception(result.error().cause);throw std::runtime_error(result.error().message);}return std::move(*result);}
struct Fixture final {
    using View=lvui_generated::NativeFunctionValuesRuntime;
    std::shared_ptr<gui::detail::WidgetState> state=std::make_shared<gui::detail::WidgetState>([](runtime::Task task){task();});
    gui::Widget host;std::shared_ptr<View::Scope> scope;
    Fixture(){NativeFunctionBackend::during={};host=take(state->content_host({},"root",{640,420}));good(host.set_content([&](gui::WidgetBuilder& widgets){return View::build(widgets,{}, {},&scope);}));}
    ~Fixture(){NativeFunctionBackend::during={};state->close();}
    std::vector<gui::Widget> find(std::string id){std::vector<gui::Widget> found;for(const auto& node:take(state->snapshots()))if(node.id==id||node.id.ends_with("/"+id))found.push_back(take(state->find(node.id)));return found;}
    gui::Widget one(std::string id){auto found=find(id);check(found.size()==1,"Expected one function-result widget");return found.front();}
    std::string text(std::string id){return take(one(id).snapshot()).text;}
    // These slots belong to this generated fixture, not the public ABI. Direct
    // method declarations now also own a value before dynamic/frozen aliases.
    auto dynamic_value()const{return scope->callable_value_1;}
    auto frozen_alias_value()const{return scope->callable_value_3;}
    void formatted(std::string value){auto items=find("formatted");check(items.size()==2,"A function consumer was lost");for(const auto& item:items)check(take(item.snapshot()).text==value,"A consumer retained a stale function result");}
};
void behavior(){
    Fixture f;f.formatted("3");check(f.text("direct")=="D2"&&f.text("frozen")=="3"&&f.scope->backend_0.factories==2,"Initial native function values were wrong");
    const auto identity=take(f.find("formatted").front().snapshot()).id;
    f.scope->backend_0.count.set(3);f.formatted("4");check(f.text("direct")=="D3"&&f.text("frozen")=="4","A method reference or Static function lost its input dependency");
    f.scope->backend_0.amount.set(5);f.formatted("8");check(f.text("frozen")=="4"&&f.scope->backend_0.factories==3&&take(f.find("formatted").front().snapshot()).id==identity,
          "A function version update recreated widgets or reevaluated a Static factory");
    check(f.scope->backend_0.file_revision.get()==0&&f.scope->revision_0.get()==0,"Read-only function evaluation mutated file or backend revision");
}
void errors(){
    Fixture f;f.scope->backend_0.amount.set(-1);f.formatted("3");
    auto graph=f.scope->native_lifetime.lock();check(graph&&!graph->failures()->empty(),"A failed function factory hid its error");
    f.scope->backend_0.amount.set(6);f.formatted("8");check(graph->failures()->empty(),"Function factory recovery retained its error");
    f.scope->backend_0.count.set(-1);f.formatted("8");check(!graph->failures()->empty(),"A failing function invocation discarded its valid display result or hid its error");
    f.scope->backend_0.count.set(2);f.formatted("8");check(graph->failures()->empty()&&f.scope->backend_0.file_revision.get()==0,"Function invocation failed to recover without file mutation");
}
void lifetime(){
    Fixture f;auto value=f.dynamic_value();std::weak_ptr<Fixture::View::Scope> retired=f.scope;f.state->close();f.scope.reset();
    bool rejected=false;try{(void)(*value)(0);}catch(const std::logic_error&){rejected=true;}
    check(rejected&&retired.expired(),"A saved function returned a fabricated value or retained a closed backend");
}
void self(){
    Fixture f;auto value=f.dynamic_value();std::weak_ptr<Fixture::View::Scope> retired=f.scope;
    NativeFunctionBackend::during=[&]{f.state->close();f.scope.reset();check(!retired.expired(),"A native function lost its receiver before producing a result");};
    check((*value)(2)=="3"&&retired.expired(),"A native function removing its view lost its result or retained its receiver");
}
void snapshots(){
    Fixture f;check(f.text("frozenAlias")=="3"&&f.text("pinned")=="3","Initial function snapshots were wrong");
    f.scope->backend_0.amount.set(5);f.formatted("7");
    check(f.text("frozenAlias")=="3"&&f.text("pinned")=="3"&&f.scope->backend_0.factories==3,"A Static alias followed the dynamic function version or reran its factory");
    f.scope->backend_0.count.set(4);
    check(f.text("frozenAlias")=="5"&&f.text("pinned")=="5","A frozen function lost its current invocation arguments");
    f.scope->backend_0.visible.set(false);check(f.find("pinned").empty(),"A nested snapshot consumer survived unmount");
    f.scope->backend_0.visible.set(true);
    check(f.text("pinned")=="9"&&f.text("frozenAlias")=="5","A reactivated snapshot did not select its new function version independently");
    check(f.scope->backend_0.file_revision.get()==0&&f.scope->revision_0.get()==0,"Snapshot evaluation published file or synthetic backend revisions");
}
void snapshot_self(){
    Fixture f;auto value=f.frozen_alias_value();std::weak_ptr<Fixture::View::Scope> retired=f.scope;
    NativeFunctionBackend::during=[&]{f.state->close();f.scope.reset();check(!retired.expired(),"A snapshot lost its source receiver before returning");};
    check((*value)(2)=="3"&&retired.expired(),"A snapshot removing its view lost its result or retained its receiver");
    bool rejected=false;try{(void)(*value)(0);}catch(const std::logic_error&){rejected=true;}
    check(rejected,"A saved snapshot admitted a call after view retirement");
}
}
int main(int argc,char** argv){try{const std::string name=argc>1?argv[1]:"behavior";
    if(name=="behavior")behavior();else if(name=="errors")errors();else if(name=="lifetime")lifetime();else if(name=="self")self();else if(name=="snapshots")snapshots();else if(name=="snapshot_self")snapshot_self();else throw std::runtime_error("Unknown native function widget test");
    std::cout<<"Native function result widgets "<<name<<" passed without a window\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
