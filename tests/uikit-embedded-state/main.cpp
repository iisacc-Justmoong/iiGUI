#include "../../src/Platform/UIKitEmbeddedState.h"
#include "../../src/Platform/ExternalCloseBarrier.h"
#include <limits>
#include <stdexcept>

using ii::gui::detail::UIKitEmbeddedMetrics;
using ii::gui::detail::UIKitEmbeddedState;
using ii::gui::detail::UIKitPickerAdmission;
void check(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
int main(){
    UIKitEmbeddedState state;
    check(state.pickerAdmission(false,false,false)==UIKitPickerAdmission::unavailable,
          "A picker was admitted before attaching a parent");
    UIKitEmbeddedMetrics initial{.width=320,.height=640,.scale=2,.attached=true,.sceneActive=true,.applicationActive=true};
    auto disconnected=initial;disconnected.attached=false;
    check(!state.attach(disconnected),"A detached parent was accepted");
    auto invalid=initial;invalid.scale=0;
    check(!state.attach(invalid),"An invalid display scale was accepted");
    check(state.attach(initial),"A valid parent was rejected");
    check(state.pickerAdmission(false,false,false)==UIKitPickerAdmission::unavailable,
          "A picker was admitted without an attached owner controller");
    check(state.pickerAdmission(true,true,false)==UIKitPickerAdmission::busy,
          "A picker was admitted over another owner's modal");
    check(state.pickerAdmission(true,false,true)==UIKitPickerAdmission::busy,
          "A picker was admitted during an owner transition");
    check(state.pickerAdmission(true,false,false)==UIKitPickerAdmission::ready,
          "An active attached owner did not admit a picker");
    check(!state.attach(initial),"The same embedded host attached twice");
    auto same=state.update(initial);
    check(same&&!same->geometry&&!same->activity&&!same->detached,"An unchanged event requested work");
    auto resized=initial;resized.width=375;
    auto geometry=state.update(resized);
    check(geometry&&geometry->geometry&&!geometry->activity,"A changed size was lost");
    resized.scale=3;
    geometry=state.update(resized);
    check(geometry&&geometry->geometry&&!geometry->activity,"A display scale change was lost");
    auto bad=resized;bad.width=(std::numeric_limits<double>::quiet_NaN)();
    check(!state.update(bad),"Invalid geometry was accepted");
    same=state.update(resized);
    check(same&&!same->geometry,"Invalid geometry changed the stored bounds");
    auto background=resized;background.applicationActive=false;
    auto activity=state.update(background);
    check(activity&&activity->activity&&!activity->active&&!activity->geometry,"Background did not suspend the surface");
    check(state.pickerAdmission(true,false,false)==UIKitPickerAdmission::busy,
          "A background surface admitted a picker");
    background.sceneActive=false;background.applicationActive=true;
    same=state.update(background);
    check(same&&!same->activity,"An inactive scene resumed from an app-only event");
    background.sceneActive=true;
    activity=state.update(background);
    check(activity&&activity->activity&&activity->active,"An active scene did not resume");
    auto detached=background;detached.attached=false;
    auto retired=state.update(detached);
    check(retired&&retired->detached&&!state.live(),"A detached parent did not retire the surface");
    check(state.pickerAdmission(true,false,false)==UIKitPickerAdmission::unavailable,
          "A retired surface admitted a picker");
    check(!state.close()&&!state.update(initial),"A retired surface admitted more work");
    UIKitEmbeddedState explicitClose;
    check(explicitClose.attach(initial)&&explicitClose.close()&&!explicitClose.close(),"Explicit close was not idempotent");
    UIKitEmbeddedState temporarilyEmpty;
    auto empty=initial;empty.width=0;empty.height=0;
    check(temporarilyEmpty.attach(empty),"A temporarily empty parent was rejected");
    auto restored=temporarilyEmpty.update(initial);
    check(restored&&restored->geometry,"A restored parent size was lost");

    // An outer host callback may enter a foreign nested run loop. A close
    // completion reached in that loop must wait for the outer callback.
    ii::gui::detail::ExternalCloseBarrier barrier;
    barrier.enter();barrier.enter();
    check(!barrier.admit(),"Close completed inside a nested callback");
    check(!barrier.admit(),"A repeated nested close completed before the public call returned");
    check(!barrier.leave(),"The inner callback rearmed close");
    check(barrier.leave(),"The outer callback lost the close request");
    check(barrier.admit()&&!barrier.leave(),"Close was not ready exactly once");
}
