#include "NativeLayoutRuntime.generated.h"
#include "NativeLayoutAlignmentRuntime.generated.h"
#include "NativeLayoutConditionalRuntime.generated.h"
#include "NativeLayoutMeasuredRuntime.generated.h"
#include <LVUI/NativeWidgetLayout.h>
#include "../src/iiGUI/WidgetState.h"
#include <iostream>
#include <limits>
#include <stdexcept>
#include <thread>
using namespace ii;
void check(bool value,const char* message) { if (!value) throw std::runtime_error(message); }
void good(core::Result<void> value) {
    if (!value) { if(value.error().cause)std::rethrow_exception(value.error().cause);throw std::runtime_error(value.error().message); }
}
template<class T>T take(core::Result<T> value) {
    if (!value) { if(value.error().cause)std::rethrow_exception(value.error().cause);throw std::runtime_error(value.error().message); }
    return std::move(*value);
}
template<class View> struct Mounted final {
    std::shared_ptr<gui::detail::WidgetState> state=std::make_shared<gui::detail::WidgetState>([](runtime::Task task){task();});
    gui::Widget host=take(state->content_host({},"root",{640,640}));
    std::shared_ptr<typename View::Scope> scope;
    Mounted() { good(host.set_content([&](gui::WidgetBuilder& builder){return View::build(builder,{}, {},&scope);})); }
    ~Mounted() { state->close();scope.reset(); }
    gui::Widget widget(std::string_view suffix) {
        for (const auto& node:take(state->snapshots()))
            if (node.id==suffix || node.id.ends_with(std::string("/")+std::string(suffix))) return take(host.content_item(node.id));
        throw std::runtime_error("Layout widget not found");
    }
    gui::Rect bounds(std::string_view suffix) { return take(widget(suffix).snapshot()).bounds; }
};
using View=lvui_generated::NativeLayoutRuntime;
void values() {
    using namespace lvui::native;using A=gui::StackAlignment;
    auto value=stack_layout({.spacing=-1,.default_spacing=6,.padding=4,.left_padding=2,.width=200,.height=80,.flex=2});
    check(value.stack.spacing==6 && value.stack.insets==gui::Insets{2,4,4,4} && value.extent==gui::Size{200,80} && value.flex==2,
          "Native layout values lost fallback spacing, insets, extent or flex");
    auto axes=stack_alignment(std::string("BOTTOMTRAILING"),StackAxis::overlay);
    check(axes==LayoutAlignmentAxes{LayoutAlignment::trailing,LayoutAlignment::trailing},"Named alignment lost an axis");
    auto fallback=stack_layout({.alignment={LayoutAlignment::leading,LayoutAlignment::leading},.alignment_name="unrecognized",.axis=StackAxis::horizontal});
    check(gui::vertical_alignment(fallback.stack)==A::leading,"An unrecognized alignmentName did not preserve its base alignment");
    bool refused=false;try{const char* missing=nullptr;(void)stack_alignment(missing,StackAxis::vertical);}catch(const std::invalid_argument&){refused=true;}
    check(refused,"A null native alignment String was accepted");
}
struct Api final {
    std::shared_ptr<gui::detail::WidgetState> state=std::make_shared<gui::detail::WidgetState>([](runtime::Task task){task();});
    gui::Widget host=take(state->content_host({},"root",{640,480})),panel,field;
    Api() { good(host.set_content([&](gui::WidgetBuilder& builder)->core::Result<void>{
        auto outer=take(builder.vstack({},"outer",{.alignment=gui::StackAlignment::leading}));
        panel=take(builder.vstack(outer,"panel"));field=take(builder.input_field(panel,"field",{.text="Draft",.width=100}));return {};
    })); }
    ~Api(){state->close();}
};
void atomic() {
    Api f;good(f.field.focus());good(f.field.set_text("Local edit"));
    gui::StackLayoutOptions value{.stack={.spacing=3,.padding=5,.alignment=gui::StackAlignment::leading,.minimum_width=120,.minimum_height=60},.extent={200,100},.flex=0};
    good(f.panel.set_stack_layout(value));check(take(f.panel.stack_layout())==value,"Atomic stack update lost a field");
    const auto before=take(f.field.snapshot());good(f.panel.set_stack_layout(value));
    const auto after=take(f.field.snapshot());
    check(before.text==after.text && after.focused && before.id==after.id && take(f.panel.snapshot()).bounds.width==200,
          "A retained layout update replaced its children or input state");
}
void validation() {
    Api f;auto value=take(f.panel.stack_layout());value.stack.padding=5;value.extent={200,100};good(f.panel.set_stack_layout(value));
    for(int which=0;which<8;++which) {
        auto invalid=value;invalid.stack.padding=12;invalid.extent.width=300;
        if(which==0)invalid.flex=-1;
        if(which==1)invalid.stack.spacing=std::numeric_limits<double>::quiet_NaN();
        if(which==2)invalid.stack.minimum_width=8193;
        if(which==3)invalid.stack.insets=gui::Insets{0,0,0,-1};
        if(which==4)invalid.extent.height=std::numeric_limits<double>::infinity();
        if(which==5)invalid.stack.horizontal_alignment=static_cast<gui::StackAlignment>(99);
        if(which==6)invalid.stack.vertical_alignment=static_cast<gui::StackAlignment>(99);
        if(which==7)invalid.flex=4097;
        check(!f.panel.set_stack_layout(invalid) && take(f.panel.stack_layout())==value,"Invalid layout partially mutated a live stack");
    }
    check(!f.field.set_stack_layout(value) && !f.field.stack_layout(),"Non-stack widgets accepted stack metadata");
}
void owner() {
    Api f;bool rejected=false;
    std::thread worker([&]{auto current=f.panel.stack_layout();auto result=f.panel.set_stack_layout({});
        rejected=!current && current.error().code==core::ErrorCode::wrong_executor && !result && result.error().code==core::ErrorCode::wrong_executor;});
    worker.join();check(rejected,"A foreign executor accessed native layout state");
    f.state->close();check(!f.panel.stack_layout() && !f.panel.set_stack_layout({}),"A retired layout accepted access");
}
void updates() {
    Mounted<View> f;auto panel=f.widget("panel");auto& model=f.scope->backend_0;
    auto before=take(panel.stack_layout());check(before.stack.spacing==10 && before.extent==gui::Size{220,180} && model.gap_calls==1,"Dynamic layout did not initialize");
    model.gap.set(16);model.width.set(260);model.height.set(200);
    auto after=take(panel.stack_layout());check(after.stack.spacing==16 && after.extent==gui::Size{260,200} && model.gap_calls==2,"Dynamic dimensions or spacing did not update");
    model.gap.set(-1);model.defaultGap.set(7);
    check(take(panel.stack_layout()).stack.spacing==7 && model.gap_calls==3,"Default spacing did not follow the negative sentinel");
    model.visible.set(false);(void)take(f.state->snapshots());
    check(model.gap_calls==3 && model.file_revision.get()==0 && f.scope->revision_0.get()==0,"View state or inspection invoked unrelated backend work");
}
void preservation() {
    Mounted<View> f;auto draft=f.widget("draft"),panel=f.widget("panel");good(draft.focus());good(draft.set_text("User edit"));
    const auto before=take(draft.snapshot());const auto count=take(f.state->snapshots()).size();
    f.scope->backend_0.gap.set(20);f.scope->backend_0.width.set(280);f.scope->backend_0.alignment.set("trailing");
    const auto after=take(draft.snapshot());
    check(after.id==before.id && after.text=="User edit" && after.focused && take(f.state->snapshots()).size()==count && NativeLayoutModel::live==1,
          "Geometry changes rebuilt widgets, input state or backend objects");
    check(take(panel.snapshot()).bounds.width==280,"Retained geometry did not reach layout");
}
void alignment() {
    Mounted<lvui_generated::NativeLayoutAlignmentRuntime> f;
    auto relative=[&](std::string_view parent,std::string_view child){auto p=f.bounds(parent),c=f.bounds(child);return gui::Size{c.x-p.x,c.y-p.y};};
    check(relative("row","rowChild")==gui::Size{0,60} && relative("column","columnChild")==gui::Size{160,0} &&
          relative("overlay","overlayChild")==gui::Size{160,60} && relative("typed","typedChild")==gui::Size{160,0},"Initial stack alignment did not use its actual axes");
    auto& model=f.scope->backend_0;model.rowAlignment.set("top");model.columnAlignment.set("leading");model.overlayAlignment.set("topLeading");
    check(relative("row","rowChild")==gui::Size{0,0} && relative("column","columnChild")==gui::Size{0,0} &&
          relative("overlay","overlayChild")==gui::Size{0,0},"Changing alignment did not move retained children");
    model.overlayAlignment.set("stretch");check(f.bounds("overlayChild").width==200 && f.bounds("overlayChild").height==80,"Overlay stretch lost an axis");
    model.axes.set({gui::StackAlignment::leading,gui::StackAlignment::trailing});
    check(relative("typed","typedChild")==gui::Size{0,60},"Typed native alignment axes did not update");
    model.axes.set({static_cast<gui::StackAlignment>(99),gui::StackAlignment::center});
    check(relative("typed","typedChild")==gui::Size{0,60},"Invalid typed axes replaced a valid overlay layout");
}
void insets() {
    Mounted<View> f;auto panel=f.widget("panel");auto& model=f.scope->backend_0;
    check(take(panel.stack_layout()).stack.insets==gui::Insets{2,3,5,6},"Per-edge insets were dropped");
    auto p=f.bounds("panel"),draft=f.bounds("draft");check(draft.x==p.x+2 && draft.y==p.y+3,"Insets did not place content");
    model.left.set(9);model.top.set(11);model.right.set(13);model.bottom.set(15);
    check(take(panel.stack_layout()).stack.insets==gui::Insets{9,11,13,15},"Per-edge insets did not update");
    p=f.bounds("panel");draft=f.bounds("draft");check(draft.x==p.x+9 && draft.y==p.y+11,"Changing insets did not move retained content");
    model.intrinsic.set(true);model.measureHidden.set(true);
    check(take(panel.stack_layout()).stack.intrinsic_width && take(panel.stack_layout()).stack.measure_hidden_children,"Native measurement flags lost their bindings");
    model.width.set(0);model.height.set(0);model.childVisible.set(false);model.measureHidden.set(false);
    const auto hidden=take(panel.measure(600)).height;model.measureHidden.set(true);
    check(take(panel.measure(600)).height>hidden,"Measuring hidden children did not change natural height");
    model.alignment.set("stretch");model.intrinsic.set(false);
    const auto stretched=take(panel.measure(600)).width;model.intrinsic.set(true);
    check(stretched==600 && take(panel.measure(600)).width<stretched,"Intrinsic width did not preserve content measurement under stretch");
    model.alignment.set("trailing");model.alignmentName.set("LEADING");
    check(gui::horizontal_alignment(take(panel.stack_layout()).stack)==gui::StackAlignment::leading,"A valid alignmentName did not override the base alignment");
    model.alignmentName.set("top");
    check(gui::horizontal_alignment(take(panel.stack_layout()).stack)==gui::StackAlignment::trailing,"An unknown axis-specific name did not retain the base alignment");
}
void minimums() {
    Mounted<View> f;auto panel=f.widget("panel");auto& model=f.scope->backend_0;
    model.width.set(0);model.height.set(0);model.minimumWidth.set(300);model.minimumHeight.set(250);
    const auto natural=take(panel.measure(600));check(natural.width>=300 && natural.height>=250,"Dynamic minimum geometry did not affect natural measurement");
    model.minimumWidth.set(0);model.minimumHeight.set(0);
    const auto reduced=take(panel.measure(600));check(reduced.width<natural.width && reduced.height<natural.height,"Removing a minimum retained stale measurement cache");
}
void flex() {
    Mounted<lvui_generated::NativeLayoutAlignmentRuntime> f;
    check(f.bounds("weightA").width==100 && f.bounds("weightB").width==100,"Initial flex distribution is incorrect");
    f.scope->backend_0.firstWeight.set(3);
    check(f.bounds("weightA").width==150 && f.bounds("weightB").width==50 && f.bounds("commonA").width==150 && f.bounds("commonB").width==50,
          "Stack or common control flex failed to redistribute available space");
    f.scope->backend_0.childVisible.set(false);
    check(!take(f.widget("commonB").snapshot()).visible && f.bounds("commonA").width==200,"Common visibility did not participate in retained flex layout");
}
void errors() {
    Mounted<View> f;auto panel=f.widget("panel");auto good_value=take(panel.stack_layout());auto& model=f.scope->backend_0;
    model.width.set(-1);model.padding.set(9);
    check(take(panel.stack_layout())==good_value,"Invalid dynamic geometry partially changed the live layout");
    model.width.set(240);check(take(panel.stack_layout()).extent.width==240 && take(panel.stack_layout()).stack.padding==9,"Invalid geometry did not recover");
    good_value=take(panel.stack_layout());model.alignment.set("unknown");
    check(take(panel.stack_layout())==good_value,"Invalid dynamic alignment replaced a valid layout");
    model.alignment.set("leading");
    auto candidate=f.host.set_content([&](gui::WidgetBuilder& builder){return View::build(builder,{},[](auto& owner){owner.backend_0.width.set(-1);});});
    check(!candidate && take(panel.stack_layout())==good_value && NativeLayoutModel::live==1,"Failed initial layout replaced the live view or retained a candidate");
}
void lifetime() {
    check(NativeLayoutModel::live==0,"Previous layout activation leaked");
    {Mounted<View> f;auto panel=f.widget("panel");auto owner=f.scope;const auto calls=owner->backend_0.gap_calls;
     f.state->close();owner->backend_0.gap.set(22);check(owner->backend_0.gap_calls==calls && !panel.stack_layout(),"Retired layout subscriptions remained active");}
    check(NativeLayoutModel::live==0,"Layout bindings retained their backend owner");
}
void conditional() {
    Mounted<lvui_generated::NativeLayoutConditionalRuntime> f;auto old=f.widget("panel");auto& model=f.scope->backend_0;
    model.visible.set(false);model.gap.set(20);
    check(!old.stack_layout() && model.gap_calls==1,"A removed conditional layout kept its subscription");
    model.width.set(280);model.visible.set(true);
    auto current=f.widget("panel");check(take(current.stack_layout()).stack.spacing==20 && take(current.stack_layout()).extent.width==280 && model.gap_calls==2,
          "A reactivated layout did not use current native inputs");
}
void measured() {
    Mounted<lvui_generated::NativeLayoutMeasuredRuntime> f;auto& model=f.scope->backend_0;
    model.items.get()[0]->title.set("Short");
    auto list=f.widget("results"),scroll=f.widget("results/viewport");
    auto row=take(list.keyed_content_item("0","Tile@0/row"));auto button=take(list.keyed_content_item("0","Tile@0/hit"));
    good(button.activate());const auto first=take(row.snapshot()).bounds.height;const auto revision=model.items.revision();
    model.rowPadding.set(6);model.rowGap.set(9);(void)take(scroll.snapshot());
    const auto after=take(row.snapshot()).bounds.height;
    check(after==first+19 && take(button.snapshot()).text=="1" && model.items.revision()==revision && model.file_revision.get()==0 && f.scope->revision_0.get()==0,
          "Native row layout did not remeasure retained rows independently of file/model state");
    check(take(list.keyed_content_keys()).size()<20,"Changing row layout materialized an entire collection");
}
int main(int argc,char** argv) {
    try {
        std::string which=argc>1?argv[1]:"updates";
        if(which=="values")values();else if(which=="atomic")atomic();else if(which=="validation")validation();else if(which=="owner")owner();
        else if(which=="updates")updates();else if(which=="preservation")preservation();else if(which=="alignment")alignment();else if(which=="insets")insets();
        else if(which=="minimums")minimums();else if(which=="flex")flex();else if(which=="errors")errors();else if(which=="lifetime")lifetime();
        else if(which=="conditional")conditional();else if(which=="measured")measured();else throw std::runtime_error("Unknown native layout test");
        std::cout<<"Native layout "<<which<<" passed\n";
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
