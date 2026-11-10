#include "../src/Platform/HostActivity.h"
#include <stdexcept>

namespace {
void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
}

int main() {
    ii::gui::detail::HostActivity activity;
    check(!activity.take(), "An unknown activity state was fabricated");
    activity.observe(true);
    activity.observe(false);
    check(!activity.take(), "Activity was delivered before the host opened");
    activity.open();
    check(activity.take() == false, "The last pre-open state was not delivered");
    check(!activity.take(), "Activity was delivered twice");
    activity.observe(false);
    check(!activity.take(), "An unchanged state was delivered twice");
    activity.observe(true);
    check(activity.take() == true, "An active transition was lost");
    activity.observe(false);
    activity.observe(true);
    check(activity.take() == false && activity.take() == true,
          "Post-open transitions were coalesced or reordered");
    activity.observe(false);
    activity.close();
    activity.open();
    activity.observe(true);
    check(!activity.take(), "A closed host resumed activity delivery");

    ii::gui::detail::HostActivity closing;
    int calls=0;
    closing.set_handler([&](bool value) { check(value, "Unexpected activity value"); ++calls; closing.close(); });
    closing.observe(true);
    closing.open();
    closing.deliver();
    closing.observe(false);
    closing.deliver();
    check(calls==1 && !closing.has_handler(), "Closing from an activity callback did not revoke delivery");

    ii::gui::detail::HostActivity recursive;
    int recursive_calls=0;
    recursive.set_handler([&](bool value) { ++recursive_calls; recursive.observe(!value); });
    recursive.observe(true); recursive.open();
    bool limited=false;
    try { recursive.deliver(); } catch (const std::runtime_error&) { limited=true; }
    check(limited && recursive_calls==64,"Reentrant native activity was not bounded");

    ii::gui::detail::HostActivity overflow;
    overflow.open();
    for (int i=0;i<65;++i) overflow.observe((i%2)==0);
    bool rejected=false;
    try { overflow.deliver(); } catch (const std::runtime_error&) { rejected=true; }
    check(rejected,"Activity queue overflow silently dropped transitions");
}
