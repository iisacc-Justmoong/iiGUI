#include <iiGUI/Motion.h>
#include <chrono>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <thread>
#include <vector>
using namespace ii;
using namespace std::chrono_literals;
namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(core::Result<T> value){if(!value)throw std::runtime_error(value.error().message);return std::move(*value);}
void good(core::Result<void> value){if(!value)throw std::runtime_error(value.error().message);}
void preferences(){
    gui::MotionPreferences model;
    auto initial=take(model.state());check(initial.enabled&&!initial.reduced_motion&&initial.animated()&&initial.speed==1,"Motion defaults differ");
    check(take(model.duration(420ms))==420ms,"Default duration differs");
    good(model.set_speed(3));check(take(model.duration(420ms))==140ms,"Speed division differs");
    good(model.set_speed(.5));check(take(model.duration(420ms))==840ms,"Slow duration differs");
    good(model.set_speed(0));check(take(model.duration(420ms))==4200ms,"Lower factor clamp differs");
    good(model.set_speed(10));check(take(model.duration(420ms))==105ms,"Upper factor clamp differs");
    good(model.set_speed(std::numeric_limits<double>::quiet_NaN()));check(take(model.duration(420ms))==420ms,"Nonfinite factor fallback differs");
    good(model.set_reduced_motion(true));check(!take(model.state()).animated()&&take(model.duration(420ms))==0ms,"Reduced motion did not settle");
    good(model.set_reduced_motion(false));good(model.set_enabled(false));check(take(model.duration(420ms))==0ms,"Disabled motion did not settle");
    good(model.set_enabled(true));good(model.set_speed(.1));check(take(model.duration(runtime::Clock::duration::max()))==runtime::Clock::duration::max(),"Duration overflow was not saturated");
    check(take(model.duration(-10ms))==0ms,"Negative duration was not clamped");
    bool rejected=false;std::thread worker([&]{auto result=model.state();rejected=!result&&result.error().code==core::ErrorCode::wrong_executor;});worker.join();check(rejected,"Foreign thread read owning motion state");
}
void observers(){
    gui::MotionPreferences model;int notifications=0;bool busy=false;
    auto receiver=std::make_shared<gui::MotionChangePort>([&](const gui::MotionState& state){++notifications;check(state.revision==2&&!state.enabled,"Query saw uncommitted preference state");auto result=model.set_speed(2);busy=!result&&result.error().code==core::ErrorCode::busy;});
    good(model.observe(receiver));good(model.observe(receiver));good(model.set_enabled(false));
    check(notifications==1&&busy,"Idempotent observation or mutation barrier differs");
    good(model.set_enabled(false));check(notifications==1,"No-op edit notified receivers");
    receiver.reset();good(model.set_enabled(true));check(notifications==1,"Expired receiver survived");
    auto bad=std::make_shared<gui::MotionChangePort>([](const gui::MotionState&){throw std::runtime_error("observer failure");});good(model.observe(bad));
    auto result=model.set_reduced_motion(true);check(!result&&result.error().cause&&take(model.state()).reduced_motion,"Notification failure lost committed preference change");
}
void capacity(){
    gui::MotionPreferences model;std::vector<std::shared_ptr<gui::MotionChangePort>> receivers;
    for(int i=0;i<256;++i){auto p=std::make_shared<gui::MotionChangePort>([](const auto&){});good(model.observe(p));receivers.push_back(std::move(p));}
    auto extra=std::make_shared<gui::MotionChangePort>([](const auto&){});check(!model.observe(extra),"Observation capacity was not bounded");
    for(auto& p:receivers)check(bool(p->close()),"Receiver close failed");
    good(model.set_speed(2));good(model.observe(extra));good(model.set_enabled(false));
}
}
int main(int argc,char** argv){try{const std::string name=argc>1?argv[1]:"preferences";if(name=="preferences")preferences();else if(name=="observers")observers();else if(name=="capacity")capacity();else throw std::runtime_error("Unknown motion preference case");std::cout<<"MOTION_PREFERENCES_OK "<<name<<'\n';}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
