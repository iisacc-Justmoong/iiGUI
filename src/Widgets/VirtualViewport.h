#pragma once
#include <iiSDKCore/Core/Property/Observable.h>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
namespace ii::gui {
template<class Policy>
class BasicVirtualViewport final {
    using VirtualViewportState=typename Policy::VirtualViewportState;using VirtualAnchor=typename Policy::VirtualAnchor;using CollectionReveal=typename Policy::CollectionReveal;
public:
    BasicVirtualViewport()=default;
    BasicVirtualViewport(const BasicVirtualViewport&)=delete;
    BasicVirtualViewport& operator=(const BasicVirtualViewport&)=delete;
    template<class Context> VirtualViewportState read(Context& context){owner();return context.read(state_);}
    VirtualViewportState current()const{owner();return state_.get();}
    VirtualAnchor anchor()const{owner();return anchor_;}
    void reveal(std::string key,CollectionReveal alignment=CollectionReveal::contain){
        owner();if(!active_)return;
        if(static_cast<unsigned>(alignment)>3)throw std::invalid_argument("Invalid collection reveal alignment");
        if(!key.empty())key=Policy::collection_key(key);
        auto next=state_.get();if(next.reveal_key==key&&next.reveal_alignment==alignment)return;
        if(next.movement==UINT64_MAX||next.reveal_serial==UINT64_MAX)throw std::overflow_error("Viewport reveal exhausted");
        next.reveal_key=std::move(key);next.reveal_alignment=alignment;++next.reveal_serial;++next.movement;state_.set(std::move(next));
    }
    void observe(VirtualViewportState value){
        owner();if(!active_)return;
        if(!std::isfinite(value.width)||!std::isfinite(value.height)||value.width<0||value.height<0||value.width>8192||value.height>8192||
           !std::isfinite(value.x)||!std::isfinite(value.y)||value.x<0||value.y<0)
            throw std::invalid_argument("Invalid observed viewport");
        const auto before=state_.get();
        value.movement=before.movement;
        value.reveal_key=before.reveal_key;value.reveal_alignment=before.reveal_alignment;value.reveal_serial=before.reveal_serial;
        if(value.x!=before.x||value.y!=before.y){
            if(value.movement==(std::numeric_limits<std::uint64_t>::max)())throw std::overflow_error("Viewport movement exhausted");
            ++value.movement;
            value.reveal_key.clear();
        }
        state_.set(value);
    }
    template<class Frame> bool commit(const Frame& frame){
        owner();if(!active_)return false;
        if(!std::isfinite(frame.x)||!std::isfinite(frame.y)||frame.x<0||frame.y<0||
           !std::isfinite(frame.anchor.delta)||frame.anchor.delta<0)
            throw std::invalid_argument("Invalid viewport commit");
        auto current=state_.get();
        if(frame.movement!=current.movement||frame.width!=current.width||frame.height!=current.height)return false;
        anchor_=frame.anchor;
        current.x=frame.x;current.y=frame.y;
        if(current.reveal_serial==frame.reveal_serial&&current.reveal_key==frame.revealed_key)current.reveal_key.clear();
        state_.set(current);return true;
    }
    void close(){owner();active_=false;anchor_={};}
private:
    void owner()const{if(owner_!=std::this_thread::get_id())throw std::logic_error("Viewport requires its owner thread");}
    const std::thread::id owner_=std::this_thread::get_id();
    bool active_=true;
    ii::core::Observable<VirtualViewportState> state_{VirtualViewportState{}};
    VirtualAnchor anchor_;
};
} // namespace ii::gui
