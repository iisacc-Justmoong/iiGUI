#include "../src/Widgets/WidgetState.h"
#include <iostream>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

using namespace ii;
namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void good(core::Result<void> result){if(!result){if(result.error().cause)std::rethrow_exception(result.error().cause);throw std::runtime_error(result.error().message);}}
template<class T>T take(core::Result<T> result){if(!result){if(result.error().cause)std::rethrow_exception(result.error().cause);throw std::runtime_error(result.error().message);}return std::move(*result);}

struct Fixture final {
    std::shared_ptr<gui::detail::WidgetState> state=std::make_shared<gui::detail::WidgetState>([](runtime::Task task){task();});
    gui::Widget host,root,target;
    int clicks=0;
    std::vector<gui::WidgetInteraction> external;
    std::shared_ptr<gui::WidgetInteractionPort> port;
    Fixture(){
        host=take(state->content_host({},"host",{240,100}));
        good(host.set_content([&](gui::WidgetBuilder& b)->core::Result<void>{
            root=take(b.absolute_stack({},"root",{240,100}));
            target=take(b.button(root,"button",{.text="Button",.height=40,.width=100},[&]{++clicks;}));
            good(target.set_frame({20,20,100,40}));
            port=std::make_shared<gui::WidgetInteractionPort>([this](const auto& value){external.push_back(value);});
            return target.observe_interaction(port);
        }));
    }
    ~Fixture(){state->close();}
    void flush(){(void)take(target.snapshot());}
    void move(double x=30,double y=30){good(state->input({.kind=gui::WidgetInputKind::pointer_move,.x=x,.y=y}));}
};

void committed(){
    std::vector<gui::WidgetInteraction> changes;Fixture f;
    good(f.target.set_interaction_handler([&](const auto& value){changes.push_back(value);}));f.flush();
    check(changes.size()==1&&changes.back().alive&&changes.back().visible&&changes.back().enabled&&f.external.size()==1,"Initial committed interaction was not delivered to both channels");
    f.move();check(changes.back().hovered&&f.external.back().hovered,"Hover was not observed after input committed");
    good(f.state->input({.kind=gui::WidgetInputKind::pointer_down,.x=30,.y=30,.buttons=1}));
    check(changes.back().pressed&&changes.back().focused,"Press and focus were delivered before commit");
    good(f.state->input({.kind=gui::WidgetInputKind::pointer_up,.x=30,.y=30}));
    check(f.clicks==1&&!changes.back().pressed,"Interaction handling changed activation");
    auto count=changes.size();good(f.target.request_repaint());f.flush();check(changes.size()==count,"Redraw duplicated unchanged interaction");
    using Origin=gui::WidgetPressOrigin;
    check(take(f.target.press_origin())==Origin::none,"Idle widget retained a press origin");
    good(f.state->input({.kind=gui::WidgetInputKind::pointer_down,.x=30,.y=30,.buttons=1}));
    check(take(f.target.press_origin())==Origin::pointer&&take(f.root.press_origin())==Origin::pointer,
          "Pointer press origin was not reported on the target and ancestor");
    f.move(200,90);check(take(f.target.press_origin())==Origin::pointer,"Leaving lost the captured pointer origin");
    good(f.state->input({.kind=gui::WidgetInputKind::pointer_up,.x=200,.y=90}));
    check(take(f.target.press_origin())==Origin::none,"Pointer release retained its origin");
    good(f.target.focus());good(f.state->input({.kind=gui::WidgetInputKind::key_down,.key=gui::WidgetKey::space}));
    check(take(f.target.press_origin())==Origin::keyboard,"Keyboard press origin was not reported");
    f.move();check(take(f.target.press_origin())==Origin::keyboard,"Pointer hover replaced the keyboard origin");
    good(f.state->input({.kind=gui::WidgetInputKind::key_down,.key=gui::WidgetKey::escape}));
    check(take(f.target.press_origin())==Origin::none,"Cancellation retained its press origin");
    bool rejected=false;std::thread worker([&]{auto result=f.target.press_origin();
        rejected=!result&&result.error().code==core::ErrorCode::wrong_executor;});worker.join();
    check(rejected,"Press origin crossed its owner thread");
    good(f.host.clear_content());check(!f.target.press_origin(),"Retired widget retained a press origin");
}
void reentrant(){
    std::vector<gui::WidgetInteraction> first,second;Fixture f;
    good(f.target.set_interaction_handler([&](const auto& value){first.push_back(value);if(value.hovered)good(f.target.set_interaction_handler([&](const auto& next){second.push_back(next);}));}));
    f.flush();f.move();
    check(first.size()==2&&second.size()==1&&second.back().hovered,"Replacement during delivery did not receive the committed state");
    good(f.target.set_interaction_handler({}));f.move(200,90);
    check(second.size()==1&&!f.external.back().hovered,"Clearing the owned handler affected external observation");
}
void fanout(){
    std::vector<gui::WidgetInteraction> changes;Fixture f;good(f.target.set_interaction_handler([](const auto&){throw std::runtime_error("handler failed");}));
    auto result=f.target.snapshot();
    check(!result&&f.external.size()==1,"A failed handler starved the external subscriber");
    good(f.target.set_interaction_handler([&](const auto& value){changes.push_back(value);}));
    good(f.target.set_enabled(false));f.flush();
    check(changes.size()==1&&!changes.back().enabled&&!f.external.back().enabled,"A failed handler lost the committed state");
}
void retirement(){
    std::vector<gui::WidgetInteraction> changes;Fixture f;
    good(f.target.set_interaction_handler([&](const auto& value){changes.push_back(value);if(value.alive&&value.hovered)good(f.host.clear_content());}));
    f.flush();f.move();
    check(changes.size()==3&&changes[1].hovered&&!changes.back().alive&&!f.external.back().alive&&!f.target.interaction(),"Reentrant retirement did not deliver a final state safely");
}
void capacity(){
    auto state=std::make_shared<gui::detail::WidgetState>([](runtime::Task task){task();});
    auto host=take(state->content_host({},"host",{240,100}));gui::Widget first;std::size_t deliveries=0;
    std::vector<std::shared_ptr<gui::WidgetInteractionPort>> ports;
    good(host.set_content([&](gui::WidgetBuilder& b)->core::Result<void>{
        auto root=take(b.absolute_stack({},"root",{240,100}));
        for(int i=0;i<1100;++i){
            auto node=take(b.action_surface(root,"action"+std::to_string(i),{.width=1,.height=1}));
            if(i==0)first=node;
            good(node.set_interaction_handler([&](const auto& value){if(value.alive)++deliveries;}));
        }
        for(int i=0;i<1024;++i){auto p=std::make_shared<gui::WidgetInteractionPort>([](const auto&){});good(first.observe_interaction(p));ports.push_back(std::move(p));}
        auto overflow=std::make_shared<gui::WidgetInteractionPort>([](const auto&){});
        check(!first.observe_interaction(overflow),"External observer bound unexpectedly changed");return {};
    }));
    (void)take(first.snapshot());check(deliveries==1100,"Owned handlers consumed the external observer capacity");state->close();
}
void owner(){
    Fixture f;bool rejected=false;std::thread worker([&]{auto r=f.target.set_interaction_handler([](const auto&){});rejected=!r&&r.error().code==core::ErrorCode::wrong_executor;});worker.join();
    check(rejected,"Interaction handler accepted a non-owner thread");
}
void programmatic_focus(){
    auto state=std::make_shared<gui::detail::WidgetState>([](runtime::Task task){task();});
    auto host=take(state->content_host({},"host",{240,100}));gui::Widget first,second;
    std::vector<gui::WidgetInteraction> first_changes,second_changes,external;
    auto port=std::make_shared<gui::WidgetInteractionPort>([&](const auto& value){external.push_back(value);});
    good(host.set_content([&](gui::WidgetBuilder& b)->core::Result<void>{
        auto root=take(b.absolute_stack({},"root",{240,100}));
        first=take(b.button(root,"first",{.text="First",.height=40,.width=100},{}));
        second=take(b.button(root,"second",{.text="Second",.height=40,.width=100},{}));
        good(first.set_frame({10,10,100,40}));good(second.set_frame({120,10,100,40}));
        good(first.set_interaction_handler([&](const auto& value){first_changes.push_back(value);}));
        good(second.set_interaction_handler([&](const auto& value){second_changes.push_back(value);}));
        return first.observe_interaction(port);
    }));
    (void)take(first.snapshot());
    good(first.focus());
    check(first_changes.back().focused&&external.back().focused,"Programmatic focus was not delivered before returning");
    const auto count=first_changes.size();good(first.focus());
    check(first_changes.size()==count,"Unchanged programmatic focus produced duplicate notifications");
    good(second.focus());
    check(!first_changes.back().focused&&second_changes.back().focused&&!external.back().focused,
          "Programmatic focus transfer did not notify both controls and the observer");
    good(first.set_interaction_handler([&](const auto& value){first_changes.push_back(value);if(value.focused)good(second.focus());}));
    good(first.focus());
    check(!take(first.interaction()).focused&&take(second.interaction()).focused&&!first_changes.back().focused,
          "Reentrant programmatic focus did not settle on the latest target");
    good(first.set_interaction_handler([](const auto& value){if(value.focused)throw std::runtime_error("focus observer failed");}));
    const auto failed=first.focus();
    check(!failed&&failed.error().code==core::ErrorCode::failed&&take(first.interaction()).focused&&external.back().focused,
          "Programmatic focus observer failure lost the committed state or external delivery");
    state->close();
}
}
int main(int argc,char** argv){try{
    const std::string name=argc>1?argv[1]:"committed";
    if(name=="committed")committed();else if(name=="reentrant")reentrant();else if(name=="fanout")fanout();else if(name=="retirement")retirement();else if(name=="capacity")capacity();else if(name=="owner")owner();else if(name=="programmatic_focus")programmatic_focus();else throw std::runtime_error("Unknown case");
    std::cout<<"INTERACTION_HANDLER_OK "<<name<<'\n';
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
