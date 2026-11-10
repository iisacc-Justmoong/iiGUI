#pragma once
#include <iiGUI/NativeWidgets.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

namespace ii::gui::detail {
// Android's pointer indices are transient; (device,id) identifies a contact.
// WidgetState owns one capture. A two-finger gesture first cancels that capture;
// a remaining finger is never promoted to a new press.
enum class AndroidPointerAction { down, up, secondary_down, secondary_up, move, buttons, cancel,
                                  hover_enter, hover_move, hover_exit, scroll };
enum class AndroidPointerTool { touch, mouse, pen, eraser, palm };
struct AndroidPointerSample final {
    std::int32_t id=0;
    AndroidPointerTool tool=AndroidPointerTool::touch;
    double x=0,y=0,pressure=1,tilt=0,orientation=0;
};
struct AndroidPointerFrame final {
    AndroidPointerAction action=AndroidPointerAction::move;
    std::int32_t device=0;
    std::size_t index=0;
    double scale=1,time=0;
    std::uint32_t buttons=0;
    bool shift=false,control=false,alt=false,meta=false,canceled=false;
    double scroll_x=0,scroll_y=0;
    std::vector<AndroidPointerSample> samples;
};
struct AndroidPointerResult final {bool handled=true;std::vector<WidgetInput> events;};
class AndroidPointerState final {
    struct Capture final {std::int32_t device,id;PointerDevice tool;};
    struct Gesture final {
        std::int32_t device,first,second;
        double start_x,start_y,start_distance,last_x,last_y,last_distance;
        bool active=false;
    };
    std::optional<Capture> captured_;
    std::optional<Gesture> gesture_;
    std::optional<std::int32_t> suppressed_touch_;
    std::uint64_t revision_=0;
    static std::uint64_t identity(std::int32_t device,std::int32_t id)noexcept{
        return (std::uint64_t(std::uint32_t(device))<<32)|std::uint32_t(id);
    }
    static PointerDevice tool(AndroidPointerTool value)noexcept{
        switch(value){
        case AndroidPointerTool::mouse:return PointerDevice::mouse;
        case AndroidPointerTool::pen:return PointerDevice::pen;
        case AndroidPointerTool::eraser:return PointerDevice::eraser;
        default:return PointerDevice::touch;
        }
    }
    static WidgetInput convert(const AndroidPointerFrame& frame,const AndroidPointerSample& sample,
                               WidgetInputKind kind){
        if(!std::isfinite(frame.scale)||frame.scale<=0||!std::isfinite(sample.x)||!std::isfinite(sample.y))
            throw std::invalid_argument("Invalid Android pointer coordinates or scale");
        WidgetInput result;result.kind=kind;result.device=tool(sample.tool);
        result.device_id=identity(frame.device,sample.id);result.x=sample.x/frame.scale;result.y=sample.y/frame.scale;
        if(!std::isfinite(result.x)||!std::isfinite(result.y))throw std::invalid_argument("Android pointer coordinates overflow");
        result.timestamp=std::isfinite(frame.time)?std::max(0.,frame.time):0;
        result.shift=frame.shift;result.control=frame.control;result.alt=frame.alt;result.meta=frame.meta;
        result.pressure=std::isfinite(sample.pressure)?std::clamp(sample.pressure,0.,1.):0;
        result.buttons=frame.buttons&0x7f;
        const bool contact=kind==WidgetInputKind::pointer_down||kind==WidgetInputKind::pointer_move;
        if(contact&&result.device!=PointerDevice::mouse)result.buttons|=1;
        if(kind==WidgetInputKind::pointer_up)result.buttons&=~std::uint32_t{1};
        if((result.device==PointerDevice::pen||result.device==PointerDevice::eraser)&&
           std::isfinite(sample.tilt)&&std::isfinite(sample.orientation)){
            const auto angle=std::clamp(sample.tilt,0.,std::numbers::pi/2);
            const auto vertical=std::cos(angle),horizontal=std::sin(angle);
            result.tilt_x=std::atan2(horizontal*std::sin(sample.orientation),vertical)*180/std::numbers::pi;
            result.tilt_y=std::atan2(-horizontal*std::cos(sample.orientation),vertical)*180/std::numbers::pi;
        }
        if(kind==WidgetInputKind::scroll){
            // Same logical-pixel wheel step as the existing desktop hosts.
            result.scroll_x=std::isfinite(frame.scroll_x)?frame.scroll_x*48:0;
            result.scroll_y=std::isfinite(frame.scroll_y)?frame.scroll_y*48:0;
            if(!std::isfinite(result.scroll_x)||!std::isfinite(result.scroll_y))
                throw std::invalid_argument("Android wheel delta overflow");
        }
        return result;
    }
    std::optional<WidgetInput> cancel_contact()noexcept{
        if(!captured_)return {};
        WidgetInput value;value.kind=WidgetInputKind::pointer_cancel;value.device=captured_->tool;
        value.device_id=identity(captured_->device,captured_->id);captured_.reset();++revision_;return value;
    }
public:
    [[nodiscard]] std::uint64_t revision()const noexcept{return revision_;}
    std::optional<WidgetInput> cancel()noexcept{
        if(!captured_&&!gesture_&&!suppressed_touch_)return {};
        WidgetInput value;value.kind=WidgetInputKind::focus_lost;
        if(captured_){value.device=captured_->tool;value.device_id=identity(captured_->device,captured_->id);}
        captured_.reset();gesture_.reset();suppressed_touch_.reset();++revision_;return value;
    }
    AndroidPointerResult translate(const AndroidPointerFrame& frame){
        AndroidPointerResult result;result.events.reserve(3);
        const auto indexed=[&]() -> const AndroidPointerSample& {
            if(frame.index>=frame.samples.size())throw std::invalid_argument("Invalid Android pointer index");
            return frame.samples[frame.index];
        };
        const auto append_cancel=[&]{if(auto value=cancel_contact())result.events.push_back(std::move(*value));};
        const auto stylus=[](const AndroidPointerSample& point){return point.tool==AndroidPointerTool::pen||point.tool==AndroidPointerTool::eraser;};
        if(frame.action==AndroidPointerAction::cancel){
            if(captured_&&captured_->device==frame.device)append_cancel();
            if(gesture_&&gesture_->device==frame.device){gesture_.reset();++revision_;}
            if(suppressed_touch_==frame.device){suppressed_touch_.reset();++revision_;}return result;
        }
        if(frame.action==AndroidPointerAction::secondary_down){
            const auto& point=indexed();
            if(point.tool==AndroidPointerTool::touch&&gesture_&&gesture_->device==frame.device&&
               point.id!=gesture_->first&&point.id!=gesture_->second){
                append_cancel();gesture_.reset();suppressed_touch_=frame.device;++revision_;return result;
            }
            if(stylus(point)&&(!captured_||(captured_->device==frame.device&&captured_->tool==PointerDevice::touch))){
                auto input=convert(frame,point,WidgetInputKind::pointer_down);append_cancel();
                if(gesture_){suppressed_touch_=gesture_->device;gesture_.reset();++revision_;}
                captured_=Capture{frame.device,point.id,input.device};++revision_;result.events.push_back(std::move(input));
            }else if(point.tool==AndroidPointerTool::touch&&captured_&&captured_->device==frame.device&&captured_->tool==PointerDevice::touch){
                if(gesture_){append_cancel();gesture_.reset();suppressed_touch_=frame.device;++revision_;return result;}
                const auto first=std::ranges::find_if(frame.samples,[&](const auto& item){return item.id==captured_->id&&item.tool==AndroidPointerTool::touch;});
                if(first==frame.samples.end()||point.id==first->id)return result;
                const auto a=convert(frame,*first,WidgetInputKind::pointer_move),b=convert(frame,point,WidgetInputKind::pointer_move);
                const auto x=(a.x+b.x)/2,y=(a.y+b.y)/2,d=std::hypot(a.x-b.x,a.y-b.y);
                if(!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(d))throw std::invalid_argument("Invalid Android gesture geometry");
                gesture_=Gesture{frame.device,first->id,point.id,x,y,d,x,y,d,false};++revision_;
            }return result;
        }
        if(frame.action==AndroidPointerAction::down){
            const auto& point=indexed();
            if(point.tool==AndroidPointerTool::palm)return result;
            if(suppressed_touch_==frame.device)suppressed_touch_.reset();
            if(captured_&&captured_->device!=frame.device&&!(captured_->tool==PointerDevice::touch&&stylus(point)))return result;
            if(gesture_&&(gesture_->device==frame.device||stylus(point))){
                if(stylus(point))suppressed_touch_=gesture_->device;
                gesture_.reset();++revision_;
            }
            const bool context=point.tool==AndroidPointerTool::mouse&&(frame.buttons&2)&&!(frame.buttons&1);
            auto input=convert(frame,point,context?WidgetInputKind::context_request:WidgetInputKind::pointer_down);
            if(captured_)append_cancel();
            if(context){result.events.push_back(std::move(input));return result;}
            captured_=Capture{frame.device,point.id,input.device};++revision_;result.events.push_back(std::move(input));return result;
        }
        if(frame.action==AndroidPointerAction::up||frame.action==AndroidPointerAction::secondary_up){
            const auto& point=indexed();
            if(gesture_&&gesture_->device==frame.device&&(point.id==gesture_->first||point.id==gesture_->second)){
                const bool active=gesture_->active,primary=point.id==gesture_->first;gesture_.reset();++revision_;
                if(active){suppressed_touch_=frame.device;return result;}
                if(primary){append_cancel();suppressed_touch_=frame.device;return result;}
            }
            if(captured_&&captured_->device==frame.device&&captured_->id==point.id){
                if(frame.canceled||point.tool==AndroidPointerTool::palm)append_cancel();
                else {auto input=convert(frame,point,WidgetInputKind::pointer_up);captured_.reset();++revision_;result.events.push_back(std::move(input));}
            }
            if(frame.action==AndroidPointerAction::up&&suppressed_touch_==frame.device){suppressed_touch_.reset();++revision_;}
            return result;
        }
        if(frame.action==AndroidPointerAction::move||(frame.action==AndroidPointerAction::buttons&&captured_)){
            if(gesture_&&gesture_->device==frame.device&&frame.action==AndroidPointerAction::move){
                auto first=std::ranges::find_if(frame.samples,[&](const auto& item){return item.id==gesture_->first&&item.tool==AndroidPointerTool::touch;});
                auto second=std::ranges::find_if(frame.samples,[&](const auto& item){return item.id==gesture_->second&&item.tool==AndroidPointerTool::touch;});
                if(first==frame.samples.end()||second==frame.samples.end()){
                    append_cancel();gesture_.reset();suppressed_touch_=frame.device;++revision_;return result;
                }
                const auto a=convert(frame,*first,WidgetInputKind::pointer_move),b=convert(frame,*second,WidgetInputKind::pointer_move);
                const auto x=(a.x+b.x)/2,y=(a.y+b.y)/2,d=std::hypot(a.x-b.x,a.y-b.y);
                if(!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(d))throw std::invalid_argument("Invalid Android gesture geometry");
                auto& g=*gesture_;
                if(!g.active&&(std::abs(d-g.start_distance)>=4||std::hypot(x-g.start_x,y-g.start_y)>=4)){
                    g.active=true;append_cancel();++revision_;
                }
                if(g.active){
                    auto make=[&]{WidgetInput input;input.kind=WidgetInputKind::scroll;input.device=PointerDevice::touch;
                        input.x=x;input.y=y;input.timestamp=std::isfinite(frame.time)?std::max(0.,frame.time):0;
                        input.precise_scroll=true;input.device_id=identity(frame.device,g.first);return input;};
                    const auto dx=x-g.last_x,dy=y-g.last_y;
                    if(dx!=0||dy!=0){auto input=make();input.scroll_x=dx;input.scroll_y=dy;result.events.push_back(std::move(input));}
                    if(g.last_distance>0&&d>0&&d!=g.last_distance){auto input=make();input.control=true;input.scroll_y=(std::log(d)-std::log(g.last_distance))*pinch_scroll_units;
                        if(!std::isfinite(input.scroll_y))throw std::invalid_argument("Invalid Android pinch scale");result.events.push_back(std::move(input));}
                    g.last_x=x;g.last_y=y;g.last_distance=d;return result;
                }
            }
            if(!captured_||captured_->device!=frame.device)return result;
            const auto point=std::ranges::find_if(frame.samples,[&](const auto& s){return s.id==captured_->id;});
            if(point==frame.samples.end()||point->tool==AndroidPointerTool::palm){append_cancel();return result;}
            result.events.push_back(convert(frame,*point,WidgetInputKind::pointer_move));return result;
        }
        if(captured_)return result;
        const auto& point=indexed();
        if(point.tool==AndroidPointerTool::palm)return result;
        const auto kind=frame.action==AndroidPointerAction::hover_exit?WidgetInputKind::pointer_leave:
                        frame.action==AndroidPointerAction::scroll?WidgetInputKind::scroll:WidgetInputKind::pointer_move;
        auto input=convert(frame,point,kind);
        // Hover is not contact, including a hovering stylus.
        if(kind==WidgetInputKind::pointer_move)input.buttons=frame.buttons&0x7f;
        result.events.push_back(std::move(input));return result;
    }
};
// A callback can retire a surface or reenter the native event loop. A later
// event from the same translation must not revive a superseded capture.
template<class Deliver>
bool dispatch_android_pointer(AndroidPointerState& state,AndroidPointerResult result,Deliver&& deliver){
    const auto revision=state.revision();
    for(auto& event:result.events)
        if(!deliver(std::move(event))||state.revision()!=revision)return false;
    return true;
}
} // namespace ii::gui::detail
