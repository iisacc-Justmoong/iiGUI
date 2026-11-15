#pragma once
#include <iiGUI/NativeWidgets.h>
#include <iiGUI/SrgbColor.h>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <ranges>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>
namespace ii::gui {
template<class Policy>
class BasicNativeTableHeader final {
    using TableHeaderInput=typename Policy::Input;
    using TableHeader=typename Policy::Model;
    using TableViewportFrame=typename Policy::Frame;
    using NativeTableHeaderPresentation=typename Policy::Presentation;
    using NativeTableViewGroup=typename Policy::ViewGroup;
    using NativeTableViews=typename Policy::Views;
    using TableViewItem=typename Policy::ViewItem;
    using TableViewSnapshot=typename Policy::ViewSnapshot;
    using NativeTableHeaderCallbacks=typename Policy::Callbacks;
    using TableSortOrder=typename Policy::SortOrder;
    struct Prepared final {
        TableHeaderInput authored;TableHeader model;TableViewportFrame frame;
        typename NativeTableHeaderPresentation::Snapshot presentation;
    };
    using Snapshot=std::shared_ptr<const Prepared>;
    struct State final {
        Snapshot prepared;
        std::string id;
        ii::gui::Widget root;
        ii::gui::Widget view_stack;
        NativeTableHeaderCallbacks callbacks;
        std::unique_ptr<NativeTableViewGroup> views;
        std::optional<std::string> focused,pressed;
        std::optional<ii::core::Error> failure;
        const std::thread::id owner=std::this_thread::get_id();
        std::uint64_t revision=0;
        unsigned updates=0;
        bool live=false,retired=false;
    };
    static void release(const std::shared_ptr<State>& state)noexcept {
        state->prepared.reset();state->views.reset();state->callbacks={};state->focused.reset();state->pressed.reset();
    }
    struct UpdateGuard final {
        std::shared_ptr<State> state;
        explicit UpdateGuard(std::shared_ptr<State> value):state(std::move(value)){++state->updates;}
        ~UpdateGuard(){if(--state->updates==0&&state->retired)release(state);}
    };
    explicit BasicNativeTableHeader(std::shared_ptr<State> state):state_(std::move(state)){}
    [[nodiscard]] static ii::core::Result<Snapshot> prepare(const TableHeaderInput& input,double width,std::optional<double> offset={},Snapshot previous={}) {
        try {
            if(!Policy::valid_text(input.text)||input.icon_revision<0||input.icon_revision>9007199254740991LL)
                return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid table header text or icon revision");
            auto result=std::make_shared<Prepared>();result->authored=input;
            if(!result->model.set_configuration(input.configuration))return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid table header configuration");
            auto frame=result->model.layout(width,offset.value_or(input.offset));if(!frame)return ii::core::fail(ii::core::ErrorCode::invalid_argument,frame.error().message);
            result->frame=std::move(*frame);const auto& c=input.configuration;
            auto presentation=NativeTableHeaderPresentation::prepare(c.columns,Policy::appearance(c),c.horizontal_padding,
                input.icon_source,input.icon_revision,previous?previous->presentation:nullptr);
            if(!presentation)return std::unexpected(presentation.error());result->presentation=std::move(*presentation);
            return Snapshot(std::move(result));
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Table header preparation failed",std::current_exception());}
    }
    [[nodiscard]] static ii::gui::ActionOptions action(const Snapshot& value,bool custom_views=false){
        const auto& c=value->authored.configuration;
        return {.accessibility_name=value->authored.text.header_group,.enabled=c.enabled,.focus_ring=false,.width=c.width,
                .height=c.row_height+c.separator_height,.interaction_enabled=c.interactive||custom_views,
                .tab_focusable=c.interactive,.pointer_focusable=c.interactive};
    }
    [[nodiscard]] static ii::core::Result<void> render_views(const std::shared_ptr<State>& state){
        if(!state->views)return {};
        const auto current=state->prepared;const auto& c=current->authored.configuration;
        if(auto extent=state->view_stack.set_extent({current->frame.width,c.row_height});!extent)return extent;
        std::vector<TableViewItem> items;items.reserve(current->frame.columns.size());
        for(const auto& span:current->frame.columns){
            const auto& column=c.columns[span.index];if(!state->views->has(column.key))continue;
            TableViewSnapshot value;value.header=true;value.column_key=column.key;value.column_index=span.index;value.text=column.title;
            value.enabled=c.enabled&&column.enabled;value.sorted=c.sort&&c.sort->column==column.key;
            value.descending=value.sorted&&c.sort->order==TableSortOrder::descending;
            value.x=span.offset;value.width=span.extent;value.height=c.row_height;
            items.push_back({column.key,std::move(value),{float(span.offset),0,float(span.extent),float(c.row_height)}});
        }
        return state->views->update(items,{current->frame.width,c.row_height});
    }
    [[nodiscard]] static ii::core::Result<void> accessibility(const std::shared_ptr<State>& state){
        try{
            const auto current=state->prepared;const auto& c=current->authored.configuration;const auto& frame=current->frame;
            std::vector<ii::gui::VirtualAccessibilityNode> children;children.reserve(frame.columns.size());
            for(const auto& span:frame.columns){
                if(span.index>=c.columns.size())return ii::core::fail(ii::core::ErrorCode::out_of_range,"Table header accessibility index is stale");
                const auto left=(std::max)(0.,span.offset),right=(std::min)(frame.width,span.offset+span.extent);if(right<=left)continue;
                const auto& column=c.columns[span.index];const auto number=std::to_string(span.index+1);
                ii::gui::VirtualAccessibilityNode child;child.key=column.key;
                child.name=column.title.empty()?Policy::unnamed_column(current->authored.text.unnamed_column,number):Policy::display_text(column.title,256);
                if(c.sort&&c.sort->column==column.key)child.description=c.sort->order==TableSortOrder::ascending?
                    current->authored.text.sorted_ascending:current->authored.text.sorted_descending;
                child.bounds={float(left),0,float(right-left),float(c.row_height)};child.enabled=c.enabled&&column.enabled;
                child.focused=state->focused==column.key;child.role=ii::gui::AccessibilityRole::column_header;child.actionable=c.interactive;
                children.push_back(std::move(child));
            }
            std::weak_ptr<State> weak=state;
            return state->root.set_virtual_accessibility(std::move(children),[weak](std::string_view key,ii::gui::WidgetInputKind action)->ii::core::Result<void>{
                auto owner=weak.lock();if(!owner||!owner->live)return ii::core::fail(ii::core::ErrorCode::closed,"Table header accessibility owner retired");
                try{auto result=accessibility_action(owner,key,action);if(!result)owner->failure=result.error();return result;}
                catch(...){owner->failure=ii::core::Error{ii::core::ErrorCode::failed,"Table header accessibility action failed",std::current_exception()};return std::unexpected(*owner->failure);}
            });
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Table header accessibility update failed",std::current_exception());}
    }
    [[nodiscard]] static ii::core::Result<void> accessibility_action(const std::shared_ptr<State>& state,std::string_view key,ii::gui::WidgetInputKind action){
        UpdateGuard guard(state);if(!state->live||!state->prepared)return ii::core::fail(ii::core::ErrorCode::closed,"Table header retired");
        const auto current=state->prepared;const auto revision=state->revision;const auto& c=current->authored.configuration;
        if(!c.enabled||!c.visible||!c.interactive)return {};
        const auto span=std::ranges::find_if(current->frame.columns,[&](const auto& value){return value.index<c.columns.size()&&c.columns[value.index].key==key;});
        if(span==current->frame.columns.end())return ii::core::fail(ii::core::ErrorCode::not_found,"Table header is no longer visible");
        const auto index=span->index;if(!c.columns[index].enabled)return {};
        const auto previous=state->focused;state->focused=c.columns[index].key;
        if(auto focused=state->root.focus();!focused){if(state->live&&state->revision==revision&&state->prepared==current)state->focused=previous;return focused;}
        if(!state->live||state->revision!=revision||state->prepared!=current)return {};
        if(auto published=accessibility(state);!published){if(state->live&&state->revision==revision&&state->prepared==current)state->focused=previous;return published;}
        if(!state->live||state->revision!=revision||state->prepared!=current)return {};
        if(auto painted=state->root.request_repaint();!painted)return painted;
        if(state->live&&state->revision==revision&&state->prepared==current&&action==ii::gui::WidgetInputKind::activate&&state->callbacks.column_clicked){
            auto callback=state->callbacks.column_clicked;auto column=c.columns[index];callback(index,column);
        }
        return {};
    }
    [[nodiscard]] static ii::core::Result<std::shared_ptr<const ii::gui::WidgetDrawing>> paint(
            const std::shared_ptr<State>& state,const ii::gui::WidgetPaintState& view){
        UpdateGuard guard(state);if(!state->prepared)return ii::core::fail(ii::core::ErrorCode::closed,"Table header retired");
        try {
            auto drawing=std::make_shared<ii::gui::WidgetDrawing>();const auto& prepared=*state->prepared;
            const auto& c=prepared.authored.configuration;const auto& frame=prepared.frame;
            const auto background=ii::gui::srgb_to_linear(c.palette.background),divider=ii::gui::srgb_to_linear(c.palette.divider),ink=ii::gui::srgb_to_linear(c.palette.header_text);
            if(view.size.width>0&&view.size.height>0)drawing->panels.push_back({{0,0,float(view.size.width),float(view.size.height)},background,background});
            drawing->images_before_text=true;
            const auto budget=std::min<std::size_t>(8192,std::max<std::size_t>(4,128*1024/std::max<std::size_t>(1,frame.columns.size())));
            for(const auto& span:frame.columns){
                const auto& column=c.columns[span.index];
                const auto left=(std::max)(0.,span.offset),right=(std::min)(view.size.width,span.offset+span.extent);
                if(right<=left)continue;
                const bool sorted=c.sort&&c.sort->column==column.key;
                const auto indicator=sorted&&span.extent>=24?12.:0.;
                const bool custom=state->views&&state->views->has(column.key);
                if(!custom)if(auto content=prepared.presentation->draw(*drawing,span.index,column.title,span.offset,span.extent,c.row_height,
                    view,ink,13,16,indicator,budget);!content)return std::unexpected(content.error());
                if(!custom&&indicator&&view.size.height>0)drawing->texts.push_back({
                    .bounds={float(span.offset+span.extent-indicator),0,float(indicator),float((std::min)(c.row_height,view.size.height))},
                    .text=c.sort->order==TableSortOrder::ascending?"↑":"↓",.color=ink,.font_size=13,.font_weight=500,
                    .alignment=ii::gui::TextAlignment::center,.font_family=view.font_family,.line_height=16,
                    .clip=ii::gui::Rect{float(left),0,float(right-left),float((std::min)(c.row_height,view.size.height))}});
                const auto edge=span.offset+span.extent-1;
                if(edge>=0&&edge<view.size.width&&view.size.height>0)drawing->panels.push_back({{float(edge),0,1,float(view.size.height)},divider,divider});
                if(view.focused&&c.interactive&&c.enabled&&state->focused==column.key&&right-left>=2&&c.row_height>=2){
                    const auto focus=ii::gui::srgb_to_linear(c.palette.current);drawing->panels.push_back({{float(left),0,float(right-left),float((std::min)(c.row_height,view.size.height))},focus,focus,0,1});
                }
            }
            const auto separator=(std::min)(c.separator_height,view.size.height);
            if(view.size.width>0&&separator>0)drawing->panels.push_back({{0,float(view.size.height-separator),float(view.size.width),float(separator)},divider,divider});
            return std::shared_ptr<const ii::gui::WidgetDrawing>(std::move(drawing));
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Table header paint failed",std::current_exception());}
    }
    [[nodiscard]] static ii::core::Result<void> update(const std::shared_ptr<State>& state,TableHeaderInput input){
        if(state->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"Table header requires its owner thread");
        UpdateGuard guard(state);if(state->retired||!state->prepared)return ii::core::fail(ii::core::ErrorCode::closed,"Table header retired");
        try{
            const auto entry_revision=state->revision;const auto was_live=state->live;
            auto live=state->root.snapshot();
            if(was_live&&(!state->live||state->revision!=entry_revision))return {};
            if(!live)return std::unexpected(live.error());if(input==state->prepared->authored)return {};
            if(state->revision==(std::numeric_limits<std::uint64_t>::max)())return ii::core::fail(ii::core::ErrorCode::out_of_range,"Table header revision exhausted");
            const auto observed=state->revision;const auto previous=state->prepared;
            auto comparison=input;comparison.configuration.palette=previous->authored.configuration.palette;
            const bool palette_only=comparison==previous->authored;
            const auto width=input.configuration.width==previous->authored.configuration.width&&live->bounds.width>=1?
                double(live->bounds.width):input.configuration.width;
            auto next=prepare(input,width,input.offset==previous->authored.offset?std::optional(previous->frame.x):std::nullopt,previous);
            if(!state->live||state->revision!=observed)return {};if(!next)return std::unexpected(next.error());
            const auto previous_focused=state->focused;const auto revision=++state->revision;
            state->prepared=std::move(*next);if(!palette_only)state->pressed.reset();
            if(state->focused&&std::ranges::none_of(input.configuration.columns,[&](const auto& column){return column.enabled&&column.key==*state->focused;}))state->focused.reset();
            const auto rollback=[&]{if(state->revision==revision){state->prepared=previous;state->focused=previous_focused;
                (void)state->root.set_action_options(action(previous,bool(state->views)));(void)state->root.set_visible(previous->authored.configuration.visible);
                (void)state->root.set_accessibility_semantics(ii::gui::AccessibilitySemantics{.role=ii::gui::AccessibilityRole::column_header_group,.name=previous->authored.text.header_group});
                (void)accessibility(state);(void)render_views(state);(void)state->root.request_repaint();}};
            if(auto result=state->root.set_action_options(action(state->prepared,bool(state->views)));!result){rollback();return result;}
            if(state->revision!=revision)return {};
            if(auto result=state->root.set_visible(input.configuration.visible);!result){rollback();return result;}
            if(state->revision!=revision)return {};
            auto actual=state->root.snapshot();
            if(!state->live||state->revision!=revision)return {};
            if(!actual){rollback();return std::unexpected(actual.error());}
            if(actual->bounds.width>=1&&actual->bounds.width!=state->prepared->frame.width){
                const auto candidate=state->prepared;
                auto resized=prepare(candidate->authored,actual->bounds.width,candidate->frame.x,candidate);
                if(!state->live||state->revision!=revision)return {};
                if(state->prepared==candidate){if(!resized){rollback();return std::unexpected(resized.error());}state->prepared=std::move(*resized);}
            }
            if(auto result=state->root.set_accessibility_semantics(ii::gui::AccessibilitySemantics{.role=ii::gui::AccessibilityRole::column_header_group,.name=state->prepared->authored.text.header_group});!result){rollback();return result;}
            if(state->revision!=revision)return {};
            if(auto result=accessibility(state);!result){rollback();return result;}
            if(auto result=render_views(state);!result){rollback();return result;}
            if(state->revision!=revision)return {};
            if(auto result=state->root.request_repaint();!result){rollback();return result;}
            state->failure.reset();return {};
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Table header update failed",std::current_exception());}
    }
    [[nodiscard]] static ii::core::Result<bool> input(const std::shared_ptr<State>& state,const ii::gui::WidgetInput& event){
        UpdateGuard guard(state);
        using ii::gui::WidgetInputKind;using ii::gui::WidgetKey;
        if(!state->live)return false;const auto current=state->prepared;const auto& c=current->authored.configuration;
        if(Policy::pointer_canceled(event.kind)||event.kind==WidgetInputKind::pointer_leave){state->pressed.reset();return false;}
        if(event.native_content_control_active||(event.kind==WidgetInputKind::key_down&&event.native_control_active&&event.target!=state->id))return false;
        if(!c.enabled||!c.interactive||!c.visible)return false;
        const auto send=[&](std::size_t index){if(state->live&&state->prepared==current&&state->callbacks.column_clicked){
            auto callback=state->callbacks.column_clicked;auto column=c.columns[index];callback(index,column);}};
        if(event.kind==WidgetInputKind::key_down&&!event.alt&&!event.control&&!event.meta){
            std::optional<std::size_t> index;for(std::size_t i=0;i<c.columns.size();++i)if(c.columns[i].enabled&&state->focused==c.columns[i].key){index=i;break;}
            if(!index)for(std::size_t i=0;i<c.columns.size();++i)if(c.columns[i].enabled){index=i;break;}
            if(!index)return false;
            if(event.key==WidgetKey::left||event.key==WidgetKey::right||event.key==WidgetKey::home||event.key==WidgetKey::end){
                const auto direction=event.key==WidgetKey::left||event.key==WidgetKey::end?-1:1;
                auto position=event.key==WidgetKey::home?std::ptrdiff_t(-1):event.key==WidgetKey::end?std::ptrdiff_t(c.columns.size()):std::ptrdiff_t(*index);
                for(position+=direction;position>=0&&position<std::ptrdiff_t(c.columns.size());position+=direction)
                    if(c.columns[std::size_t(position)].enabled){index=std::size_t(position);break;}
                state->focused=c.columns[*index].key;
                auto revealed=current->frame.source->frame({.height=c.row_height,.header_height=0,.x=current->frame.x,
                    .overscan_rows=0,.overscan_columns=0,.reveal_column=*state->focused});
                if(!revealed)return ii::core::fail(ii::core::ErrorCode::invalid_argument,revealed.error().message);
                if(revealed->x!=current->frame.x){auto next=std::make_shared<Prepared>(*current);next->frame=std::move(*revealed);state->prepared=std::move(next);
                    if(auto rendered=render_views(state);!rendered){state->prepared=current;(void)render_views(state);return std::unexpected(rendered.error());}}
                if(auto result=accessibility(state);!result)return std::unexpected(result.error());
                if(auto result=state->root.request_repaint();!result)return std::unexpected(result.error());return true;
            }
            if(event.key==WidgetKey::enter||event.key==WidgetKey::space){state->focused=c.columns[*index].key;if(auto result=accessibility(state);!result)return std::unexpected(result.error());send(*index);return true;}
            return false;
        }
        if(event.kind!=WidgetInputKind::pointer_down&&event.kind!=WidgetInputKind::pointer_up)return false;
        std::optional<std::size_t> hit;
        if(event.local_position&&event.local_position->y>=0&&event.local_position->y<c.row_height)
            hit=current->model.hit(current->frame,event.local_position->x);
        if(event.kind==WidgetInputKind::pointer_down){
            state->pressed=hit?std::optional(c.columns[*hit].key):std::nullopt;
            if(hit){state->focused=c.columns[*hit].key;if(auto focused=state->root.focus();!focused)return std::unexpected(focused.error());
                if(state->live&&state->prepared==current)if(auto result=accessibility(state);!result)return std::unexpected(result.error());}
            return true;
        }
        const auto pressed=std::exchange(state->pressed,std::nullopt);
        if(hit&&pressed&&*pressed==c.columns[*hit].key)send(*hit);return true;
    }
public:
    [[nodiscard]] static ii::core::Result<BasicNativeTableHeader> create(ii::gui::WidgetBuilder& builder,ii::gui::Widget parent,std::string_view id,
            TableHeaderInput input={},NativeTableHeaderCallbacks callbacks={},NativeTableViews views={}){
        try{
            auto state=std::make_shared<State>();state->id=std::string(id);auto prepared=prepare(input,input.configuration.width);if(!prepared)return std::unexpected(prepared.error());
            state->prepared=std::move(*prepared);state->callbacks=std::move(callbacks);std::weak_ptr<State> weak=state;
            auto root=builder.action_surface(parent,id,action(state->prepared,bool(views.header)||!views.headers.empty()),{});if(!root)return std::unexpected(root.error());state->root=*root;
            if(views.header||!views.headers.empty()){
                auto stack=builder.absolute_stack(*root,std::string(id)+".view-stack",{state->prepared->frame.width,input.configuration.row_height});
                if(!stack)return std::unexpected(stack.error());state->view_stack=*stack;
                auto host=builder.content_host(*stack,std::string(id)+".views",{});if(!host)return std::unexpected(host.error());
                if(auto passthrough=host->set_content_input_passthrough(true);!passthrough)return std::unexpected(passthrough.error());
                state->views=std::make_unique<NativeTableViewGroup>(*host,std::move(views.header),std::move(views.headers));
                if(auto rendered=render_views(state);!rendered)return std::unexpected(rendered.error());
            }
            if(auto semantic=root->set_accessibility_semantics(ii::gui::AccessibilitySemantics{.role=ii::gui::AccessibilityRole::column_header_group,.name=input.text.header_group});!semantic)return std::unexpected(semantic.error());
            if(auto children=accessibility(state);!children)return std::unexpected(children.error());
            if(auto handler=root->set_paint_handler([weak](const auto& view)->ii::core::Result<std::shared_ptr<const ii::gui::WidgetDrawing>>{
                auto state=weak.lock();if(!state)return ii::core::fail(ii::core::ErrorCode::closed,"Table header retired");return paint(state,view);
            });!handler)return std::unexpected(handler.error());
            if(auto handler=root->set_input_handler([weak](const ii::gui::WidgetInput& event)->ii::core::Result<bool>{
                auto state=weak.lock();if(!state)return false;try{auto result=BasicNativeTableHeader::input(state,event);if(!result)state->failure=result.error();return result;}
                catch(...){state->failure=ii::core::Error{ii::core::ErrorCode::failed,"Table header input failed",std::current_exception()};return std::unexpected(*state->failure);}
            });!handler)return std::unexpected(handler.error());
            if(auto handler=root->set_layout_handler([weak](ii::gui::Rect bounds){if(auto state=weak.lock()){
                UpdateGuard guard(state);if(state->retired||!state->prepared)return;
                if(bounds.width<1||bounds.width==state->prepared->frame.width)return;const auto previous=state->prepared;const auto revision=state->revision;const auto was_live=state->live;
                auto result=prepare(previous->authored,bounds.width,previous->frame.x,previous);
                if(state->revision!=revision||state->prepared!=previous||(was_live&&!state->live))return;
                if(result){state->prepared=std::move(*result);
                    if(auto updated=accessibility(state);!updated){if(state->revision==revision){state->prepared=std::move(previous);(void)accessibility(state);}state->failure=updated.error();return;}
                    if(auto rendered=render_views(state);!rendered){if(state->revision==revision){state->prepared=std::move(previous);(void)accessibility(state);(void)render_views(state);}state->failure=rendered.error();return;}
                    (void)state->root.request_repaint();}else state->failure=result.error();
            }});!handler)return std::unexpected(handler.error());
            if(auto visible=root->set_visible(input.configuration.visible);!visible)return std::unexpected(visible.error());
            if(auto retained=builder.retain(state);!retained)return std::unexpected(retained.error());
            if(auto published=builder.on_publish([weak]{if(auto state=weak.lock())state->live=true;});!published)return std::unexpected(published.error());
            if(auto retired=builder.on_retire([weak]{if(auto state=weak.lock()){state->live=false;state->retired=true;
                if(state->revision!=(std::numeric_limits<std::uint64_t>::max)())++state->revision;
                if(state->updates==0)release(state);}});!retired)return std::unexpected(retired.error());
            return BasicNativeTableHeader(std::move(state));
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Table header construction failed",std::current_exception());}
    }
    [[nodiscard]] ii::gui::Widget root()const{return state_->root;}
    [[nodiscard]] ii::core::Result<TableHeaderInput> options()const{
        if(state_->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"Table header requires its owner thread");
        UpdateGuard guard(state_);if(state_->retired||!state_->prepared)return ii::core::fail(ii::core::ErrorCode::closed,"Table header retired");
        if(auto live=state_->root.snapshot();!live)return std::unexpected(live.error());return state_->prepared->authored;
    }
    [[nodiscard]] ii::core::Result<TableViewportFrame> viewport()const{
        if(state_->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"Table header requires its owner thread");
        UpdateGuard guard(state_);if(state_->retired||!state_->prepared)return ii::core::fail(ii::core::ErrorCode::closed,"Table header retired");
        if(auto live=state_->root.snapshot();!live)return std::unexpected(live.error());return state_->prepared->frame;
    }
    [[nodiscard]] ii::core::Result<void> set_options(TableHeaderInput value)const{return update(state_,std::move(value));}
    [[nodiscard]] ii::core::Result<std::optional<ii::core::Error>> failure()const{
        if(state_->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"Table header requires its owner thread");
        if(auto live=state_->root.snapshot();!live)return std::unexpected(live.error());return state_->failure;
    }
private:
    std::shared_ptr<State> state_;
};

} // namespace ii::gui
