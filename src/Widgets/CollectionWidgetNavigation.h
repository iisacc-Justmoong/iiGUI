#pragma once
#include <iiGUI/NativeWidgets.h>
#include <iiSDKCore/Core/Property/Observable.h>
#include <algorithm>
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
    void close(){owner();if(!active_)return;active_=false;watch_.reset();frame_.reset();viewport_.reset();
        admission_={};revision_={};events_={};filter_={};navigation_=nullptr;}
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
    ii::core::Result<bool> input(const ii::gui::WidgetInput& event){
        owner();if(!active_)return false;auto admission=admission_;if(!admission)return false;auto admitted=admission();if(!admitted||!active_||!frame_||!frame_->source)return false;
        if(event.kind==ii::gui::WidgetInputKind::key_down&&event.key==ii::gui::WidgetKey::escape)escape_allowed_=!event.text_input_active&&!event.native_navigation_active;
        auto frame=frame_;
        const auto filter=filter_;const auto eligible=[filter](const T& value,std::size_t index){return !filter||filter(value,index);};
        const auto modifiers=CollectionModifiers{.range=event.shift,.additive=event.control||event.meta};
        CollectionNavigationReply reply;
        if(event.kind==ii::gui::WidgetInputKind::pointer_down){
            if(event.buttons&~1u)return false;
            auto snapshot=target_.snapshot();if(!snapshot)return std::unexpected(snapshot.error());
            // The collection's own input handler marks its target as a control.
            // Only a distinct nested control should suppress row selection.
            const std::string_view id=snapshot->id,target=event.target;
            const bool self=id==target||(!target.empty()&&id.size()>target.size()&&id.ends_with(target)&&id[id.size()-target.size()-1]=='/');
            if(event.native_control_active&&!self)return false;
            if(!active_||!admission()||!active_||!frame_)return true;frame=frame_;
            const auto x=event.x-snapshot->bounds.x,y=event.y-snapshot->bounds.y;
            for(std::size_t n=0;n<frame->cells.size();++n){const auto& cell=frame->cells[n];
                if(cell.width<=0||cell.height<=0||x<cell.x||y<cell.y||x>=cell.x+cell.width||y>=cell.y+cell.height)continue;
                const bool select=select_on_click_||modifiers.additive||modifiers.range;
                reply=navigation_->focus_at(*frame->source,frame->collection.items[n].index,modifiers,select,eligible);break;
            }
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
