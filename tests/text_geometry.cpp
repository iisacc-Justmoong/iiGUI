#include "../src/Widgets/WidgetState.h"
#include <cmath>
#include <limits>
#include <memory>
#include <stdexcept>
#include <thread>
#include <vector>

using namespace ii;
using namespace ii::gui;
using namespace ii::gui::detail;
namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(core::Result<T> value){if(!value)throw std::runtime_error(value.error().message);return std::move(*value);}
void good(core::Result<void> value){if(!value)throw std::runtime_error(value.error().message);}
bool near(float a,float b){return std::abs(a-b)<.01f;}

void geometry(){
    auto tree=std::make_shared<WidgetState>([](runtime::Task task){task();});
    auto root=take(tree->absolute_stack({},"root",{300,200}));
    auto editor=take(tree->absolute_stack(root,"editor",{120,60}));
    good(editor.set_frame({31,27,120,60}));
    auto provider=std::make_shared<TextInputProvider>();
    std::uint64_t revision=0;
    provider->state=[&]()->core::Result<TextInputState>{return TextInputState{.length=9,.revision=revision};};
    bool split=false;unsigned geometry_calls=0;
    provider->read=[&](TextRange r)->core::Result<TextContent>{
        if(split&&r.location==5)return core::fail(core::ErrorCode::out_of_range,"Split scalar");
        return TextContent{r,std::string(r.length,'a')};
    };
    bool invalid=false,reenter=false,retire=false,change_revision=false,too_many=false;
    auto geometry_provider=[&](TextRange range)->core::Result<TextInputGeometry>{
        ++geometry_calls;
        check(range==TextRange{1,4},"Geometry range changed in transit");
        if(reenter){auto snapshot=editor.snapshot();check(!snapshot&&snapshot.error().code==core::ErrorCode::busy,"Geometry provider reentered the widget tree");}
        if(retire)tree->close();
        if(change_revision)++revision;
        TextInputGeometry value{{2,3,1,12},{32,19,1,12},{{2,3,22,12},{4,19,28,12}}};
        if(invalid)value.selection[0].width=std::numeric_limits<float>::infinity();
        if(too_many)value.selection.resize(4097,{0,0,1,1});
        return value;
    };
    good(editor.set_text_input_provider(provider));
    good(editor.set_text_geometry_provider(geometry_provider));
    (void)take(tree->drawing_snapshot({300,200}));
    const auto snapshot=take(editor.snapshot());const auto value=take(editor.text_geometry({1,4}));
    check(near(value.start_caret.x,snapshot.visual_bounds.x+2)&&near(value.start_caret.y,snapshot.visual_bounds.y+3)&&
          near(value.end_caret.x,snapshot.visual_bounds.x+32)&&value.selection.size()==2&&
          near(value.selection[1].x,snapshot.visual_bounds.x+4)&&near(value.selection[1].y,snapshot.visual_bounds.y+19),
          "Local text geometry was not translated into the native surface");
    check(!editor.text_geometry({8,2})&&!editor.text_geometry({std::numeric_limits<std::size_t>::max(),1}),"Out-of-document geometry was accepted");
    split=true;const auto calls=geometry_calls;check(!editor.text_geometry({1,4})&&geometry_calls==calls,
        "Geometry provider received a range that split a scalar");split=false;
    reenter=true;(void)take(editor.text_geometry({1,4}));reenter=false;
    invalid=true;check(!editor.text_geometry({1,4}),"Non-finite provider geometry was accepted");invalid=false;
    too_many=true;check(!editor.text_geometry({1,4}),"Excessive provider geometry was accepted");too_many=false;
    change_revision=true;auto changed=editor.text_geometry({1,4});
    check(!changed&&changed.error().code==core::ErrorCode::busy,"Geometry survived a text revision change");change_revision=false;
    good(root.set_clip_content(true));good(editor.set_frame({290,150,120,60}));
    (void)take(tree->drawing_snapshot({300,200}));const auto clipped=take(editor.text_geometry({1,4}));
    check(clipped.selection.size()==2&&near(clipped.selection[0].width,8)&&near(clipped.selection[1].width,6)&&clipped.end_caret.width==0,
          "Ancestor clipping was not applied to text geometry");
    bool wrong_thread=false;std::thread thread([&]{auto result=editor.text_geometry({1,4});wrong_thread=!result&&result.error().code==core::ErrorCode::wrong_executor;});thread.join();
    check(wrong_thread,"Text geometry crossed the owner thread");
    good(editor.set_text_input_provider(nullptr));good(editor.set_text_input_provider(provider));
    auto cleared=editor.text_geometry({1,4});check(!cleared&&cleared.error().code==core::ErrorCode::unsupported,
        "A replacement text provider retained stale geometry");
    good(editor.set_text_geometry_provider(geometry_provider));
    good(editor.set_visible(false));const auto hidden=take(editor.text_geometry({1,4}));
    check(hidden.selection.empty()&&hidden.start_caret.width==0&&hidden.end_caret.width==0,
          "A hidden editor exposed visible text geometry");
    good(editor.set_visible(true));
    retire=true;auto result=editor.text_geometry({1,4});check(!result&&result.error().code==core::ErrorCode::closed,"Retired provider returned geometry");
}

void popup(){
    auto tree=std::make_shared<WidgetState>([](runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{200,100}));Widget popup,editor;
    auto provider=std::make_shared<TextInputProvider>();
    provider->state=[]()->core::Result<TextInputState>{return TextInputState{.length=1};};
    provider->read=[](TextRange range)->core::Result<TextContent>{return TextContent{range,std::string(range.length,'a')};};
    good(host.set_content([&](WidgetBuilder& builder)->core::Result<void>{
        auto root=take(builder.absolute_stack({},"root",{200,100}));
        popup=take(builder.popover(root,"popup",{.width=120,.height=80}));return {};
    }));
    good(popup.set_popup_content([&](WidgetBuilder& builder)->core::Result<void>{
        editor=take(builder.absolute_stack({},"editor",{100,40}));good(editor.set_text_input_provider(provider));
        return editor.set_text_geometry_provider([](TextRange)->core::Result<TextInputGeometry>{return TextInputGeometry{{4,5,1,10},{14,5,1,10},{{4,5,10,10}}};});
    }));
    good(popup.open_at(20,10));(void)take(tree->drawing_snapshot({200,100}));
    auto state=take(editor.snapshot());auto result=take(editor.text_geometry({0,1}));
    check(near(result.start_caret.x,state.visual_bounds.x+4*state.visual_bounds.width/state.bounds.width)&&
          near(result.start_caret.y,state.visual_bounds.y+5*state.visual_bounds.height/state.bounds.height),
          "Popup text geometry lost its visual transform");
    tree->close();
}

void unsupported(){
    auto tree=std::make_shared<WidgetState>([](runtime::Task task){task();});
    auto field=take(tree->input_field({},"field",{.text="abc"},{},{}));
    (void)take(tree->drawing_snapshot({120,40}));
    auto result=field.text_geometry({0,1});
    check(!result&&result.error().code==core::ErrorCode::unsupported,"A missing geometry provider invented a rectangle");
    auto empty=std::make_shared<WidgetState>([](runtime::Task task){task();});
    auto bare=take(empty->absolute_stack({},"unused",{10,10}));
    check(!bare.set_text_geometry_provider([](TextRange)->core::Result<TextInputGeometry>{return TextInputGeometry{};}),
          "Geometry registered without a text provider");
    empty->close();tree->close();
}

void nested(){
    auto tree=std::make_shared<WidgetState>([](runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{200,100}));Widget editor;
    auto provider=std::make_shared<TextInputProvider>();
    provider->state=[]()->core::Result<TextInputState>{return TextInputState{.length=2};};
    provider->read=[](TextRange r)->core::Result<TextContent>{return TextContent{r,std::string(r.length,'x')};};
    good(host.set_content([&](WidgetBuilder& builder)->core::Result<void>{
        auto root=take(builder.absolute_stack({},"root",{200,100}));
        editor=take(builder.absolute_stack(root,"editor",{80,40}));good(editor.set_frame({20,18,80,40}));
        good(editor.set_text_input_provider(provider));
        return editor.set_text_geometry_provider([](TextRange)->core::Result<TextInputGeometry>{return TextInputGeometry{{5,7,1,10},{15,7,1,10},{{5,7,10,10}}};});
    }));
    (void)take(tree->drawing_snapshot({200,100}));
    auto snapshot=take(editor.snapshot());auto value=take(editor.text_geometry({0,2}));
    check(near(value.start_caret.x,snapshot.visual_bounds.x+5)&&near(value.start_caret.y,snapshot.visual_bounds.y+7),
          "Nested content geometry lost its host offset");
    tree->close();
}
}
int main(){geometry();unsupported();nested();popup();}
