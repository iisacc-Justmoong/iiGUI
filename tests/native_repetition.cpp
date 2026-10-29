#include "NativeRepetitionRuntime.generated.h"
#include "NativeNestedRepetitionRuntime.generated.h"
#include "NativeListValuesRuntime.generated.h"
#include "NativeSharedRepetitionRuntime.generated.h"
#include "../src/iiGUI/WidgetState.h"
#include <iostream>
#include <stdexcept>
using namespace ii;
namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void good(core::Result<void> value){if(!value){if(value.error().cause)std::rethrow_exception(value.error().cause);throw std::runtime_error(value.error().message);}}
template<class T>T take(core::Result<T> value){if(!value){if(value.error().cause)std::rethrow_exception(value.error().cause);throw std::runtime_error(value.error().message);}return std::move(*value);}
struct Fixture final {
    using View=lvui_generated::NativeRepetitionRuntime;
    std::shared_ptr<gui::detail::WidgetState> state=std::make_shared<gui::detail::WidgetState>([](runtime::Task task){task();});
    gui::Widget host,list;
    std::shared_ptr<View::Scope> scope;
    Fixture(){
        NativeRowActivation::constructions=NativeRowActivation::destructions=0;
        host=take(state->content_host({},"root",{640,480}));
        good(host.set_content([&](gui::WidgetBuilder& builder){return View::build(builder,{}, {},&scope);}));
        list=take(host.content_item("rows"));
    }
    ~Fixture(){state->close();}
    gui::Widget item(std::string_view key,std::string_view id){return take(list.keyed_content_item(key,id));}
    std::string text(std::string_view key,std::string_view id){return take(item(key,id).snapshot()).text;}
};
void identity(){
    Fixture f;
    check(NativeRowActivation::constructions==3,"Repeated activation created more than the visible row backends");
    auto b=f.item("b","advance");const auto identity=take(b.snapshot()).id;
    good(b.activate());
    check(f.text("b","counter")=="2"&&f.text("a","counter")=="0","Repeated row state was shared across keys");
    auto data=f.scope->backend_0.items.get();
    data[1].title="Updated";std::swap(data[0],data[2]);
    f.scope->backend_0.items.set(data);
    check(NativeRowActivation::constructions==3&&take(f.item("b","advance").snapshot()).id==identity&&f.text("b","counter")=="2",
          "Reordering rebuilt an existing backend or lost its widget identity");
    check(f.text("b","Caption@0/caption")=="Updated"&&f.text("b","Caption@0/position")=="1"&&
          f.text("c","Caption@0/position")=="0"&&f.text("a","Caption@0/position")=="2"&&f.text("b","initial")=="B",
          "Repeated data, index or static snapshot was stale");
    auto graph=f.scope->native_lifetime.lock();
    const auto source_revision=f.scope->backend_0.items.revision();
    good(graph->begin_batch());f.scope->backend_0.first.set(1);f.scope->backend_0.count.set(1);good(graph->end_batch());
    check(take(f.list.keyed_content_keys())==std::vector<std::string>{"b"}&&NativeRowActivation::destructions==2,
          "A viewport change retained off-range row activations");
    check(f.scope->backend_0.items.revision()==source_revision&&f.scope->revision_0.get()==0,
          "Pure viewport state changed the source model or published a broad backend revision");
    good(graph->begin_batch());f.scope->backend_0.first.set(0);f.scope->backend_0.count.set(3);good(graph->end_batch());
    check(NativeRowActivation::constructions==5&&f.text("b","counter")=="2","Overlapping viewport rows lost state or new rows failed to activate");
    const auto before=take(f.list.snapshot()).content_generation;
    NativeRowActivation::fail_next=true;
    f.scope->backend_0.items.set({{"b","Should not publish",{}},{"d","New",{}}});
    check(graph->failures()->size()==1&&take(f.list.snapshot()).content_generation==before&&f.text("b","Caption@0/caption")=="Updated",
          "A failed row factory replaced the working list or published staged row data");
    f.scope->backend_0.items.set({{"b","Recovered",{}},{"a","A",{}}});
    check(graph->failures()->empty()&&f.text("b","Caption@0/caption")=="Recovered"&&NativeRowActivation::constructions==5,
          "A repeated list failed to recover without reconstructing retained rows");
    std::weak_ptr<Fixture::View::Scope> retired=f.scope;
    f.state->close();f.scope.reset();graph.reset();
    check(retired.expired()&&!b.activate()&&NativeRowActivation::constructions==NativeRowActivation::destructions,
          "Repeated factories or row callbacks retained a retired owner");
}
void viewport(){
    Fixture f;
    std::vector<NativeRepeatItem> data;
    for(int n=0;n<1000;++n)data.push_back({std::to_string(n),"Row "+std::to_string(n),{}});
    auto graph=f.scope->native_lifetime.lock();
    good(graph->begin_batch());f.scope->backend_0.first.set(100);f.scope->backend_0.count.set(12);f.scope->backend_0.items.set(data);good(graph->end_batch());
    check(NativeRowActivation::constructions==15&&NativeRowActivation::destructions==3&&take(f.list.keyed_content_keys()).size()==12&&
          take(f.state->snapshots()).size()<200,"A 1000-item source materialized off-viewport widgets or backends");
    const auto kept=f.item("105","advance");good(kept.activate());
    f.scope->backend_0.first.set(105);
    check(NativeRowActivation::constructions==20&&NativeRowActivation::destructions==8&&f.text("105","counter")=="2"&&
          f.text("105","Caption@0/position")=="105","Viewport scrolling rebuilt overlapping rows or lost global indices");
    data[999].id="0";f.scope->backend_0.items.set(data);
    check(graph->failures()->size()==1&&take(f.list.keyed_content_keys()).front()=="105",
          "An off-viewport duplicate key corrupted the visible list");
    f.state->close();f.scope.reset();graph.reset();
    check(NativeRowActivation::constructions==NativeRowActivation::destructions,"Viewport retirement leaked a row backend");
}
void nested(){
    using View=lvui_generated::NativeNestedRepetitionRuntime;
    auto state=std::make_shared<gui::detail::WidgetState>([](runtime::Task task){task();});
    auto host=take(state->content_host({},"root",{640,480}));
    std::shared_ptr<View::Scope> scope;
    good(host.set_content([&](gui::WidgetBuilder& builder){return View::build(builder,{}, {},&scope);}));
    scope->backend_0.items.set({{"g1","One",{{"a","A",{}},{"b","B",{}}}},{"g2","Two",{{"a","Other A",{}}}}});
    auto groups=take(host.content_item("groups"));
    auto children=take(groups.keyed_content_item("g1","children"));
    auto a=take(children.keyed_content_item("a","child"));const auto identity=take(a.snapshot()).id;
    auto other=take(take(groups.keyed_content_item("g2","children")).keyed_content_item("a","child"));
    check(take(other.snapshot()).id!=identity&&take(other.snapshot()).text=="Other A","Nested collections collided on a locally repeated key");
    auto data=scope->backend_0.items.get();
    data[0].children[0].title="Updated A";std::swap(data[0].children[0],data[0].children[1]);std::swap(data[0],data[1]);
    scope->backend_0.items.set(data);
    check(take(a.snapshot()).id==identity&&take(a.snapshot()).text=="Updated A"&&
          take(children.keyed_content_keys())==std::vector<std::string>{"b","a"},"A nested row lost data or identity after parent and child reordering");
    scope->backend_0.items.set({data[0]});
    check(!a.snapshot()&&other.snapshot().has_value(),"Removing a parent did not isolate its nested collection retirement");
    std::weak_ptr<View::Scope> retired=scope;
    state->close();scope.reset();
    check(retired.expired()&&!other.snapshot(),"Nested row closures retained the root owner");
}
void values(){
    using View=lvui_generated::NativeListValuesRuntime;
    auto state=std::make_shared<gui::detail::WidgetState>([](runtime::Task task){task();});
    auto host=take(state->content_host({},"root",{640,480}));
    std::shared_ptr<View::Scope> scope;
    good(host.set_content([&](gui::WidgetBuilder& builder){return View::build(builder,{}, {},&scope);}));
    auto list=take(host.content_item("words"));
    auto a=take(list.keyed_content_item("A","word"));const auto identity=take(a.snapshot()).id;
    good(scope->inferred_words->set({"B","A","C"}));
    check(take(list.keyed_content_keys())==std::vector<std::string>{"B","A","C"}&&take(a.snapshot()).id==identity,
          "A mutable list declaration did not drive native keyed updates");
    const auto epoch=take(list.snapshot()).content_generation;
    scope->backend_0.first.set(9);
    check(scope->inferred_numbers->get()==std::vector<double>{9,7}&&scope->inferred_initial->get()==std::vector<double>{0,1}&&
          take(list.snapshot()).content_generation==epoch,"An independent list dependency rebuilt unrelated repeated content or changed a static list");
    std::weak_ptr<View::Scope> retired=scope;
    state->close();scope.reset();
    check(retired.expired()&&!a.snapshot(),"Reactive list storage retained its retired scope");
}
void shared(){
    using View=lvui_generated::NativeSharedRepetitionRuntime;
    NativeLiveItem::constructions=NativeLiveItem::destructions=0;
    auto state=std::make_shared<gui::detail::WidgetState>([](runtime::Task task){task();});
    auto host=take(state->content_host({},"root",{640,480}));
    std::shared_ptr<View::Scope> scope;
    good(host.set_content([&](gui::WidgetBuilder& builder){return View::build(builder,{}, {},&scope);}));
    auto list=take(host.content_item("live"));
    auto title=take(list.keyed_content_item("a","Cell@0/title"));
    auto action=take(list.keyed_content_item("a","Cell@0/hit"));
    const auto identity=take(title.snapshot()).id;
    auto old=scope->backend_0.items.get()[0];
    good(action.activate());
    check(NativeLiveItem::constructions==2&&old->hits.get()==1,"Passing a noncopyable item backend cloned it or lost its method");
    auto replacement=std::make_shared<NativeLiveItem>("a","Replacement");
    auto data=scope->backend_0.items.get();data[0]=replacement;scope->backend_0.items.set(data);data.clear();
    check(take(title.snapshot()).id==identity&&take(title.snapshot()).text=="Replacement","Replacing a shared item with the same key rebuilt its row or failed to rebind properties");
    old->title.set("Stale");good(action.activate());
    check(take(title.snapshot()).text=="Replacement"&&replacement->hits.get()==1&&old->hits.get()==1&&scope->revision_0.get()==0,
          "A shared item replacement kept stale subscriptions, callbacks or a broad backend revision");
    old.reset();replacement.reset();
    scope->backend_0.items.set({std::make_shared<NativeLiveItem>("a","Third")});
    good(action.activate());
    check(take(title.snapshot()).text=="Third"&&take(take(list.keyed_content_item("a","Cell@0/hits")).snapshot()).text=="1"&&NativeLiveItem::constructions==4,
          "A retained row failed after its old shared backend lost all external owners");
    std::weak_ptr<View::Scope> retired=scope;
    state->close();scope.reset();
    check(retired.expired()&&NativeLiveItem::destructions==4&&!action.activate(),"Shared item rows retained a retired backend or root scope");
}
}
int main(int argc,char** argv){try{
    const std::string name=argc>1?argv[1]:"identity";
    if(name=="identity")identity();else if(name=="viewport")viewport();else if(name=="nested")nested();else if(name=="values")values();
    else if(name=="shared")shared();
    else throw std::runtime_error("Unknown native repetition test");
    std::cout<<"Generated native repetition "<<name<<" passed without a window\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
