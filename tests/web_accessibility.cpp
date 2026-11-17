#include "../src/Widgets/WebAccessibility.h"
#include <algorithm>
#include <stdexcept>

using namespace ii::gui;
using namespace ii::gui::detail;

static void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
static WidgetSnapshot node(std::string id,std::string parent,WidgetKind kind){
    WidgetSnapshot result{};result.id=std::move(id);result.parent_id=std::move(parent);
    result.kind=kind;result.attached=true;result.bounds=result.visual_bounds={0,0,40,20};return result;
}
int main(){
    auto root=node("root","",WidgetKind::vertical_stack);
    auto label=node("label","root",WidgetKind::label);label.text="Count \"one\"\n한글";
    auto button=node("button","root",WidgetKind::button);button.text="Increment";button.bounds=button.visual_bounds={17,31,80,44};
    auto box=node("box","root",WidgetKind::action_surface);box.action_role=ActionRole::check_box;box.checked=true;box.text="Choice";
    auto toggle=node("toggle","root",WidgetKind::action_surface);toggle.action_role=ActionRole::toggle_switch;toggle.text="Snap";toggle.enabled=false;
    auto radio=node("radio","root",WidgetKind::radio_button);radio.checked=true;radio.text="A";
    auto slider=node("slider","root",WidgetKind::action_surface);
    slider.accessibility_semantics=AccessibilitySemantics{.role=AccessibilityRole::slider,.name="Size",.range=AccessibilityRange{-1,10,2.5}};
    auto progress=node("progress","root",WidgetKind::progress);progress.minimum=0;progress.maximum=100;progress.value=50;
    auto hidden=node("hidden","root",WidgetKind::button);hidden.visible=false;
    auto secret=node("secret","root",WidgetKind::input_field);secret.password=true;secret.text="private";
    auto nodes=web_accessibility_nodes({root,label,button,box,toggle,radio,slider,progress,hidden,secret});
    check(nodes.size()==7,"Unsupported, hidden or structural nodes were exposed");
    check(nodes[0].role=="text"&&nodes[0].name==label.text,"Text name was lost");
    check(nodes[1].role=="button"&&nodes[1].actionable,"Button action was lost");
    check(nodes[1].bounds.x==17&&nodes[1].bounds.y==31&&nodes[1].bounds.width==80&&nodes[1].bounds.height==44,"Visible action bounds were lost");
    check(nodes[2].role=="checkbox"&&nodes[2].checked,"Checkbox state was lost");
    check(nodes[3].role=="switch"&&!nodes[3].enabled&&!nodes[3].actionable,"Disabled switch state was lost");
    check(nodes[4].role=="radio"&&nodes[4].checked,"Radio state was lost");
    check(nodes[5].role=="slider"&&nodes[5].actionable&&nodes[5].range&&nodes[5].range->value==2.5,"Slider range was lost");
    check(nodes[6].role=="progressbar"&&!nodes[6].actionable&&nodes[6].range&&nodes[6].range->value==50,"Progress range was lost");
    const auto json=web_accessibility_json(nodes);
    check(json.find("Count \\\"one\\\"")!=std::string::npos,"JSON text was not escaped");
    check(json.find("\\u000a한글")!=std::string::npos,"Control characters or UTF-8 were corrupted");
    check(json.find("private")==std::string::npos&&json.find("hidden")==std::string::npos,"Private or hidden text escaped");
    check(json.find("\"minimum\":-1")!=std::string::npos&&json.find("\"value\":2.5")!=std::string::npos,"Range JSON lost precision or sign");
    check(json.find("\"bounds\":[17,31,80,44]")!=std::string::npos,"Action bounds were not projected for host focus");
}
