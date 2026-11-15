#pragma once
#include <iiGUI/NativeWidgets.h>
#include <iiSDKCore/Core/Property/Observable.h>
#include <algorithm>
#include <cmath>
#include <functional>
#include <memory>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>
namespace ii::gui {
template<class Policy> class BasicCollectionWidgetNavigation final:
    public std::enable_shared_from_this<BasicCollectionWidgetNavigation<Policy>> {
    using T=typename Policy::Item;using Navigation=typename Policy::Navigation;
    using VirtualViewport=typename Policy::Viewport;using CollectionNavigationEvents=typename Policy::Events;
    using CollectionSelectionState=typename Policy::Selection;using CollectionModifiers=typename Policy::Modifiers;
    using CollectionNavigationReply=typename Policy::Reply;using CollectionCommand=typename Policy::Command;
    using CollectionNavigationLayout=typename Policy::Layout;
public:
    using Frame=typename Policy::Frame;
    using Admission=std::function<std::shared_ptr<void>()>;
    using Filter=std::function<bool(const T&,std::size_t)>;
    static ii::core::Result<std::shared_ptr<BasicCollectionWidgetNavigation>> create(
        ii::gui::WidgetBuilder& widgets,ii::gui::Widget target,Navigation& navigation,
        std::shared_ptr<VirtualViewport> viewport,Admission admission,
        std::function<void()> publish_revision,CollectionNavigationEvents events={},Filter filter={}){
        if(!viewport||!admission)return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Collection navigation requires a viewport and admission");
        auto value=std::shared_ptr<BasicCollectionWidgetNavigation>(new BasicCollectionWidgetNavigation(target,navigation,std::move(viewport),std::move(admission),std::move(publish_revision),std::move(events),std::move(filter)));
        auto weak=std::weak_ptr(value);
        if(auto retained=widgets.retain(value);!retained)return std::unexpected(retained.error());
        if(auto retiring=widgets.on_retire([weak]{if(auto owner=weak.lock())owner->close();});!retiring)return std::unexpected(retiring.error());
        if(auto input=target.set_input_handler([weak](const ii::gui::WidgetInput& event)->ii::core::Result<bool>{if(auto owner=weak.lock())return owner->input(event);return false;});!input)return std::unexpected(input.error());
        if(auto escape=target.set_escape_action([weak]{if(auto owner=weak.lock())owner->escape();});!escape)return std::unexpected(escape.error());
        value->watch_=navigation.state.observe([weak](const auto& state,std::uint64_t){if(auto owner=weak.lock())owner->changed(state);});
        if(!value->last_.current_key.empty())value->viewport_->reveal(value->last_.current_key);
        return value;
    }
    void publish(const Frame& frame){owner();if(active_)frame_=std::make_shared<const Frame>(frame);}
    void select_on_click(bool enabled){owner();if(active_)select_on_click_=enabled;}
    void activate_on_click(bool enabled){owner();if(active_&&click_enabled_!=enabled){click_enabled_=enabled;reset_click();}}
    void activate_on_double_click(bool enabled){owner();if(active_&&double_click_enabled_!=enabled){double_click_enabled_=enabled;reset_click();}}
    void close(){owner();if(!active_)return;active_=false;watch_.reset();frame_.reset();viewport_.reset();
        reset_click();admission_={};revision_={};events_={};filter_={};navigation_=nullptr;}
private:
    BasicCollectionWidgetNavigation(ii::gui::Widget target,Navigation& navigation,
        std::shared_ptr<VirtualViewport> viewport,Admission admission,std::function<void()> revision,
        CollectionNavigationEvents events,Filter filter):target_(target),navigation_(&navigation),viewport_(std::move(viewport)),
        admission_(std::move(admission)),revision_(std::move(revision)),events_(std::move(events)),filter_(std::move(filter)),last_(navigation.state.get()){}
    void owner()const{if(owner_!=std::this_thread::get_id())throw std::logic_error("Collection widget navigation requires its owner thread");}
    void changed(const CollectionSelectionState& state){
        owner();if(!active_)return;auto admission=admission_;if(!admission)return;auto admitted=admission();if(!admitted||!active_)return;
        const auto snapshot=state;const bool focus=last_.current_key!=snapshot.current_key,selection=last_.selected_keys!=snapshot.selected_keys;last_=snapshot;
        auto viewport=viewport_;if(focus&&viewport)viewport->reveal(snapshot.current_key);
        if(!active_||!admission()||!active_)return;
        if(focus||selection){auto revision=revision_;if(revision)revision();}
        if(!active_||!admission()||!active_)return;
        if(focus){auto callback=events_.current_changed;if(callback)callback(snapshot.current_key);}
        if(active_&&admission()&&active_&&selection){auto callback=events_.selection_changed;if(callback)callback(snapshot.selected_keys);}
    }
    void reset_click(){last_click_key_.clear();pending_click_key_.clear();last_click_time_=0;last_click_released_=false;}
    ii::core::Result<bool> input(const ii::gui::WidgetInput& event){
        owner();if(!active_)return false;auto admission=admission_;if(!admission)return false;auto admitted=admission();if(!admitted||!active_||!frame_||!frame_->source)return false;
        if(event.kind==ii::gui::WidgetInputKind::pointer_cancel||event.kind==ii::gui::WidgetInputKind::focus_lost||event.kind==ii::gui::WidgetInputKind::key_cancel){reset_click();return false;}
        if(event.kind==ii::gui::WidgetInputKind::key_down&&event.key==ii::gui::WidgetKey::escape)escape_allowed_=!event.text_input_active&&!event.native_navigation_active;
        auto frame=frame_;
        const auto filter=filter_;const auto eligible=[filter](const T& value,std::size_t index){return !filter||filter(value,index);};
        const auto modifiers=CollectionModifiers{.range=event.shift,.additive=event.control||event.meta};
        CollectionNavigationReply reply;
        if(event.kind==ii::gui::WidgetInputKind::pointer_down){
            if(event.buttons&~1u){reset_click();return false;}
            auto snapshot=target_.snapshot();if(!snapshot)return std::unexpected(snapshot.error());
            // The collection's own input handler marks its target as a control.
            // Only a distinct nested control should suppress row selection.
            const std::string_view id=snapshot->id,target=event.target;
            const bool self=id==target||(!target.empty()&&id.size()>target.size()&&id.ends_with(target)&&id[id.size()-target.size()-1]=='/');
            if(event.native_control_active&&!self){reset_click();return false;}
            if(!active_||!admission()||!active_||!frame_)return true;frame=frame_;
            // Snapshot bounds are in window coordinates for embedded views,
            // while dispatched event.x/y belong to this content host. The
            // handler-local position is stable at every nesting depth.
            const auto x=event.local_position?event.local_position->x:event.x-snapshot->bounds.x;
            const auto y=event.local_position?event.local_position->y:event.y-snapshot->bounds.y;
            std::string clicked;
            for(std::size_t n=0;n<frame->cells.size();++n){const auto& cell=frame->cells[n];
                if(cell.width<=0||cell.height<=0||x<cell.x||y<cell.y||x>=cell.x+cell.width||y>=cell.y+cell.height)continue;
                const bool select=select_on_click_||modifiers.additive||modifiers.range;
                clicked=frame->collection.items[n].key;
                reply=navigation_->focus_at(*frame->source,frame->collection.items[n].index,modifiers,select,eligible);break;
            }
            if(!active_||!admission()||!active_)return reply.handled;
            if(!reply.handled||clicked.empty()||navigation_->state.get().current_key!=clicked){reset_click();}
            else if(click_enabled_){
                reset_click();pending_click_key_=clicked;pending_click_x_=event.x;pending_click_y_=event.y;
                last_click_device_=event.device;last_click_device_id_=event.device_id;
            }
            else if(!double_click_enabled_||modifiers.range||modifiers.additive||event.alt){reset_click();}
            else{
                const bool same_device=last_click_device_==event.device&&(!last_click_device_id_||!event.device_id||last_click_device_id_==event.device_id);
                const bool same=last_click_released_&&last_click_key_==clicked&&same_device&&std::hypot(event.x-last_click_x_,event.y-last_click_y_)<=4;
                const bool timed=event.native_click_count==0&&same&&std::isfinite(event.timestamp)&&last_click_time_>0&&
                    event.timestamp>last_click_time_&&event.timestamp-last_click_time_<=.4;
                const bool doubled=same&&(event.native_click_count==2||timed);
                pending_click_key_=doubled?clicked:std::string{};pending_click_x_=event.x;pending_click_y_=event.y;
                last_click_key_=doubled||event.native_click_count>2?std::string{}:clicked;
                last_click_time_=std::isfinite(event.timestamp)&&event.timestamp>0?event.timestamp:0;
                last_click_x_=event.x;last_click_y_=event.y;last_click_device_=event.device;last_click_device_id_=event.device_id;
                last_click_released_=false;
            }
        }else if(event.kind==ii::gui::WidgetInputKind::pointer_up){
            const bool pending=!pending_click_key_.empty();
            auto key=pending?pending_click_key_:last_click_key_;
            if(key.empty())return false;
            const bool same_device=last_click_device_==event.device&&(!last_click_device_id_||!event.device_id||last_click_device_id_==event.device_id);
            const auto down_x=pending?pending_click_x_:last_click_x_,down_y=pending?pending_click_y_:last_click_y_;
            if(!same_device||(!click_enabled_&&(event.alt||modifiers.range||modifiers.additive))||
               std::hypot(event.x-down_x,event.y-down_y)>4||navigation_->state.get().current_key!=key){reset_click();return false;}
            auto snapshot=target_.snapshot();if(!snapshot)return std::unexpected(snapshot.error());
            const std::string_view id=snapshot->id,target=event.target;
            const bool self=id==target||(!target.empty()&&id.size()>target.size()&&id.ends_with(target)&&id[id.size()-target.size()-1]=='/');
            if(event.native_control_active&&!self){reset_click();return false;}
            const auto x=event.local_position?event.local_position->x:event.x-snapshot->bounds.x;
            const auto y=event.local_position?event.local_position->y:event.y-snapshot->bounds.y;
            bool inside=false;
            for(std::size_t n=0;n<frame->cells.size();++n){const auto& cell=frame->cells[n];
                if(frame->collection.items[n].key==key&&cell.width>0&&cell.height>0&&x>=cell.x&&y>=cell.y&&x<cell.x+cell.width&&y<cell.y+cell.height){inside=true;break;}}
            if(!inside||!active_||!admission()||!active_){reset_click();return false;}
            if(!pending){last_click_released_=true;return false;}
            reset_click();
            auto callback=events_.activated;if(callback)callback(key);return true;
        }else if(event.kind==ii::gui::WidgetInputKind::key_down){
            if(event.text_input_active||event.native_navigation_active||event.alt)return false;
            using Key=ii::gui::WidgetKey;using Command=CollectionCommand;
            std::optional<Command> command;
            switch(event.key){
            case Key::left:command=Command::left;break;case Key::right:command=Command::right;break;
            case Key::up:command=Command::up;break;case Key::down:command=Command::down;break;
            case Key::home:command=Command::home;break;case Key::end:command=Command::end;break;
            case Key::page_up:command=Command::page_previous;break;case Key::page_down:command=Command::page_next;break;
            case Key::select_all:if(event.control||event.meta)command=Command::select_all;break;
            case Key::space:if(!event.native_control_active)command=Command::toggle;break;
            case Key::enter:if(!event.native_control_active)command=Command::activate;break;
            case Key::escape:if(!navigation_->state.get().selected_keys.empty())command=Command::clear;break;
            default:break;
            }
            if(!command)return false;
            std::set<std::size_t> lines;
            for(std::size_t n=0;n<frame->cells.size();++n){const auto& cell=frame->cells[n];
                const bool visible=frame->layout.horizontal?cell.x+cell.width>0&&cell.x<frame->width:cell.y+cell.height>0&&cell.y<frame->height;
                if(visible)lines.insert(frame->layout.horizontal?frame->collection.items[n].index:frame->collection.items[n].index/frame->columns);
            }
            CollectionNavigationLayout layout{.columns=frame->layout.horizontal?1:frame->columns,.page_rows=std::max<std::size_t>(1,lines.size()),.horizontal=frame->layout.horizontal};
            if(event.native_control_active)for(const auto& item:frame->collection.items){
                auto row=target_.keyed_content_item(item.key);if(!row)continue;auto interaction=row->interaction();
                if(interaction&&interaction->focused){layout.current_index=item.index;break;}
            }
            reply=navigation_->command(*frame->source,*command,layout,modifiers,eligible);
        }else return false;
        if(!reply.handled)return false;
        if(!active_||!admission()||!active_)return true;
        const auto key=navigation_->state.get().current_key;
        auto viewport=viewport_;if(!reply.focus_changed&&!key.empty()&&viewport)viewport->reveal(key);
        if(!active_)return true;
        if(auto focused=target_.focus();!focused)return std::unexpected(focused.error());
        if(active_&&admission()&&active_&&reply.activated){auto callback=events_.activated;if(callback)callback(key);}
        return true;
    }
    void escape(){
        owner();if(!active_||!escape_allowed_||!frame_||!frame_->source)return;
        auto admission=admission_;if(!admission)return;auto admitted=admission();if(!admitted||!active_||navigation_->state.get().selected_keys.empty())return;
        auto frame=frame_;(void)navigation_->command(*frame->source,CollectionCommand::clear);
    }
    const std::thread::id owner_=std::this_thread::get_id();
    bool active_=true,select_on_click_=true,escape_allowed_=true;
    bool click_enabled_=false,double_click_enabled_=false,last_click_released_=false;
    std::string last_click_key_,pending_click_key_;
    double last_click_time_=0,last_click_x_=0,last_click_y_=0,pending_click_x_=0,pending_click_y_=0;
    ii::gui::PointerDevice last_click_device_=ii::gui::PointerDevice::mouse;
    std::uint64_t last_click_device_id_=0;
    ii::gui::Widget target_;
    Navigation* navigation_;
    std::shared_ptr<VirtualViewport> viewport_;
    Admission admission_;
    std::function<void()> revision_;
    CollectionNavigationEvents events_;
    Filter filter_;
    CollectionSelectionState last_;
    ii::core::ObservableSubscription watch_;
    std::shared_ptr<const Frame> frame_;
};
} // namespace ii::gui
