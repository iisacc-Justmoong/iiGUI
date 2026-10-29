#include "NativeReadonlyRuntime.generated.h"
#include "NativeReadonlyPostfixRuntime.generated.h"
#include "../src/iiGUI/WidgetState.h"
#include <iostream>
#include <stdexcept>
using namespace ii;
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
template<class T>T take(core::Result<T> value){if(!value){if(value.error().cause)std::rethrow_exception(value.error().cause);throw std::runtime_error(value.error().message);}return std::move(*value);}
void good(core::Result<void> value){if(!value){if(value.error().cause)std::rethrow_exception(value.error().cause);throw std::runtime_error(value.error().message);}}
template<class View>struct Mounted final {
    std::shared_ptr<gui::detail::WidgetState> state=std::make_shared<gui::detail::WidgetState>([](runtime::Task task){task();});
    gui::Widget host=take(state->content_host({},"root",{640,480}));
    std::shared_ptr<typename View::Scope> scope;
    Mounted(){good(host.set_content([&](gui::WidgetBuilder& builder){return View::build(builder,{}, {},&scope);}));}
    ~Mounted(){state->close();scope.reset();}
    auto& model(){return scope->backend_0;}
    gui::Widget widget(std::string_view suffix){for(const auto& item:take(state->snapshots()))if(item.id==suffix||item.id.ends_with("/"+std::string(suffix)))return take(state->find(item.id));throw std::runtime_error("Readonly widget not found");}
    std::string text(std::string_view suffix){return take(widget(suffix).snapshot()).text;}
    bool has(std::string_view suffix){for(const auto& item:take(state->snapshots()))if(item.id==suffix||item.id.ends_with("/"+std::string(suffix)))return true;return false;}
};
using View=lvui_generated::NativeReadonlyRuntime;
void fields(){
    Mounted<View> m;check(m.text("title")=="First"&&m.text("count")=="2"&&m.text("local")=="2","Readonly initial member paths were wrong");
    m.model().first->title.set("Changed");m.model().first->metrics.count.set(7);
    check(m.text("title")=="Changed"&&m.text("count")=="7"&&m.text("local")=="7"&&m.text("initial")=="First","Readonly members did not update through aliases and lexical scopes or changed Static scalar state");
    check(m.model().file_revision.get()==0&&m.scope->revision_0.get()==0,"Readonly view updates changed file or fallback revision");
}
void selection(){
    Mounted<View> m;auto old=m.model().first;m.model().selectSecond();m.model().second->metrics.count.set(11);
    check(m.text("title")=="Second"&&m.text("count")=="11"&&m.text("local")=="11","Readonly replacement lost child bindings");
    old->title.set("Old");old->metrics.count.set(99);
    check(m.text("title")=="Second"&&m.text("count")=="11","A retired selection still drove the view");
}
void references(){
    Mounted<View> m;m.model().editStable(8);
    check(m.text("borrowed")=="8"&&m.text("pointer")=="8"&&m.text("count")=="2","Readonly standard references or pointers lost publication or affected another object");
}
void records(){
    Mounted<View> m;m.model().record={"Published",9};m.scope->publish_native_changes();
    check(m.text("record")=="Published"&&m.text("draft")=="Record","A noncomparable record did not publish or an owned var followed its initializer");
    check(m.model().file_revision.get()==0,"Record publication changed document state");
}
void mutations(){
    Mounted<View> m;auto field=m.widget("edit");good(field.set_text("Working"));good(field.focus());const auto nodes=take(m.state->snapshots()).size();
    good(m.scope->inferred_draft->set({"Local",7}));good(m.scope->inferred_draft->set({"Local",7}));
    const auto input=take(field.snapshot());
    check(m.text("draft")=="Local"&&m.text("record")=="Record"&&m.scope->inferred_draft->revision()==2,
          "An inferred noncomparable native var lost its typed mutation contract");
    check(input.text=="Working"&&input.focused&&take(m.state->snapshots()).size()==nodes&&m.model().file_revision.get()==0,"Native record mutation rebuilt the view or changed file state");
}
void nulls(){
    Mounted<View> m;m.model().selected.set({});
    check(m.text("title")=="First"&&m.text("count")=="2"&&!take(m.scope->native_lifetime.lock()->failures()).empty(),"A null native selection erased valid content or hid its failure");
    m.model().selected.set(m.model().second);m.model().second->metrics.count.set(12);
    check(m.text("title")=="Second"&&m.text("count")=="12"&&take(m.scope->native_lifetime.lock()->failures()).empty(),"Readonly paths did not recover after null selection");
}
void retirement(){
    Mounted<View> m;std::weak_ptr<const NativeReadonlyItem> old=m.model().first;m.model().first.reset();m.model().selectSecond();
    check(old.expired()&&m.text("title")=="Second","Readonly observations retained an obsolete selected object");
    auto owner=m.scope;good(m.host.clear_content());check(!owner->active(),"Readonly scope stayed active after view retirement");
}
void conditional(){
    Mounted<View> m;check(m.has("conditional"),"Readonly conditional content was missing");m.model().visible.set(false);
    check(!m.has("conditional"),"Readonly conditional view did not retire");m.model().selectSecond();m.model().visible.set(true);
    check(m.has("conditional")&&m.text("conditional")=="Second","Readonly reactivation used the retired object's data");
}
void postfix(){
    Mounted<lvui_generated::NativeReadonlyPostfixRuntime> m;m.model().first->metrics.count.set(13);m.model().editStable(17);
    check(m.text("count")=="13"&&m.text("borrowed")=="17","Parenthesized C++ member AST paths bypassed native observation");
    m.model().selectSecond();m.model().second->title.set("Next");check(m.text("title")=="Next","Parenthesized pointer paths did not follow selection changes");
}
int main(int argc,char** argv){try{std::string name=argc>1?argv[1]:"fields";
    if(name=="fields")fields();else if(name=="selection")selection();else if(name=="references")references();else if(name=="records")records();else if(name=="mutations")mutations();else if(name=="nulls")nulls();else if(name=="retirement")retirement();else if(name=="conditional")conditional();else if(name=="postfix")postfix();else throw std::runtime_error("Unknown readonly test");
    check(NativeReadonlyModel::live==0&&NativeReadonlyItem::live==0,"Readonly native owners leaked");std::cout<<"Readonly native values "<<name<<" passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
