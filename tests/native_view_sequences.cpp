#include "NativeViewSequencesRuntime.generated.h"
#include "NativeViewSequenceRowsRuntime.generated.h"
#include "NativeViewSequenceRootRuntime.generated.h"
#include "NativeViewSequenceOverrideRuntime.generated.h"
#include "../src/iiGUI/WidgetState.h"
#include <algorithm>
#include <iostream>
#include <stdexcept>
using namespace ii;
namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void good(core::Result<void> result){if(!result){if(result.error().cause)std::rethrow_exception(result.error().cause);throw std::runtime_error(result.error().message);}}
template<class T>T take(core::Result<T> result){if(!result){if(result.error().cause)std::rethrow_exception(result.error().cause);throw std::runtime_error(result.error().message);}return std::move(*result);}
template<class Generated=lvui_generated::NativeViewSequencesRuntime>struct Fixture final {
    using View=Generated;
    std::shared_ptr<gui::detail::WidgetState> state=std::make_shared<gui::detail::WidgetState>([](runtime::Task task){task();});
    gui::Widget host;std::shared_ptr<typename View::Scope> scope;
    Fixture(){NativeSequenceLocal::reset();NativeSequenceModel::constructions=NativeSequenceModel::destructions=0;
        host=take(state->content_host({},"root",{800,600}));good(host.set_content([&](gui::WidgetBuilder& widgets){return View::build(widgets,{}, {},&scope);}));}
    ~Fixture(){NativeSequenceLocal::retirement={};state->close();}
    std::vector<gui::Widget> find(std::string id){std::vector<gui::Widget> found;for(const auto& node:take(state->snapshots()))if(node.id==id||node.id.ends_with("/"+id))found.push_back(take(state->find(node.id)));return found;}
    gui::Widget one(std::string id){auto items=find(id);check(items.size()==1,"Expected one sequence widget");return items.front();}
    bool under(gui::Widget widget,const std::string& ancestor){auto id=take(widget.snapshot()).parent_id;while(!id.empty()){if(id==ancestor)return true;id=take(take(state->find(id)).snapshot()).parent_id;}return false;}
    std::vector<gui::WidgetSnapshot> children(gui::Widget parent){std::vector<gui::WidgetSnapshot> result;const auto id=take(parent.snapshot()).id;for(const auto& node:take(state->snapshots()))if(node.parent_id==id)result.push_back(node);return result;}
    gui::Widget conditional(){for(const auto& node:take(state->snapshots()))if(node.kind==gui::WidgetKind::content_host&&node.id!="root")return take(state->find(node.id));throw std::runtime_error("Missing conditional host");}
    void all(std::string id,std::string text,std::size_t count){auto items=find(id);check(items.size()==count,"A sequence consumer count changed unexpectedly");for(const auto& item:items)check(take(item.snapshot()).text==text,"A sequence value retained stale or shadowed data");}
};
void structure(){
    Fixture f;
    for(const auto id:{"first","second"}){
        auto nodes=f.children(f.one(id));check(nodes.size()==3,"A sequence inserted an intermediate layout widget or lost children");
        std::sort(nodes.begin(),nodes.end(),[](const auto& a,const auto& b){return a.bounds.x<b.bounds.x;});
        check(nodes[0].id.ends_with("/heading")&&nodes[1].id.ends_with("/cell")&&nodes[2].id.ends_with("/tail"),"The sequence did not preserve child order");
    }
    check(f.find("heading").size()==3&&f.find("tail").size()==3&&NativeSequenceModel::constructions==1&&NativeSequenceLocal::constructions==4,
          "Repeated sequence use shared widget identities or copied the borrowed root model");
    check(f.scope->backend_0.factories==3,"Structural inspection evaluated factories or a sequence duplicated its callback input");
    check(f.children(f.one("emptyPanel")).empty(),"An empty named sequence retained its default content or became an event");
}
void updates(){
    Fixture f;auto first=f.find("heading").front();const auto identity=take(first.snapshot()).id;
    f.scope->backend_0.title.set("Changed");f.all("heading","Changed",3);f.all("tail","Changed",3);f.all("cellTitle","Changed",3);f.all("local","Local",3);
    for(const auto& action:f.find("action"))good(action.activate());
    check(f.scope->backend_0.count.get()==5&&take(f.one("mainCount").snapshot()).text=="5"&&take(first.snapshot()).id==identity&&
          NativeSequenceLocal::constructions==4&&f.scope->backend_0.file_revision.get()==0,"A sequence update rebuilt widgets, used a shadowed receiver or mutated file state");
}
void independent(){
    Fixture f;auto actions=f.find("localAction");good(actions.front().activate());
    std::vector<std::string> counts;for(const auto& widget:f.find("count"))counts.push_back(take(widget.snapshot()).text);std::sort(counts.begin(),counts.end());
    check(counts==std::vector<std::string>{"100","100","101"}&&f.scope->backend_0.count.get()==2,"Separate sequence uses shared their private imported state");
}
void conditional(){
    Fixture f;auto old=f.find("action").back();f.scope->backend_0.ready.set(false);
    check(NativeSequenceLocal::destructions==2&&f.find("action").size()==2&&!old.activate(),"A removed sequence panel retained local imports or admitted a stale event");
    f.scope->backend_0.title.set("Remounted");f.scope->backend_0.ready.set(true);f.all("cellTitle","Remounted",3);
    check(NativeSequenceLocal::constructions==6&&f.scope->backend_0.factories==4,"A remounted sequence reused its retired local state or copied the root model");
}
void active(){
    Fixture f;auto host=f.conditional();const auto ancestor=take(host.snapshot()).id;gui::Widget action;
    for(const auto& widget:f.find("retire"))if(f.under(widget,ancestor))action=widget;
    check(!action.is_null(),"A conditional sequence receiver was missing");
    NativeSequenceLocal::retirement=[weak=std::weak_ptr(f.scope)]{if(auto owner=weak.lock())owner->backend_0.ready.set(false);};
    good(action.activate());NativeSequenceLocal::retirement={};
    check(NativeSequenceLocal::retirement_returns==1&&NativeSequenceLocal::destructions==2&&f.scope->active()&&!action.activate(),
          "A sequence removed by its own method destroyed its active receiver or retained its subtree");
}
void rollback(){
    Fixture f;auto old=f.find("action").front();const auto before=NativeSequenceLocal::live.size();
    NativeSequenceLocal::fail_at=NativeSequenceLocal::attempts+2;std::shared_ptr<Fixture<>::View::Scope> candidate;
    auto result=f.host.set_content([&](gui::WidgetBuilder& widgets){return Fixture<>::View::build(widgets,{}, {},&candidate);});candidate.reset();
    NativeSequenceLocal::fail_at=0;
    check(!result&&NativeSequenceLocal::live.size()==before&&NativeSequenceModel::destructions==1,"A failed sequence candidate leaked imported objects or replaced the valid view");
    good(old.activate());check(f.scope->backend_0.count.get()==3,"A failed candidate disconnected the old sequence receiver");
}
void lifetime(){
    Fixture f;auto old=f.find("action").front();std::weak_ptr<Fixture<>::View::Scope> retired=f.scope;
    f.state->close();f.scope.reset();
    check(retired.expired()&&NativeSequenceLocal::live.empty()&&NativeSequenceLocal::destructions==4&&NativeSequenceModel::destructions==1&&!old.activate(),
          "A retired sequence kept imports, subscriptions or admitted a saved callback");
}
void root(){
    Fixture<lvui_generated::NativeViewSequenceRootRuntime> f;const auto children=f.children(f.host);
    check(children.size()==1&&children.front().kind==gui::WidgetKind::label&&children.front().text=="One concrete root",
          "An empty sequence consumed the root or a singleton sequence introduced a wrapper");
}
void rows(){
    Fixture<lvui_generated::NativeViewSequenceRowsRuntime> f;
    auto titles=f.find("rowTitle");check(titles.size()==2&&NativeSequenceLocal::constructions==2,"A row sequence was not mounted once per stable key");
    std::string first;for(const auto& widget:titles)if(take(widget.snapshot()).text=="a")first=take(widget.snapshot()).id;
    f.scope->backend_0.items.set(std::vector<std::string>{"b","a","c"});
    check(NativeSequenceLocal::constructions==3,"Reordering row sequences recreated retained local objects");
    bool retained=false;for(const auto& widget:f.find("rowTitle"))if(take(widget.snapshot()).text=="a")retained=take(widget.snapshot()).id==first;
    check(retained,"A stable-key sequence row changed its widget identity");f.scope->backend_0.items.set(std::vector<std::string>{"a","c"});
    check(NativeSequenceLocal::destructions==1&&f.find("rowDetail").size()==2,"Removing a row sequence failed to retire its local state");
}
void named_override(){
    Fixture<lvui_generated::NativeViewSequenceOverrideRuntime> f;
    f.scope->backend_0.title.set("Definition");f.scope->backend_1.title.set("Caller");
    check(take(f.one("override").snapshot()).text=="Definition / Caller","A named sequence factory override lost its call or definition environment");
    good(f.one("overrideAction").activate());check(f.scope->backend_1.count.get()==3&&f.scope->backend_0.count.get()==2,
          "A captured View factory lost its named parameters or invoked the shadowed backend");
}
}
int main(int argc,char** argv){try{const std::string name=argc>1?argv[1]:"structure";
    if(name=="structure")structure();else if(name=="updates")updates();else if(name=="independent")independent();else if(name=="conditional")conditional();else if(name=="active")active();else if(name=="rollback")rollback();else if(name=="lifetime")lifetime();else if(name=="root")root();else if(name=="rows")rows();else if(name=="named_override")named_override();else throw std::runtime_error("Unknown View sequence widget test");
    std::cout<<"Native View sequences "<<name<<" passed without a window\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
