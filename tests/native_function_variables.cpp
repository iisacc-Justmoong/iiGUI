#include "NativeFunctionVariablesRuntime.generated.h"
#include "../src/iiGUI/WidgetState.h"
#include <iostream>
#include <stdexcept>
using namespace ii;
namespace {
using View=lvui_generated::NativeFunctionVariablesRuntime;
using Value=lvui::native::CallbackValue<std::string(std::int64_t)>;
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void good(core::Result<void> result){if(!result){if(result.error().cause)std::rethrow_exception(result.error().cause);throw std::runtime_error(result.error().message);}}
template<class T>T take(core::Result<T> result){if(!result){if(result.error().cause)std::rethrow_exception(result.error().cause);throw std::runtime_error(result.error().message);}return std::move(*result);}
struct Fixture final {
    std::shared_ptr<gui::detail::WidgetState> state=std::make_shared<gui::detail::WidgetState>([](runtime::Task task){task();});
    gui::Widget host;std::shared_ptr<View::Scope> scope;
    Fixture(){NativeFunctionVariableBackend::during={};host=take(state->content_host({},"root",{640,420}));good(host.set_content([&](gui::WidgetBuilder& widgets){return View::build(widgets,{}, {},&scope);}));}
    ~Fixture(){NativeFunctionVariableBackend::during={};state->close();}
    std::vector<gui::Widget> find(std::string id){std::vector<gui::Widget> found;for(const auto& node:take(state->snapshots()))if(node.id==id||node.id.ends_with("/"+id))found.push_back(take(state->find(node.id)));return found;}
    gui::Widget one(std::string id){auto found=find(id);check(found.size()==1,"Expected one mutable function widget");return found.front();}
    std::string text(std::string id){return take(one(id).snapshot()).text;}
    void formatted(std::string text){auto items=find("formatted");check(items.size()==2,"A mutable function consumer was lost");for(const auto& item:items)check(take(item.snapshot()).text==text,"A mutable function consumer used a stale version");}
};
void behavior(){
    Fixture f;f.formatted("3");check(f.text("native")=="3"&&f.text("direct")=="D2"&&f.text("pinned")=="3"&&f.text("local")=="3"&&f.scope->backend_0.factories==2,"Initial function variables were wrong");
    f.scope->backend_0.amount.set(5);f.formatted("3");
    check(f.text("native")=="3"&&f.scope->backend_0.factories==3,"An initializer overwrote a variable or its native factory reran");
    good(f.scope->function_var_format->set(f.scope->callable_value_0));f.formatted("7");
    check(f.text("pinned")=="3"&&f.text("local")=="3","Assignment changed a Static snapshot or independent local variable");
    f.scope->backend_0.count.set(3);f.formatted("8");check(f.text("pinned")=="4"&&f.text("native")=="4"&&f.text("direct")=="D3","Variables lost their current invocation arguments");
    check(f.scope->backend_0.file_revision.get()==0&&f.scope->revision_0.get()==0,"A function read or assignment published a file or backend revision");
}
void replacements(){
    Fixture f;const auto identity=take(f.find("formatted").front().snapshot()).id;std::weak_ptr<int> old;
    good(f.scope->function_var_format->replace_native([&]{auto token=std::make_shared<int>(10);old=token;return [token](std::int64_t n){return std::to_string(*token+n);};}));f.formatted("12");
    good(f.scope->function_var_format->replace_native([]{return [state=std::make_unique<int>(20)](std::int64_t n){return std::to_string(*state+n);};}));f.formatted("22");
    check(old.expired()&&take(f.find("formatted").front().snapshot()).id==identity,"Replacement retained obsolete resources or rebuilt consumers");
    auto value=std::make_shared<Value>([](std::int64_t n){return "V"+std::to_string(n);});good(f.scope->function_var_direct->set(value));
    check(f.text("direct")=="V2","Typed assignment failed to update a direct-method variable");
}
void events(){
    Fixture f;good(f.one("action").activate());check(f.text("events")=="1"&&f.scope->backend_0.events==1&&f.scope->revision_0.get()==1,"A mutable native event lost its original revision publication");
    auto* receiver=&f.scope->backend_0;
    good(f.scope->function_var_action->replace_native([receiver]{return [receiver]{receiver->count.set(7);};}));good(f.one("action").activate());
    f.formatted("8");check(f.scope->backend_0.events==1&&f.scope->backend_0.file_revision.get()==0,"An event used its replaced handler or mutated file state");
}
void errors(){
    Fixture f;auto failed=f.scope->function_var_format->replace_native([]()->Value::Action{throw std::runtime_error("Native factory failed");});
    check(!failed&&failed.error().cause,"A native replacement failure hid its cause");f.formatted("3");
    auto graph=f.scope->native_lifetime.lock();
    good(f.scope->function_var_format->replace_native([]{return [](std::int64_t)->std::string{throw std::runtime_error("Display computation failed");};}));
    f.formatted("3");check(!graph->failures()->empty(),"A failed replacement invocation discarded its valid display or hid its error");
    good(f.scope->function_var_format->replace_native([]{return [](std::int64_t n){return std::to_string(n+9);};}));f.formatted("11");
    check(graph->failures()->empty(),"Replacement consumers did not recover");
    failed=f.scope->function_var_format->set({});check(!failed&&failed.error().code==core::ErrorCode::invalid_argument,"An empty replacement was accepted");f.formatted("11");
}
void lifetime(){
    Fixture f;auto variable=f.scope->function_var_format;auto value=variable->value();std::weak_ptr<View::Scope> retired=f.scope;
    f.state->close();f.scope.reset();bool rejected=false;try{(void)(*value)(0);}catch(const std::logic_error&){rejected=true;}
    int factories=0;auto result=variable->replace_native([&]{++factories;return [](std::int64_t){return std::string{};};});
    check(rejected&&retired.expired()&&!result&&result.error().code==core::ErrorCode::closed&&factories==0,"Saved handles retained the closed scope or admitted a replacement");
}
void active(){
    Fixture f;auto variable=f.scope->function_var_format;auto value=variable->value();auto* receiver=f.scope.get();
    good(variable->replace_native([receiver]{return [receiver](std::int64_t n){if(auto fn=std::exchange(NativeFunctionVariableBackend::during,{}))fn();return std::to_string(receiver->backend_0.count.get()+n);};}));
    std::weak_ptr<View::Scope> retired=f.scope;
    NativeFunctionVariableBackend::during=[&]{f.state->close();f.scope.reset();variable.reset();check(!retired.expired(),"A replacing function lost its admitted receiver");};
    check((*value)(2)=="4"&&retired.expired(),"A function variable removing its view lost the result or retained the scope");
}
void conditional(){
    Fixture f;std::weak_ptr<int> version;
    good(f.scope->function_var_format->replace_native([&]{auto token=std::make_shared<int>(10);version=token;return [token](std::int64_t n){return std::to_string(*token+n);};}));
    f.scope->backend_0.visible.set(false);check(f.find("local").empty(),"A local variable survived its panel");f.scope->backend_0.visible.set(true);
    check(f.text("local")=="12","A remounted variable failed to select the current parent function");
    good(f.scope->function_var_format->replace_native([]{return [](std::int64_t n){return std::to_string(n+20);};}));f.formatted("22");
    check(f.text("local")=="12"&&!version.expired(),"A local variable did not independently own its selected version");
    f.scope->backend_0.visible.set(false);check(version.expired(),"A retired local variable kept its selected capture");f.scope->backend_0.visible.set(true);
    check(f.text("local")=="22","A second panel activation reused stale local function state");
}
core::Result<void> typed_setter(View& view,const std::shared_ptr<Value>& value){return view.set_format(value);}
void setter_gate(){
    View view;auto result=typed_setter(view,std::make_shared<Value>([](std::int64_t){return std::string{};}));
    check(!result&&result.error().code==core::ErrorCode::closed,"An unattached view admitted a typed function write");
    int factories=0;result=view.set_format_native([&]{++factories;return [](std::int64_t){return std::string{};};});
    check(!result&&result.error().code==core::ErrorCode::closed&&factories==0,"An unattached view evaluated a native replacement factory");
}
}
int main(int argc,char** argv){try{const std::string name=argc>1?argv[1]:"behavior";
    if(name=="behavior")behavior();else if(name=="replacements")replacements();else if(name=="events")events();else if(name=="errors")errors();else if(name=="lifetime")lifetime();else if(name=="active")active();else if(name=="conditional")conditional();else if(name=="setter_gate")setter_gate();else throw std::runtime_error("Unknown mutable function widget test");
    std::cout<<"Native function variable widgets "<<name<<" passed without a window\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
