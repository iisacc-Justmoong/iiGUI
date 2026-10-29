#include "NativeSharedMethodRuntime.generated.h"
#include "../src/Widgets/WidgetState.h"
#include <iostream>
#include <stdexcept>
using namespace ii;
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
template<class T>T take(core::Result<T> value){if(!value){if(value.error().cause)std::rethrow_exception(value.error().cause);throw std::runtime_error(value.error().message);}return std::move(*value);}
void good(core::Result<void> value){if(!value)throw std::runtime_error(value.error().message);}
using View=lvui_generated::NativeSharedMethodRuntime;
struct Mounted final {
    std::shared_ptr<gui::detail::WidgetState> state=std::make_shared<gui::detail::WidgetState>([](runtime::Task task){task();});
    gui::Widget host=take(state->content_host({},"root",{640,480}));std::shared_ptr<View::Scope> scope;
    Mounted(){good(host.set_content([&](gui::WidgetBuilder& builder){return View::build(builder,{}, {},&scope);}));}
    ~Mounted(){state->close();scope.reset();}
    gui::Widget widget(std::string_view id){
        std::string resolved;
        for(const auto& item:take(state->snapshots()))if(item.id==id||item.id.ends_with("/"+std::string(id))){
            check(resolved.empty(),"Shared method widget id was ambiguous");resolved=item.id;
        }
        check(!resolved.empty(),"Shared method widget was not found");return take(state->find(resolved));
    }
    std::string summary(){return take(widget("summary").snapshot()).text;}
};
void updates(){Mounted m;check(m.summary()=="6","Initial shared getter result was wrong");m.scope->backend_0.set(5);check(m.summary()=="10"&&m.scope->backend_0.file_revision.get()==0&&m.scope->revision_0.get()==0,"Shared getter observation required broad publication or changed document state");}
void editing(){Mounted m;auto edit=m.widget("edit");good(edit.set_text("Working"));good(edit.focus());auto count=take(m.state->snapshots()).size();m.scope->backend_0.set(7);auto result=take(edit.snapshot());check(m.summary()=="14"&&result.focused&&result.text=="Working"&&take(m.state->snapshots()).size()==count,"A shared module update rebuilt or disturbed editing state");}
void retirement(){Mounted m;auto retained=m.scope;auto graph=retained->native_lifetime.lock();good(m.host.clear_content());auto closed=graph->stats();check(!graph->available()&&!closed&&closed.error().code==core::ErrorCode::closed&&!retained->active(),"Shared module observation remained admitted after retirement");retained->backend_0.set(8);check(take(m.state->snapshots()).size()==1,"A retired shared model updated removed view content");}
int main(int argc,char** argv){try{std::string name=argc>1?argv[1]:"updates";if(name=="updates")updates();else if(name=="editing")editing();else if(name=="retirement")retirement();else throw std::runtime_error("Unknown shared method test");check(NativeSharedMethodBackend::live==0,"Shared method backend leaked");std::cout<<"Shared native getter "<<name<<" passed\n";}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
