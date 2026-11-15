#pragma once
#include <iiGUI/EditorViewports.h>
#include <iiGUI/NativeWidgets.h>
#include <cmath>
#include <algorithm>
#include <array>
#include <exception>
#include <functional>
#include <memory>
#include <thread>

namespace ii::gui {
struct EditorCanvasOptions final {
    double width=0,height=0;
    ii::gui::EditorCanvasViewportConfiguration transform;
    std::uint64_t revision=0;
    bool enabled=true;
    friend bool operator==(const EditorCanvasOptions&,const EditorCanvasOptions&)=default;
};
struct EditorCanvasBounds final {double x=0,y=0,width=0,height=0;};
struct EditorCanvasQuery final {
    EditorCanvasBounds document_bounds;
    ii::gui::Size surface;
    ii::gui::EditorCanvasViewportConfiguration transform;
    std::uint64_t revision=0;
};
enum class EditorCanvasPhase { begin,update,end,cancel,hover,scroll };
struct EditorCanvasInput final {
    EditorCanvasPhase phase=EditorCanvasPhase::update;
    ii::gui::EditorPoint document;
    ii::gui::WidgetInput native;
};
struct NativeEditorCanvasCallbacks final {
    std::function<ii::core::Result<std::shared_ptr<const ii::gui::WidgetDrawing>>(const EditorCanvasQuery&)> query;
    std::function<ii::core::Result<void>(const EditorCanvasInput&)> input;
};
// An application-side bridge: the source owns its document and snapshots.
class NativeEditorCanvas final {
    struct State final {
        ii::gui::Widget root;
        EditorCanvasOptions options;
        ii::gui::EditorCanvasViewport viewport;
        NativeEditorCanvasCallbacks callbacks;
        std::shared_ptr<const ii::gui::WidgetDrawing> drawing;
        ii::gui::WidgetInput last;
        const std::thread::id owner=std::this_thread::get_id();
        bool live=true,changing=false,active=false;
    };
    ii::gui::Widget root_;
    std::weak_ptr<State> state_;
    [[nodiscard]] ii::core::Result<std::shared_ptr<State>> lock()const{
        auto s=state_.lock();if(!s||!s->live)return ii::core::fail(ii::core::ErrorCode::closed,"Editor canvas retired");
        if(s->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"Editor canvas requires its owner thread");
        if(auto live=s->root.snapshot();!live)return std::unexpected(live.error());return s;
    }
    [[nodiscard]] static EditorCanvasInput event(const State& s,EditorCanvasPhase phase,ii::gui::WidgetInput value){
        const auto local=value.local_position.value_or(ii::gui::WidgetPoint{value.x,value.y});
        return {phase,s.viewport.document_point({local.x,local.y}),std::move(value)};
    }
    [[nodiscard]] static ii::core::Result<void> cancel(const std::shared_ptr<State>& s){
        if(!s->active)return {};s->active=false;
        if(s->callbacks.input){auto callback=s->callbacks.input;return callback(event(*s,EditorCanvasPhase::cancel,s->last));}return {};
    }
    [[nodiscard]] static ii::core::Result<bool> input(const std::shared_ptr<State>& s,const ii::gui::WidgetInput& value){
        using K=ii::gui::WidgetInputKind;if(!s->live||!s->options.enabled)return false;
        if(value.kind==K::pointer_cancel||value.kind==K::focus_lost||value.kind==K::key_cancel){const bool active=s->active;
            if(auto r=cancel(s);!r)return std::unexpected(r.error());return active;}
        if(value.kind==K::scroll){if(!s->callbacks.input)return false;auto callback=s->callbacks.input;
            if(auto r=callback(event(*s,EditorCanvasPhase::scroll,value));!r)return std::unexpected(r.error());return true;}
        if(value.kind==K::pointer_move&&!s->active){if(s->callbacks.input)
            {auto callback=s->callbacks.input;if(auto r=callback(event(*s,EditorCanvasPhase::hover,value));!r)return std::unexpected(r.error());}return false;}
        if(value.kind==K::pointer_down){if(s->active)return false;s->active=true;s->last=value;
            if(s->callbacks.input){auto callback=s->callbacks.input;auto r=callback(event(*s,EditorCanvasPhase::begin,value));if(!r){s->active=false;return std::unexpected(r.error());}}return true;}
        if(!s->active||(s->last.device_id&&value.device_id&&s->last.device_id!=value.device_id))return false;
        if(value.kind!=K::pointer_move&&value.kind!=K::pointer_up)return false;s->last=value;
        const auto phase=value.kind==K::pointer_up?EditorCanvasPhase::end:EditorCanvasPhase::update;
        if(phase==EditorCanvasPhase::end)s->active=false;
        if(s->callbacks.input){auto callback=s->callbacks.input;if(auto r=callback(event(*s,phase,value));!r)return std::unexpected(r.error());}return true;
    }
    [[nodiscard]] static bool valid(const EditorCanvasOptions& value){
        ii::gui::EditorCanvasViewport viewport;
        return std::isfinite(value.width)&&std::isfinite(value.height)&&value.width>=0&&value.height>=0&&
            value.width<=8192&&value.height<=8192&&viewport.configure(value.transform);
    }
    [[nodiscard]] static ii::core::Result<void> apply(const std::shared_ptr<State>& s,EditorCanvasOptions value,bool initial){
        if(s->changing)return ii::core::fail(ii::core::ErrorCode::busy,"Editor canvas query is already in progress");
        if(!valid(value))return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid editor canvas viewport");
        if(!initial&&value==s->options)return {};
        s->changing=true;struct Guard{bool& v;~Guard(){v=false;}}guard{s->changing};
        ii::gui::EditorCanvasViewport viewport;(void)viewport.configure(value.transform);
        const bool query=initial||value.width!=s->options.width||value.height!=s->options.height||
            value.transform!=s->options.transform||value.revision!=s->options.revision;
        auto drawing=s->drawing;
        if(query){const std::array points{viewport.document_point({0,0}),viewport.document_point({value.width,0}),
                viewport.document_point({0,value.height}),viewport.document_point({value.width,value.height})};
            const auto [min_x,max_x]=std::ranges::minmax(points,{},&ii::gui::EditorPoint::x);
            const auto [min_y,max_y]=std::ranges::minmax(points,{},&ii::gui::EditorPoint::y);
            EditorCanvasQuery request{{min_x.x,min_y.y,max_x.x-min_x.x,max_y.y-min_y.y},
                {value.width,value.height},value.transform,value.revision};
            auto callback=s->callbacks.query;auto result=callback(request);if(!result)return std::unexpected(result.error());
            if(!*result)return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Editor canvas query returned no drawing");drawing=std::move(*result);
            if(!s->live)return ii::core::fail(ii::core::ErrorCode::closed,"Editor canvas retired during query");}
        if(!value.enabled&&s->options.enabled)if(auto r=cancel(s);!r)return r;
        if(!s->live)return ii::core::fail(ii::core::ErrorCode::closed,"Editor canvas retired during input cancellation");
        if(initial||value.width!=s->options.width||value.height!=s->options.height)
            if(auto r=s->root.set_extent({value.width,value.height});!r)return r;
        if(initial||value.enabled!=s->options.enabled)if(auto r=s->root.set_enabled(value.enabled);!r)return r;
        s->viewport=viewport;s->options=value;s->drawing=std::move(drawing);
        if(query)return s->root.request_repaint();return {};
    }
public:
    [[nodiscard]] static ii::core::Result<NativeEditorCanvas> create(ii::gui::WidgetBuilder& builder,ii::gui::Widget parent,
            std::string_view id,EditorCanvasOptions options,NativeEditorCanvasCallbacks callbacks){
        if(!valid(options)||!callbacks.query)return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Editor canvas requires a valid query and viewport");
        try{auto root=builder.absolute_stack(parent,id,{options.width,options.height});if(!root)return std::unexpected(root.error());
            auto s=std::make_shared<State>();s->root=*root;s->callbacks=std::move(callbacks);std::weak_ptr<State> weak=s;
            if(auto r=builder.retain(s);!r)return std::unexpected(r.error());
            if(auto r=builder.on_retire([weak]{if(auto s=weak.lock()){s->live=false;try{(void)cancel(s);}catch(...){}s->callbacks={};s->drawing.reset();}});!r)return std::unexpected(r.error());
            if(auto r=root->set_paint_handler([weak](const auto&)->ii::core::Result<std::shared_ptr<const ii::gui::WidgetDrawing>>{
                auto s=weak.lock();if(!s||!s->live||!s->drawing)return ii::core::fail(ii::core::ErrorCode::closed,"Editor canvas retired");return s->drawing;});!r)return std::unexpected(r.error());
            if(auto r=root->set_input_handler([weak](const auto& value)->ii::core::Result<bool>{auto s=weak.lock();return s?input(s,value):ii::core::Result<bool>(false);});!r)return std::unexpected(r.error());
            if(auto r=apply(s,options,true);!r)return std::unexpected(r.error());
            NativeEditorCanvas result;result.root_=*root;result.state_=s;return result;
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Editor canvas creation failed",std::current_exception());}
    }
    [[nodiscard]] ii::gui::Widget root()const noexcept{return root_;}
    [[nodiscard]] ii::core::Result<EditorCanvasOptions> options()const{auto s=lock();if(!s)return std::unexpected(s.error());return (*s)->options;}
    [[nodiscard]] ii::core::Result<void> set_options(EditorCanvasOptions options)const{auto s=lock();if(!s)return std::unexpected(s.error());
        try{return apply(*s,options,false);}catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Editor canvas update failed",std::current_exception());}}
};
} // namespace ii::gui
