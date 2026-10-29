#include "NativeWorkRuntime.generated.h"
#include "../src/iiGUI/WidgetState.h"
#include <condition_variable>
#include <iostream>
#include <stdexcept>
using namespace ii;
using namespace std::chrono_literals;
namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void good(core::Result<void> result){if(!result){if(result.error().cause)std::rethrow_exception(result.error().cause);throw std::runtime_error(result.error().message);}}
template<class T>T take(core::Result<T> result){if(!result){if(result.error().cause)std::rethrow_exception(result.error().cause);throw std::runtime_error(result.error().message);}return std::move(*result);}
struct Gate final {std::mutex mutex;std::condition_variable changed;bool entered=false,released=false;
    void enter(){std::unique_lock lock(mutex);entered=true;changed.notify_all();if(!changed.wait_for(lock,5s,[&]{return released;}))throw std::runtime_error("Gate timeout");}
    void wait(){std::unique_lock lock(mutex);if(!changed.wait_for(lock,5s,[&]{return entered;}))throw std::runtime_error("Work timeout");}
    void release(){std::lock_guard lock(mutex);released=true;changed.notify_all();}
};
struct Fixture final {
    using View=lvui_generated::NativeWorkRuntime;
    runtime::Runtime workers{{.workers=1,.capacity=8}};runtime::EventLoop ui{64};lvui::native::WorkRuntime work{workers,ui.sender()};
    lvui::native::BackendFactories factories=work.factories().with<NativeWorkBackend>([](const lvui::native::BackendRequest& request){return NativeWorkBackend(lvui::native::WorkRuntime::scope(request));});
    std::shared_ptr<gui::detail::WidgetState> state=std::make_shared<gui::detail::WidgetState>([](runtime::Task task){task();});
    gui::Widget host;std::shared_ptr<View::Scope> scope;
    Fixture(){NativeWorkBackend::constructions=NativeWorkBackend::destructions=0;NativeWorkBackend::wrong_thread=false;NativeWorkBackend::body={};NativeWorkBackend::during_reply={};host=take(state->content_host({},"root",{640,420}));good(host.set_content([&](gui::WidgetBuilder& builder){return View::build(builder,{}, {},&scope,factories);}));}
    ~Fixture(){state->close();(void)work.join();scope.reset();NativeWorkBackend::body={};NativeWorkBackend::during_reply={};}
    gui::Widget find(std::string id){for(const auto& node:take(state->snapshots()))if(node.id==id||node.id.ends_with("/"+id))return take(state->find(node.id));throw std::runtime_error("Missing work widget: "+id);}
};
void result(){
    Fixture f;auto label=f.find("result");const auto identity=take(label.snapshot()).id;
    good(f.find("start").activate());check(bool(f.workers.wait_idle()),"Native job failed");f.ui.poll({64,1s});
    check(take(label.snapshot()).text=="42"&&take(label.snapshot()).id==identity&&f.scope->backend_0.starts==1&&NativeWorkBackend::constructions==2,
          "A plain C++ async result did not update its retained native view or restarted the job");
    check(f.scope->backend_0.file_revision.get()==0,"A native result publication changed file state");
}
void removal(){
    Fixture f;Gate gate;NativeWorkBackend::body=[&](auto){gate.enter();};auto local=f.find("localStart");good(local.activate());gate.wait();
    f.scope->backend_0.visible.set(false);
    check(!local.snapshot()&&NativeWorkBackend::destructions==0&&f.work.stats().retiring==1,"Removing a view blocked on or destroyed its active worker");
    gate.release();check(bool(f.workers.wait_idle()),"Worker did not finish");f.work.poll();
    check(NativeWorkBackend::destructions==1&&!NativeWorkBackend::wrong_thread&&f.work.stats().retiring==0,"An active local receiver did not retire on the UI thread");
}
void self(){
    Fixture f;NativeWorkBackend::during_reply=[&]{f.scope->backend_0.visible.set(false);};auto local=f.find("localStart");good(local.activate());
    check(bool(f.workers.wait_idle()),"Worker did not finish");f.ui.poll({64,1s});f.work.poll();
    check(!local.snapshot()&&NativeWorkBackend::destructions==1&&!NativeWorkBackend::wrong_thread,"A UI result removing its own receiver caused premature destruction");
}
void rollback(){
    Fixture f;Gate gate;NativeWorkBackend::body=[&](auto){gate.enter();};auto previous=f.find("result");
    auto rejected=f.host.set_content([&](gui::WidgetBuilder& builder){return Fixture::View::build(builder,{},[&](auto& scope){scope.backend_0.start();gate.wait();throw std::runtime_error("Configuration failed after work started");},nullptr,f.factories);});
    check(!rejected&&previous.snapshot()&&NativeWorkBackend::destructions==0,"A failed activation replaced the working view or destroyed its active job");
    gate.release();check(bool(f.workers.wait_idle()),"Failed activation work did not retire");f.work.poll();
    check(NativeWorkBackend::destructions==1&&f.scope->active()&&!NativeWorkBackend::wrong_thread,"Failed activation cleanup lost ownership or the prior view");
}
void errors(){
    Fixture f;NativeWorkBackend::body=[](auto){throw std::runtime_error("Detailed native failure");};good(f.find("start").activate());
    check(bool(f.workers.wait_idle()),"An activation failure escaped into the shared Runtime");f.ui.poll({64,1s});
    check(take(f.find("error").snapshot()).text.find("Detailed native failure")!=std::string::npos&&f.scope->backend_0.file_revision.get()==0,
          "An asynchronous native failure did not publish its evidence to the retained view");
}
void publishing_close(){
    Fixture f;auto context=f.scope->backend_0.work;
    auto watch=f.scope->revision_0.observe([&](const auto&,auto){if(f.scope->backend_0.result==42)f.state->close();});
    good(f.find("start").activate());check(bool(f.workers.wait_idle()),"Worker did not finish");f.ui.poll({64,1s});f.work.poll();
    check(!f.scope->active()&&!context->failure(),"Normal view retirement during result publication was reported as a failure");
}
}
int main(int argc,char** argv){try{const std::string name=argc>1?argv[1]:"result";
    if(name=="result")result();else if(name=="removal")removal();else if(name=="self")self();else if(name=="rollback")rollback();else if(name=="errors")errors();else if(name=="publishing_close")publishing_close();else throw std::runtime_error("Unknown activation work test");
    check(NativeWorkBackend::constructions==NativeWorkBackend::destructions&&!NativeWorkBackend::wrong_thread,
          "Final activation cleanup retained a backend or destroyed it outside the UI thread");
    std::cout<<"Native activation work widgets "<<name<<" passed without a window\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
