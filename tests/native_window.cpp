#include "NativeWindowRuntime.generated.h"
#include "NativeWindowStaticRuntime.generated.h"
#include "NativeWindowRootRuntime.generated.h"
#include <LVUI/NativeWindowValues.h>
#include "../src/Widgets/WidgetState.h"
#include <iostream>
#include <limits>
#include <stdexcept>
#include <thread>
using namespace ii;
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
void good(core::Result<void> r){if(!r){if(r.error().cause)std::rethrow_exception(r.error().cause);throw std::runtime_error(r.error().message);}}
template<class T>T take(core::Result<T> r){if(!r){if(r.error().cause)std::rethrow_exception(r.error().cause);throw std::runtime_error(r.error().message);}return std::move(*r);}
struct Port {
    gui::WindowGeometryValue actual{"Initial",{640,420},{320,220}};
    std::vector<gui::WindowGeometry> updates;
    bool reject=false;
    std::shared_ptr<gui::detail::WidgetState> state;
    gui::WindowPropertiesTarget target;
    Port(){gui::detail::WidgetFrameHooks hooks;hooks.window_geometry=[this](const gui::WindowGeometry& value)->core::Result<void>{
        if(reject)return core::fail(core::ErrorCode::failed,"Native update rejected");
        auto next=gui::resolve_window_geometry(actual,value);if(!next)return std::unexpected(next.error());actual=*next;updates.push_back(value);return {};
    };state=std::make_shared<gui::detail::WidgetState>([](runtime::Task task){task();},gui::WidgetAppearance{},std::move(hooks));}
    ~Port(){state->close();}
    void construct(){good(state->construct([&](gui::WidgetBuilder& b)->core::Result<void>{target=take(b.window_properties());take(b.input_field({},"draft",{.text="Draft"}));return {};}));}
};
template<class View>struct Mounted final : Port {
    std::shared_ptr<typename View::Scope> scope;
    Mounted(){good(state->construct([&](gui::WidgetBuilder& b){target=take(b.window_properties());return View::build(b,{}, {},&scope);}));good(state->commit_window_properties());}
    auto& model(){return scope->backend_0;}
    gui::Widget field(){for(const auto& n:take(state->snapshots()))if(n.id=="draft"||n.id.ends_with("/draft"))return take(state->find(n.id));throw std::runtime_error("Missing Window input");}
};
using View=lvui_generated::NativeWindowRuntime;
void values(){
    auto current=gui::WindowGeometryValue{"Current",{900,700},{320,220}};
    auto title=take(gui::resolve_window_geometry(current,{.title="Changed"}));
    check(title.size==current.size&&title.title=="Changed","A title update restored a declared size");
    auto minimum=take(gui::resolve_window_geometry(current,{.minimum_width=1000}));
    check(minimum.size.width==1000&&minimum.size.height==700,"A larger minimum did not grow only the required dimension");
    check(!gui::resolve_window_geometry(current,{.width=200,.minimum_width=300}),"An explicit width below its minimum was accepted");
    enum class Accent {blue,green=6};
    check(lvui::native::window_accent<Accent>("green")==Accent::green,"Native accent String lost its index");
    bool rejected=false;try{(void)lvui::native::window_accent<Accent>(static_cast<Accent>(99));}catch(const std::invalid_argument&){rejected=true;}
    check(rejected,"An invalid native Accent enum was accepted");
}
void staging(){
    Port p;p.construct();auto value=gui::WindowProperties{.geometry={.title="Prepared",.width=800}};
    good(p.target.update(value));check(p.updates.empty()&&p.actual.title=="Initial","A staged candidate changed the live window");
    good(p.state->commit_window_properties());check(p.updates.size()==1&&p.actual.title=="Prepared"&&p.actual.size.width==800,"A committed candidate lost Window properties");
    good(p.state->commit_window_properties());check(p.updates.size()==1,"Window properties committed twice");
}
void validation(){
    Port p;p.construct();good(p.target.update({.geometry={.title="Valid",.width=800}}));good(p.state->commit_window_properties());
    const auto before=take(p.target.snapshot());
    for(int i=0;i<9;++i){auto value=before;value.geometry.title="Rejected";value.appearance=gui::lvrs_appearance();
        if(i==0)value.geometry.width=-1;if(i==1)value.geometry.height=std::numeric_limits<double>::quiet_NaN();
        if(i==2)value.geometry.minimum_width=32768;if(i==3)value.geometry.title=std::string("x\0y",3);
        if(i==4)value.appearance->font_family="";if(i==5)value.appearance->window.red=2;
        if(i==6)value.appearance->text_alpha[0]=std::numeric_limits<double>::infinity();
        if(i==7)value.appearance->button_height=0;if(i==8){value.geometry.width=200;value.geometry.minimum_width=300;}
        check(!p.target.update(value)&&take(p.target.snapshot())==before&&p.actual.title=="Valid"&&p.updates.size()==1,"Invalid Window properties partially changed retained state");
    }
}
void reactive(){
    Mounted<View> p;check(p.actual.title=="Review"&&p.actual.size==gui::Size{720,500},"Initial Window expressions were not applied");
    const auto count=p.model().title_reads;p.model().title.set("Compare");
    check(p.actual.title=="Compare"&&p.updates.size()==2,"A native title dependency did not update");
    p.model().width.set(800);check(p.model().title_reads==count+1,"An unrelated Window field re-executed the title method");
    p.model().other.set("Running");check(p.model().title_reads==count+1&&p.updates.size()==3,"An unrelated view value recomputed Window configuration");
    check(p.model().file_revision==0&&p.scope->revision_0.get()==0,"Window updates changed document or activation revision");
}
void deltas(){
    Mounted<View> p;p.actual.size={1200,900};p.model().title.set("User size");
    check(p.actual.size==gui::Size{1200,900}&&!p.updates.back().width&&!p.updates.back().height,"An unchanged declaration overwrote a user resize");
    p.model().width.set(800);check(p.actual.size==gui::Size{800,900}&&p.updates.back().width&&!p.updates.back().height,"Width binding overwrote the current height");
    p.model().minimumWidth.set(1000);
    check(p.actual.size==gui::Size{1000,900}&&p.actual.minimum_size.width==1000&&!p.updates.back().width,
          "A minimum-only change incorrectly constrained the window to an unchanged width declaration");
    p.model().title.set("Larger minimum");check(p.actual.title=="Larger minimum"&&p.actual.size.width==1000,
          "An unchanged prior width invalidated an unrelated update after the minimum grew");
    p.model().width.set(950);check(p.actual.size.width==1000,"A changed width below the actual minimum was accepted");
    p.model().width.set(1100);check(p.actual.size.width==1100,"Width did not recover above the actual minimum");
}
void preservation(){
    Mounted<View> p;auto field=p.field();good(field.set_text("Local edit"));good(field.focus());const auto count=take(p.state->snapshots()).size();
    p.model().font.set("Helvetica");p.model().color.set(gui::WidgetColor{.2,.3,.4,1});p.model().width.set(900);
    const auto current=take(field.snapshot());const auto appearance=take(p.state->appearance());
    check(current.text=="Local edit"&&current.focused&&take(p.state->snapshots()).size()==count,"A Window update rebuilt retained content or lost focus");
    check(appearance.font_family=="Helvetica"&&appearance.window==gui::WidgetColor{.2,.3,.4,1},"Window appearance did not update the widget tree");
}
void errors(){
    Mounted<View> p;auto value=take(p.target.snapshot());const auto count=p.updates.size();
    p.model().width.set(0);check(take(p.target.snapshot())==value&&p.updates.size()==count,"Failed Window evaluation replaced the last valid value");
    p.model().width.set(800);check(p.actual.size.width==800,"Window geometry did not recover after an invalid value");
    p.reject=true;p.model().title.set("Refused");check(p.actual.title=="Review"&&take(p.target.snapshot()).geometry.title=="Review","Failed native application changed the committed declaration");
    p.reject=false;p.model().title.set("Recovered");check(p.actual.title=="Recovered","Window endpoint did not recover after native rejection");
    value=take(p.target.snapshot());p.model().font.set("");check(take(p.target.snapshot())==value,"Invalid font changed the committed Window state");
    p.model().font.set("Helvetica");check(take(p.state->appearance()).font_family=="Helvetica","Window font did not recover");
}
void owner(){
    Port p;p.construct();good(p.state->commit_window_properties());bool rejected=false;
    std::thread other([&]{auto r=p.target.update({.geometry={.title="Wrong"}});rejected=!r&&r.error().code==core::ErrorCode::wrong_executor;});other.join();
    check(rejected&&p.updates.empty(),"A foreign thread changed Window properties");
}
void lifetime(){
    gui::WindowPropertiesTarget escaped;{Mounted<View> p;escaped=p.target;p.state->close();
        check(!escaped.update({.geometry={.title="Retired"}}),"A closed Window accepted updates");
        check(!escaped.snapshot(),"A closed Window exposed a live endpoint");}
    check(!escaped.snapshot()&&NativeWindowModel::live==0,"Window endpoint retained its native backend or owner");
}
void embedded(){
    Port p;std::shared_ptr<View::Scope> scope;
    good(p.state->construct([&](gui::WidgetBuilder& b)->core::Result<void>{
        p.target=take(b.window_properties());auto slot=take(b.content_host({},"slot",{500,300}));
        return slot.set_content([&](gui::WidgetBuilder& child){auto endpoint=child.window_properties();check(!endpoint&&endpoint.error().code==core::ErrorCode::unsupported,"An embedded view inherited Window authority");return View::build(child,{}, {},&scope);});
    }));good(p.state->commit_window_properties());scope->backend_0.title.set("Embedded");
    check(p.updates.empty()&&p.actual.title=="Initial","An embedded Window declaration changed its ancestor window");
}
void snapshots(){
    Mounted<lvui_generated::NativeWindowStaticRuntime> p;const auto old=take(p.target.snapshot());p.model().title.set("New");p.model().color.set(gui::WidgetColor{1,0,0,1});p.model().width.set(800);
    const auto current=take(p.target.snapshot());check(current.geometry.title==old.geometry.title&&current.appearance==old.appearance&&p.actual.size.width==800,"Static Window snapshots became reactive");
    Mounted<lvui_generated::NativeWindowRootRuntime> root;root.model().title.set("Forwarded");
    check(root.actual.title=="Forwarded"&&take(root.target.snapshot()).appearance->window==root.model().color.get(),"Root component inputs lost Window bindings");
}
void rollback(){
    Port p;auto result=p.state->construct([&](gui::WidgetBuilder& b)->core::Result<void>{p.target=take(b.window_properties());good(p.target.update({.geometry={.title="Rejected"}}));return core::fail(core::ErrorCode::failed,"Candidate rejected");});
    check(!result&&p.updates.empty()&&!p.target.snapshot(),"A failed candidate committed Window settings or kept an active endpoint");
    Port invalid;auto bad=invalid.state->construct([&](gui::WidgetBuilder& b){return View::build(b,{},[](View::Scope& scope){scope.backend_0.width.set(0);});});
    check(!bad&&invalid.updates.empty(),"Invalid initial Window values were silently accepted");
}
int main(int argc,char** argv){try{std::string name=argc>1?argv[1]:"values";
    if(name=="values")values();else if(name=="staging")staging();else if(name=="validation")validation();else if(name=="reactive")reactive();else if(name=="deltas")deltas();else if(name=="preservation")preservation();else if(name=="errors")errors();else if(name=="owner")owner();else if(name=="lifetime")lifetime();else if(name=="embedded")embedded();else if(name=="snapshots")snapshots();else if(name=="rollback")rollback();else throw std::runtime_error("Unknown Window test");
    check(NativeWindowModel::live==0,"Native Window backend leaked");std::cout<<"Native Window "<<name<<" passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
