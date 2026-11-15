#pragma once
#include <iiSDKCore/Core/Property/Observable.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
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
    // A focus ticket is confined to one input callback. If no layout commits,
    // end_focus restores the previous ordinary anchor.
    [[nodiscard]] std::uint64_t begin_focus(double x,double y){
        owner();
        if constexpr(requires(VirtualAnchor anchor) {anchor.focus;anchor.proportional;}) {
            if(!active_||pending_focus_||!focus_.source||!focus_.key||!std::isfinite(x)||!std::isfinite(y)||
               x<0||y<0||x>=focus_.width||y>=focus_.height||focus_.stride<=0||!focus_.columns||!focus_.count)return 0;
            const auto current=state_.get();
            if(current.width!=focus_.width||current.height!=focus_.height||current.movement!=focus_.movement)return 0;
            const auto position=focus_.horizontal?x+focus_.x_offset:y+focus_.y_offset;
            if(position>=focus_.content_extent)return 0;
            const auto maximum_line=focus_.horizontal?focus_.count-1:(focus_.count-1)/focus_.columns;
            const auto line=std::size_t(std::clamp(std::floor(position/focus_.stride),0.,double(maximum_line)));
            std::size_t column=0;
            if(!focus_.horizontal&&focus_.cell_stride>0)
                column=std::size_t(std::clamp(std::floor(x/focus_.cell_stride),0.,double(focus_.columns-1)));
            const auto index=focus_.horizontal?line:(std::min)(line*focus_.columns+column,focus_.count-1);
            const auto within=std::clamp(position-double(line)*focus_.stride,0.,std::nextafter(focus_.stride,0.));
            VirtualAnchor next{focus_.key(focus_.source.get(),index),index,within/focus_.stride,focus_.movement};
            next.focus=focus_.horizontal?x:y;next.proportional=true;
            if(focus_serial_==(std::numeric_limits<std::uint64_t>::max)())throw std::overflow_error("Viewport focus exhausted");
            auto previous=anchor_;previous_anchor_=std::move(previous);anchor_=std::move(next);
            pending_focus_=++focus_serial_;return pending_focus_;
        }else return 0;
    }
    void end_focus(std::uint64_t ticket){
        owner();if(!active_||!ticket||ticket!=pending_focus_)return;
        anchor_=std::move(previous_anchor_);previous_anchor_={};pending_focus_=0;
    }
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
            if(pending_focus_){anchor_=std::move(previous_anchor_);previous_anchor_={};pending_focus_=0;}
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
        pending_focus_=0;previous_anchor_={};
        focus_={};
        if constexpr(requires(VirtualAnchor anchor) {anchor.focus;anchor.proportional;} &&
                     requires {frame.source;frame.layout;frame.columns;frame.measure_height;}) {
            if(frame.source&&!frame.measure_height&&frame.width>0&&frame.height>0&&frame.columns&&!frame.source->items().empty()){
                const auto stride=(frame.layout.horizontal?frame.layout.item_width:frame.layout.item_height)+frame.layout.spacing;
                if(std::isfinite(stride)&&stride>0){
                    using Source=std::remove_cv_t<typename std::remove_cvref_t<decltype(frame.source)>::element_type>;
                    focus_.source=frame.source;
                    focus_.key=[](const void* source,std::size_t index){return static_cast<const Source*>(source)->items()[index].key;};
                    focus_.count=frame.source->items().size();focus_.columns=frame.columns;
                    focus_.width=frame.width;focus_.height=frame.height;focus_.x_offset=frame.x;focus_.y_offset=frame.y;
                    focus_.stride=stride;focus_.movement=frame.movement;focus_.horizontal=frame.layout.horizontal;
                    focus_.content_extent=frame.layout.horizontal?frame.content_width:frame.content_height;
                    const auto cell_width=(std::max)(0.,(frame.width-frame.layout.spacing*double(frame.columns-1))/double(frame.columns));
                    focus_.cell_stride=cell_width+frame.layout.spacing;
                }
            }
        }
        current.x=frame.x;current.y=frame.y;
        if(current.reveal_serial==frame.reveal_serial&&current.reveal_key==frame.revealed_key)current.reveal_key.clear();
        state_.set(current);return true;
    }
    void close(){owner();active_=false;anchor_={};previous_anchor_={};pending_focus_=0;focus_={};}
private:
    void owner()const{if(owner_!=std::this_thread::get_id())throw std::logic_error("Viewport requires its owner thread");}
    const std::thread::id owner_=std::this_thread::get_id();
    bool active_=true;
    ii::core::Observable<VirtualViewportState> state_{VirtualViewportState{}};
    VirtualAnchor anchor_;
    VirtualAnchor previous_anchor_;
    std::uint64_t focus_serial_=0,pending_focus_=0;
    struct Focus final {
        std::shared_ptr<const void> source;
        std::string (*key)(const void*,std::size_t)=nullptr;
        std::size_t count=0,columns=0;
        double width=0,height=0,x_offset=0,y_offset=0,stride=0,cell_stride=0,content_extent=0;
        std::uint64_t movement=0;
        bool horizontal=false;
    } focus_;
};
} // namespace ii::gui
