#pragma once
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>
namespace ii::gui::detail {
// Owner-thread, preallocated ownership for one-shot C API callbacks. A late
// callback can safely take its state after its logical owner has retired. The
// registry's thread lifetime also releases states an OS Looper never delivers.
template<class State>class PendingCallbacks final {
    std::vector<std::shared_ptr<State>> slots_;
public:
    explicit PendingCallbacks(std::size_t capacity):slots_(capacity){if(!capacity)throw std::invalid_argument("Callback registry capacity is zero");}
    bool hold(std::shared_ptr<State> state){if(!state)return false;for(const auto& slot:slots_)if(slot.get()==state.get())return false;for(auto& slot:slots_)if(!slot){slot=std::move(state);return true;}return false;}
    std::shared_ptr<State> take(const State* state){for(auto& slot:slots_)if(slot.get()==state)return std::exchange(slot,{});return {};}
};
}
