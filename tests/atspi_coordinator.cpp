#include "../src/Widgets/AtSpiCoordinator.h"
#include <functional>
#include <limits>
#include <stdexcept>
#include <thread>
#include <vector>

using namespace ii::gui;
using namespace ii::gui::detail;
namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
WidgetSnapshot button(std::string text,bool enabled=true){WidgetSnapshot n{};n.kind=WidgetKind::button;n.id="save";n.text=std::move(text);n.attached=true;n.enabled=enabled;n.bounds=n.visual_bounds={1,2,20,10};return n;}
void dispatch_and_retirement(){
    std::vector<std::function<void()>> queue;int old_calls=0,new_calls=0;
    auto owner=AtSpiCoordinator::create("Editor",[&](auto task){queue.push_back(std::move(task));return true;});
    auto first=owner->open_window("First",{100,100}),second=owner->open_window("Second",{200,100});check(first&&second,"Cannot open windows");
    check(owner->publish(*first,{button("Save")},[&](WidgetInput){++old_calls;}),"Cannot publish original content");
    const auto frame=AtSpiApplication::window_path(*first),old=owner->model().object(frame)->children.at(0);
    check(owner->request(old,WidgetInputKind::activate)&&queue.size()==1,"Cannot admit original action");
    check(owner->publish(*first,{button("Save again")},[&](WidgetInput){++new_calls;}),"Cannot replace content");
    auto task=std::move(queue.back());queue.pop_back();task();check(old_calls==0&&new_calls==0,"A stale action reached replacement content");
    const auto current=owner->model().object(frame)->children.at(0);check(current!=old,"Replacement reused object identity");
    check(owner->request(current,WidgetInputKind::activate),"Cannot admit replacement action");
    check(owner->update(*first,{button("Disabled",false)}),"Cannot update active content");
    task=std::move(queue.back());queue.pop_back();task();check(new_calls==0,"A disabled action was delivered");
    check(owner->update(*first,{button("Enabled")})&&owner->request(current,WidgetInputKind::activate),"Cannot admit a live action");
    task=std::move(queue.back());queue.pop_back();task();check(new_calls==1,"A live action was not delivered");
    check(owner->request(current,WidgetInputKind::focus),"Cannot admit focus");check(owner->close_window(*first),"Cannot close window");
    task=std::move(queue.back());queue.pop_back();task();check(new_calls==1&&owner->model().object(AtSpiApplication::window_path(*second)).has_value(),"Closing a window did not retire only its work");
}
void lifetime_and_rejected_post(){
    std::function<void()> pending;int calls=0;
    auto owner=AtSpiCoordinator::create("Editor",[&](auto task){pending=std::move(task);return true;});
    auto window=owner->open_window("Document",{100,100});check(window&&owner->publish(*window,{button("Run")},[&](WidgetInput){++calls;}),"Cannot publish content");
    const auto path=owner->model().object(AtSpiApplication::window_path(*window))->children.at(0);
    bool foreign=false;std::thread other_thread([&]{foreign=owner->unpublish(*window)||owner->request(path,WidgetInputKind::activate);});other_thread.join();
    check(!foreign&&owner->model().object(path).has_value(),"A foreign thread changed the owner-thread model");
    check(owner->request(path,WidgetInputKind::activate),"Cannot admit action");owner.reset();pending();check(calls==0,"An action retained a closed application");
    auto rejected=AtSpiCoordinator::create("Editor",[](auto){return false;});auto other=rejected->open_window("Document",{100,100});
    check(other&&rejected->publish(*other,{button("Run")},[&](WidgetInput){++calls;}),"Cannot publish rejected-post test");
    const auto rejected_path=rejected->model().object(AtSpiApplication::window_path(*other))->children.at(0);
    check(!rejected->request(rejected_path,WidgetInputKind::activate)&&calls==0,"A rejected post was treated as accepted");
}
void text_reads_and_revision_guards(){
    std::vector<std::function<void()>> queue;std::vector<WidgetInput> delivered;
    auto owner=AtSpiCoordinator::create("Editor",[&](auto task){queue.push_back(std::move(task));return true;});
    auto window=owner->open_window("Document",{100,100});check(bool(window),"Cannot open text window");
    auto identity=std::make_shared<int>(0);WidgetSnapshot editor{};editor.kind=WidgetKind::input_field;editor.id="editor";editor.attached=true;
    editor.text_input=true;editor.focused=true;editor.text_length=4;editor.text_selection={1,2};editor.text_revision=TextInputRevision{identity,1,1,1};
    auto read=[](const AtSpiTextTarget& target){return AtSpiTextIndex::create("A😀B",target.selection,target.reversed);};
    check(owner->publish(*window,{editor},[&](WidgetInput value){delivered.push_back(std::move(value));},read),"Cannot publish editor");
    const auto path=owner->model().object(AtSpiApplication::window_path(*window))->children.at(0);auto text=owner->text(path);
    check(text&&text->index.characters()==3&&text->index.selection()==TextRange{1,1},"Text did not reach the accessibility coordinator");
    WidgetInput selection;selection.kind=WidgetInputKind::text_selection;selection.replacement_range=TextRange{3,0};selection.expected_text_revision=text->target.revision;
    check(owner->request_text(path,selection),"A valid selection was not queued");
    ++editor.text_revision->value;check(owner->update(*window,{editor}),"Cannot update text revision");auto stale=std::move(queue.back());queue.pop_back();stale();
    check(delivered.empty(),"A selection from an old text revision was delivered");
    text=owner->text(path);selection.expected_text_revision=text->target.revision;check(owner->request_text(path,selection),"Cannot queue current selection");
    auto current=std::move(queue.back());queue.pop_back();current();check(delivered.size()==1&&delivered[0].target=="editor"&&delivered[0].text_service,"Current selection lost its guarded target");
    editor.read_only=true;check(owner->update(*window,{editor}),"Cannot mark editor read-only");text=owner->text(path);
    WidgetInput edit;edit.kind=WidgetInputKind::text;edit.text="x";edit.replacement_range=TextRange{0,0};edit.expected_text_revision=text->target.revision;
    check(!owner->request_text(path,edit),"Read-only editor accepted a text edit");
}
void value_requests_and_retirement(){
    std::vector<std::function<void()>> queue;std::vector<double> delivered;bool available=true;
    auto owner=AtSpiCoordinator::create("Editor",[&](auto task){queue.push_back(std::move(task));return true;});
    auto window=owner->open_window("Document",{100,100});check(bool(window),"Cannot open value window");
    WidgetSnapshot slider{};slider.id="slider";slider.kind=WidgetKind::action_surface;slider.attached=true;
    slider.accessibility_semantics=AccessibilitySemantics{.role=AccessibilityRole::slider,.range=AccessibilityRange{0,10,2}};
    AtSpiCoordinator::ValueCallbacks callbacks{
        .can_set=[&](std::string_view id,double value){return available&&id=="slider"&&value>=0&&value<=10;},
        .set=[&](std::string_view,double value){delivered.push_back(value);}};
    check(owner->publish(*window,{slider},[](WidgetInput){},{},callbacks),"Cannot publish slider");
    const auto frame=AtSpiApplication::window_path(*window),path=owner->model().object(frame)->children.at(0);
    check(owner->request_value(path,4),"Valid value was not queued");
    slider.accessibility_semantics->range->maximum=3;check(owner->update(*window,{slider}),"Cannot change slider range");
    auto stale=std::move(queue.back());queue.pop_back();stale();check(delivered.empty(),"Stale value reached changed range");
    slider.accessibility_semantics->range->maximum=10;check(owner->update(*window,{slider})&&owner->request_value(path,4),"Cannot queue current value");
    auto current=std::move(queue.back());queue.pop_back();current();check(delivered==std::vector<double>{4},"Current value was not delivered");
    check(owner->request_value(path,5),"Cannot queue value before handler removal");available=false;
    auto unavailable=std::move(queue.back());queue.pop_back();unavailable();check(delivered==std::vector<double>{4},"Removed value handler received a queued request");available=true;
    check(!owner->request_value(path,11)&&!owner->request_value(path,std::numeric_limits<double>::quiet_NaN()),"Invalid value was admitted");
    check(owner->request_value(path,5),"Cannot queue a value before retirement");
    check(owner->publish(*window,{slider},[](WidgetInput){},{},callbacks),"Cannot replace value surface");
    auto retired=std::move(queue.back());queue.pop_back();retired();check(delivered==std::vector<double>{4},"Retired value reached replacement surface");
    const auto replacement=owner->model().object(frame)->children.at(0);available=false;
    check(!owner->request_value(replacement,5),"Unavailable value handler was accepted");
    available=true;callbacks.can_set=[&](std::string_view,double){owner.reset();return true;};
    check(owner->publish(*window,{slider},[](WidgetInput){},{},callbacks),"Cannot publish reentrant value handler");
    const auto final_path=owner->model().object(frame)->children.at(0);
    check(owner->request_value(final_path,5)&&!owner,"Value admission did not survive owner retirement");
    auto abandoned=std::move(queue.back());queue.pop_back();abandoned();check(delivered==std::vector<double>{4},"A request kept the retired owner alive");
}
}
int main(){dispatch_and_retirement();lifetime_and_rejected_post();text_reads_and_revision_guards();value_requests_and_retirement();}
