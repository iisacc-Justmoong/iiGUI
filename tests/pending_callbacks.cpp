#include "iiGUI/PendingCallbacks.h"
#include <iostream>
struct State {int& live;bool retired=false;explicit State(int& n):live(n){++live;}~State(){--live;}};
int main(){int live=0;{
    ii::gui::detail::PendingCallbacks<State> pending(1);auto state=std::make_shared<State>(live);auto* raw=state.get();
    if(!pending.hold(state)||pending.hold(state)||pending.hold(std::make_shared<State>(live)))return 1;
    state->retired=true;state.reset();if(live!=1)return 2;
    auto callback=pending.take(raw);if(!callback||!callback->retired||pending.take(raw))return 3;callback.reset();if(live)return 4;
    auto next=std::make_shared<State>(live);if(!pending.hold(next))return 5;next.reset();if(live!=1)return 6;
    // No OS callback arrives. End of the owning thread/registry releases it.
    }if(live)return 7;std::cout<<"LATE_CALLBACK_OWNERSHIP_OK\n";
}
