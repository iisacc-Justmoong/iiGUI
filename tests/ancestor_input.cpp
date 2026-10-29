#include "../src/iiGUI/WidgetState.h"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace ii;
namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void good(core::Result<void> result){if(!result){if(result.error().cause)std::rethrow_exception(result.error().cause);throw std::runtime_error(result.error().message);}}
template<class T>T take(core::Result<T> result){if(!result){if(result.error().cause)std::rethrow_exception(result.error().cause);throw std::runtime_error(result.error().message);}return std::move(*result);}
struct Fixture final {
    std::shared_ptr<gui::detail::WidgetState> state=std::make_shared<gui::detail::WidgetState>([](runtime::Task task){task();});
    gui::Widget host,panel,button,text,slider,custom_slider,custom_text,custom_stepper;
    int commands=0,clicks=0,adjusted=0,commits=0;std::optional<gui::WidgetPoint> local_pointer;std::vector<gui::WidgetInput> seen;
    Fixture(){
        host=take(state->content_host({},"root",{640,420}));
        good(host.set_content([&](gui::WidgetBuilder& builder)->core::Result<void>{
            panel=take(builder.vstack({},"panel"));
            good(panel.set_input_handler([&](const gui::WidgetInput& event)->core::Result<bool>{
                if(event.kind!=gui::WidgetInputKind::key_down)return false;
                seen.push_back(event);
                if(event.key==gui::WidgetKey::down&&!event.native_navigation_active){++commands;return true;}
                return false;
            }));
            (void)take(builder.spacer(panel,"before-content",20));
            auto content=take(builder.content_host(panel,"content",{}));
            return content.set_content([&](gui::WidgetBuilder& child)->core::Result<void>{
                auto root=take(child.vstack({},"row"));
                button=take(child.button(root,"action",{.text="Action"},[&]{++clicks;}));
                text=take(child.input_field(root,"text",{.text="AB"}));
                slider=take(child.slider(root,"slider",{.value=.5}));
                custom_slider=take(child.action_surface(root,"custom-slider",{.accessibility_name="Custom slider",.width=100,.height=32}));
                good(custom_slider.set_accessibility_semantics(gui::AccessibilitySemantics{.role=gui::AccessibilityRole::slider,.name="Custom slider",.range=gui::AccessibilityRange{0,1,.5}}));
                good(custom_slider.set_input_handler([&](const gui::WidgetInput& event)->core::Result<bool>{
                    if(event.kind==gui::WidgetInputKind::pointer_down){local_pointer=event.local_position;return true;}
                    if(event.kind==gui::WidgetInputKind::key_down&&event.key==gui::WidgetKey::down){++adjusted;return true;}return false;
                }));
                custom_text=take(child.absolute_stack(root,"custom-text",{100,30}));
                auto provider=std::make_shared<gui::TextInputProvider>();
                provider->state=[]()->core::Result<gui::TextInputState>{return gui::TextInputState{.caret={1,1,1,12}};};
                provider->read=[](gui::TextRange r)->core::Result<gui::TextContent>{return gui::TextContent{r,{}};};
                good(custom_text.set_text_input_provider(provider));
                good(custom_text.set_input_handler([&](const gui::WidgetInput& event)->core::Result<bool>{
                    if(event.kind==gui::WidgetInputKind::commit_composition){++commits;return true;}return false;
                }));
                custom_stepper=take(child.action_surface(root,"custom-stepper",{.width=18,.height=18}));
                good(custom_stepper.set_accessibility_semantics(gui::AccessibilitySemantics{.role=gui::AccessibilityRole::stepper,.name="Stepper"}));
                good(custom_stepper.set_input_handler([&](const gui::WidgetInput& event)->core::Result<bool>{
                    if(event.kind==gui::WidgetInputKind::key_down&&event.key==gui::WidgetKey::down){++adjusted;return true;}return false;
                }));return {};
            });
        }));
    }
    ~Fixture(){state->close();}
};
void ancestor(){
    Fixture f;good(f.button.focus());
    good(f.state->input({.kind=gui::WidgetInputKind::key_down,.key=gui::WidgetKey::down}));
    check(f.commands==1&&f.seen.size()==1&&f.seen.back().native_control_active&&!f.seen.back().native_navigation_active,
          "An ancestor did not receive a nested row button's navigation key exactly once");
    good(f.state->input({.kind=gui::WidgetInputKind::key_down,.key=gui::WidgetKey::enter}));
    good(f.state->input({.kind=gui::WidgetInputKind::key_up,.key=gui::WidgetKey::enter}));
    check(f.clicks==1,"An unhandled ancestor shortcut suppressed the focused button's activation");
}
void text(){
    Fixture f;good(f.text.focus());
    good(f.state->input({.kind=gui::WidgetInputKind::key_down,.key=gui::WidgetKey::down}));
    check(f.commands==0&&f.seen.size()==1&&f.seen.back().text_input_active&&f.seen.back().native_navigation_active,
          "A nested text field lost its text-input guard at an ancestor");
    good(f.state->input({.kind=gui::WidgetInputKind::text,.text="C"}));
    check(take(f.text.snapshot()).text.find('C')!=std::string::npos,"Native text editing stopped after ancestor routing");
    good(f.custom_text.focus());
    good(f.state->input({.kind=gui::WidgetInputKind::commit_composition}));
    check(f.commits==1,"A nested custom text provider lost its composition commit");
}
void adjustment(){
    Fixture f;good(f.slider.focus());
    const auto before=take(f.slider.snapshot()).value;
    good(f.state->input({.kind=gui::WidgetInputKind::key_down,.key=gui::WidgetKey::down}));
    check(f.commands==0&&f.seen.size()==1&&f.seen.back().native_navigation_active&&take(f.slider.snapshot()).value<before,
          "Collection shortcuts stole a nested slider's arrow key");
    good(f.custom_slider.focus());f.seen.clear();
    good(f.state->input({.kind=gui::WidgetInputKind::key_down,.key=gui::WidgetKey::down}));
    check(f.commands==0&&f.seen.size()==1&&f.seen.back().native_navigation_active&&f.adjusted==1,
          "Collection shortcuts stole a nested custom slider's arrow key");
    auto bounds=take(f.custom_slider.snapshot()).bounds;
    good(f.state->input({.kind=gui::WidgetInputKind::pointer_down,.x=bounds.x+5,.y=bounds.y+7}));
    check(f.local_pointer&&std::abs(f.local_pointer->x-5)<.001&&std::abs(f.local_pointer->y-7)<.001,
          "An input handler did not receive coordinates relative to its nested surface");
    good(f.state->input({.kind=gui::WidgetInputKind::pointer_up,.x=bounds.x+5,.y=bounds.y+7}));
    good(f.custom_stepper.focus());f.seen.clear();
    good(f.state->input({.kind=gui::WidgetInputKind::key_down,.key=gui::WidgetKey::down}));
    check(f.commands==0&&f.seen.size()==1&&f.seen.back().native_navigation_active&&f.adjusted==2,
          "Collection shortcuts stole a nested custom Stepper's arrow key");
    auto invalid=gui::WidgetInput{};invalid.local_position=gui::WidgetPoint{std::numeric_limits<double>::quiet_NaN(),0};
    check(!f.state->input(invalid),"Non-finite local input position was accepted");
}
}
int main(int argc,char** argv){try{
    const std::string name=argc>1?argv[1]:"ancestor";
    if(name=="ancestor")ancestor();else if(name=="text")text();else if(name=="adjustment")adjustment();else throw std::runtime_error("Unknown ancestor input test");
    std::cout<<"Native ancestor input "<<name<<" passed without a window\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
