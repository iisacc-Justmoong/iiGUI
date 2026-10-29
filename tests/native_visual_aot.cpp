#include "NativeVisualRuntime.generated.h"
#include "NativeVisualConditionalRuntime.generated.h"
#include "NativeVisualEmptyRuntime.generated.h"
#include "../src/iiGUI/WidgetState.h"
#include <iostream>
using namespace ii;
namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void good(core::Result<void> value){if(!value){if(value.error().cause)std::rethrow_exception(value.error().cause);throw std::runtime_error(value.error().message);}}
template<class T>T take(core::Result<T> value){if(!value){if(value.error().cause)std::rethrow_exception(value.error().cause);throw std::runtime_error(value.error().message);}return std::move(*value);}
template<class View>struct Mounted final {
    std::shared_ptr<gui::detail::WidgetState> tree=std::make_shared<gui::detail::WidgetState>([](runtime::Task task){task();});
    gui::Widget host=take(tree->content_host({},"root",{640,640}));
    std::shared_ptr<typename View::Scope> scope;
    Mounted(){good(host.set_content([&](gui::WidgetBuilder& b){return View::build(b,{}, {},&scope);}));}
    ~Mounted(){tree->close();scope.reset();}
    auto& model(){return scope->backend_0;}
    gui::Widget widget(std::string_view suffix){for(const auto& item:take(tree->snapshots()))if(item.id==suffix||item.id.ends_with("/"+std::string(suffix)))return take(tree->find(item.id));throw std::runtime_error("Native visual widget not found");}
    auto failures(){return take(scope->native_lifetime.lock()->failures());}
};
using View=lvui_generated::NativeVisualRuntime;
void initial(){Mounted<View> f;check(f.model().first->builds==2&&f.model().second->builds==0&&take(f.model().first->fields[0].snapshot()).text=="Native draft","AOT did not build both native view activations");auto frame=take(f.tree->drawing_snapshot({640,640}));bool drawing=false;for(const auto& layer:frame.overlays)for(const auto& p:layer.panels)if(p.top.blue==.8f)drawing=true;for(const auto& p:frame.panels)if(p.top.blue==.8f)drawing=true;check(drawing,"AOT custom source did not reach the actual drawing scene");}
void identity(){Mounted<View> f;auto field=f.model().first->fields[0];good(field.focus());good(field.set_text("Working"));good(f.widget("rename").activate());good(f.widget("resize").activate());auto s=take(field.snapshot());check(f.model().first->builds==2&&s.focused&&s.text=="Working"&&take(f.widget("caption").snapshot()).text=="After","Unrelated binding or geometry update reconstructed native content");check(f.model().file_revision==0,"View commands changed file revision");}
void selection(){Mounted<View> f;auto a=f.model().first->fields[0],pinned=f.model().first->fields[1];good(pinned.set_text("Pinned"));good(f.widget("replace").activate());check(!a.snapshot()&&pinned.snapshot()&&take(pinned.snapshot()).text=="Pinned"&&f.model().first->builds==2&&f.model().second->builds==1,"Reactive native source replacement changed Static source or reused an old generation");}
void rollback(){Mounted<View> f;auto field=f.model().first->fields[0];good(field.focus());good(field.set_text("Retained"));good(f.widget("reject").activate());check(take(field.snapshot()).focused&&take(field.snapshot()).text=="Retained"&&!f.failures().empty(),"Rejected native factory replaced a valid view or hid the error");good(f.widget("recover").activate());check(f.failures().empty()&&field.snapshot()&&f.model().first->builds==2,"Native factory failure did not recover to the unchanged valid source");}
void invalid(){Mounted<View> f;auto field=f.model().first->fields[0];good(field.focus());good(field.set_text("Retained"));f.model().width.set(-1);check(!f.failures().empty()&&field.snapshot()&&f.model().first->builds==2,"Invalid reactive dimension reached the native provider");f.model().width.set(280);check(f.failures().empty()&&take(field.snapshot()).focused&&take(field.snapshot()).text=="Retained"&&f.model().first->builds==2,"Native dimension recovery lost input or rebuilt the source");}
void conditional(){Mounted<lvui_generated::NativeVisualConditionalRuntime> f;auto field=f.model().first->fields[0];f.model().shown.set(false);check(!field.snapshot()&&f.model().first->retires==1,"Conditional native view did not retire");f.model().source.set(f.model().second);check(f.model().second->builds==0,"Hidden native view built a candidate");f.model().shown.set(true);check(f.model().second->builds==1&&f.model().second->fields[0].snapshot()&&f.model().file_revision==0&&f.scope->revision_0.get()==0,"Conditional reactivation used an old source or changed backend/file revisions");}
void retirement(){Mounted<View> f;auto field=f.model().first->fields[0],button=f.widget("replace");good(f.host.clear_content());check(!field.snapshot()&&!button.activate(),"Retired AOT visual endpoint admitted a late write");}
void empty(){auto tree=std::make_shared<gui::detail::WidgetState>([](runtime::Task task){task();});auto host=take(tree->content_host({},"root",{640,480}));good(host.set_content([&](gui::WidgetBuilder& b){return lvui_generated::NativeVisualEmptyRuntime::build(b);}));bool found=false;for(const auto& item:take(tree->snapshots()))if(item.id=="empty"||item.id.ends_with("/empty"))found=true;check(found,"AOT rejected an empty asynchronous native source");tree->close();}
}
int main(int argc,char** argv){try{std::string n=argc>1?argv[1]:"initial";if(n=="initial")initial();else if(n=="identity")identity();else if(n=="selection")selection();else if(n=="rollback")rollback();else if(n=="invalid")invalid();else if(n=="conditional")conditional();else if(n=="retirement")retirement();else if(n=="empty")empty();else throw std::runtime_error("Unknown native visual AOT case");check(NativeVisualBackend::live==0&&NativeVisualSource::live==0,"Native visual AOT leaked a provider or backend");std::cout<<"Native visual AOT "<<n<<" passed without a window\n";}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
