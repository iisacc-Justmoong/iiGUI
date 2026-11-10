#pragma once
#include <iiGUI/ButtonModel.h>
#include <iiGUI/EditorViewports.h>
#include <iiGUI/NativeWidgets.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <exception>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace ii::gui {
struct EditorTimelinePalette final {
    WidgetColor surface{},divider{},text{},clip{},selected{},accent{};
    friend bool operator==(const EditorTimelinePalette&,const EditorTimelinePalette&)=default;
};
struct EditorTimelineOptions final {
    double width=0,height=0;
    std::int64_t frame_count=0;
    double pixels_per_frame=1,scroll_x=0,scroll_y=0;
    std::int64_t playhead=0;
    std::uint64_t revision=0;
    bool enabled=true;
    EditorTimelinePalette palette;
    friend bool operator==(const EditorTimelineOptions&,const EditorTimelineOptions&)=default;
};
struct EditorTimelineQuery final {
    ii::gui::EditorFrameRange frames;
    double top=0,bottom=0;
    std::uint64_t revision=0;
    friend bool operator==(const EditorTimelineQuery&,const EditorTimelineQuery&)=default;
};
struct EditorTimelineTrack final {std::string id,label;double top=0,height=40;};
struct EditorTimelineClip final {std::string id,track_id,label;std::int64_t first=0,end=0;bool selected=false,locked=false;};
struct EditorTimelineKey final {std::string id,track_id;std::int64_t frame=0;};
// An immutable app-produced thumbnail, waveform or curve tile in track coordinates.
struct EditorTimelineTile final {
    std::string id,track_id;
    std::int64_t first=0,end=0;
    double top=0,height=0;
    std::shared_ptr<const ii::gui::RasterImage> image;
};
struct EditorTimelineProjection final {
    std::vector<EditorTimelineTrack> tracks;
    std::vector<EditorTimelineClip> clips;
    std::vector<EditorTimelineKey> keys;
    std::vector<EditorTimelineTile> tiles;
};
enum class EditorTimelineGesture {scrub,move_clip,trim_start,trim_end,move_key};
enum class EditorTimelinePhase {begin,update,end,cancel};
struct EditorTimelineEvent final {
    EditorTimelinePhase phase=EditorTimelinePhase::begin;
    EditorTimelineGesture kind=EditorTimelineGesture::scrub;
    std::string id;
    std::int64_t frame=0,initial_frame=0;
};
struct NativeEditorTimelineCallbacks final {
    std::function<ii::core::Result<EditorTimelineProjection>(const EditorTimelineQuery&)> query;
    std::function<ii::core::Result<void>(const EditorTimelineEvent&)> input;
    std::function<void(const EditorTimelineOptions&)> viewport_changed;
};
class NativeEditorTimeline final {
    static constexpr double header=192,ruler=44;
    struct PaintLayers final {
        std::shared_ptr<const ii::gui::WidgetDrawing> background,foreground;
    };
    struct State final {
        ii::gui::Widget root,ink,playhead;
        EditorTimelineOptions options;
        EditorTimelineQuery query;
        std::shared_ptr<const EditorTimelineProjection> projection;
        ii::gui::EditorTimelineViewport viewport;
        PaintLayers drawing;
        NativeEditorTimelineCallbacks callbacks;
        EditorTimelineEvent gesture;
        std::uint64_t device_id=0;
        double pointer_x=0;
        ii::runtime::Clock::time_point edge_time{};
        ii::gui::WidgetFrameHandler edge_frame;
        const std::thread::id owner=std::this_thread::get_id();
        bool live=true,changing=false,active=false,edge_active=false;
    };
    ii::gui::Widget root_;
    std::weak_ptr<State> state_;
    [[nodiscard]] ii::core::Result<std::shared_ptr<State>> lock()const{
        auto s=state_.lock();if(!s||!s->live)return ii::core::fail(ii::core::ErrorCode::closed,"Editor timeline retired");
        if(s->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"Editor timeline requires its owner thread");
        if(auto live=s->root.snapshot();!live)return std::unexpected(live.error());return s;
    }
    [[nodiscard]] static ii::gui::Color color(WidgetColor v)noexcept{
        const auto linear=[](double n){return n<=.04045?n/12.92:std::pow((n+.055)/1.055,2.4);};
        return {float(linear(v.red)),float(linear(v.green)),float(linear(v.blue)),float(v.alpha)};
    }
    [[nodiscard]] static bool projection_valid(const EditorTimelineProjection& p,std::int64_t count){
        if(p.tracks.size()>4096||p.clips.size()>4096||p.keys.size()>4096||p.tiles.size()>4096)return false;
        std::unordered_map<std::string,const EditorTimelineTrack*> tracks;std::unordered_set<std::string> objects,tiles;
        const auto name=[](const std::string& v){return !v.empty()&&v.size()<=256&&DefaultButtonPolicy::valid_text(v);};
        for(const auto& t:p.tracks)if(!name(t.id)||!DefaultButtonPolicy::valid_text(t.label)||t.label.size()>4096||
            !std::isfinite(t.top)||!std::isfinite(t.height)||t.top<0||t.top>1e12||t.height<=0||t.height>8192||!tracks.emplace(t.id,&t).second)return false;
        for(const auto& c:p.clips)if(!name(c.id)||!tracks.contains(c.track_id)||!objects.insert(c.id).second||
            !DefaultButtonPolicy::valid_text(c.label)||c.label.size()>4096||c.first<0||c.end<=c.first||c.end>count)return false;
        for(const auto& k:p.keys)if(!name(k.id)||!tracks.contains(k.track_id)||!objects.insert(k.id).second||k.frame<0||k.frame>=count)return false;
        for(const auto& tile:p.tiles){auto track=tracks.find(tile.track_id);
            if(!name(tile.id)||!tiles.insert(tile.id).second||track==tracks.end()||!tile.image||tile.first<0||tile.end<=tile.first||tile.end>count||
               !std::isfinite(tile.top)||!std::isfinite(tile.height)||tile.top<0||tile.height<=0||tile.top+tile.height>track->second->height)return false;}
        return true;
    }
    [[nodiscard]] static ii::gui::EditorTimelineViewport viewport(const EditorTimelineOptions& o){
        ii::gui::EditorTimelineViewport v;(void)v.configure({.frame_count=o.frame_count,.pixels_per_frame=o.pixels_per_frame,
            .scroll=o.scroll_x,.width=(std::max)(0.,o.width-header)});return v;
    }
    [[nodiscard]] static bool valid(const EditorTimelineOptions& o){
        ii::gui::EditorTimelineViewport v;
        const auto color_valid=[](const WidgetColor& c){const auto channel=[](double x){return std::isfinite(x)&&x>=0&&x<=1;};return channel(c.red)&&channel(c.green)&&channel(c.blue)&&channel(c.alpha);};
        if(!color_valid(o.palette.surface)||!color_valid(o.palette.divider)||!color_valid(o.palette.text)||
           !color_valid(o.palette.clip)||!color_valid(o.palette.selected)||!color_valid(o.palette.accent))return false;
        return std::isfinite(o.width)&&std::isfinite(o.height)&&o.width>=0&&o.height>=0&&o.width<=8192&&o.height<=8192&&
            std::isfinite(o.scroll_y)&&o.scroll_y>=0&&o.scroll_y<=1e12&&o.playhead>=0&&
            (o.frame_count==0?o.playhead==0:o.playhead<o.frame_count)&&
            v.configure({.frame_count=o.frame_count,.pixels_per_frame=o.pixels_per_frame,.scroll=o.scroll_x,.width=(std::max)(0.,o.width-header)});
    }
    [[nodiscard]] static EditorTimelineQuery query_for(const EditorTimelineOptions& o,const ii::gui::EditorTimelineViewport& v){
        return {v.visible_frames(),o.scroll_y,o.scroll_y+(std::max)(0.,o.height-ruler),o.revision};
    }
    [[nodiscard]] static PaintLayers draw(const EditorTimelineOptions& o,
            const ii::gui::EditorTimelineViewport& viewport,const EditorTimelineProjection& data){
        auto d=std::make_shared<ii::gui::WidgetDrawing>();auto foreground=std::make_shared<ii::gui::WidgetDrawing>();
        const auto surface=color(o.palette.surface);
        const auto divider=color(o.palette.divider),ink=color(o.palette.text);
        const auto clip=color(o.palette.clip),selected=color(o.palette.selected);
        const auto accent=color(o.palette.accent);
        const auto add_rect=[&](auto& target,double x,double y,double w,double h,ii::gui::Color c,double radius=0){
            const auto left=std::clamp(x,0.,o.width),top=std::clamp(y,0.,o.height);
            const auto right=std::clamp(x+w,0.,o.width),bottom=std::clamp(y+h,0.,o.height);
            if(right>left&&bottom>top)target->panels.push_back({{float(left),float(top),float(right-left),float(bottom-top)},c,c,float((std::min)(radius,(std::min)(right-left,bottom-top)/2))});};
        const auto rect=[&](double x,double y,double w,double h,ii::gui::Color c,double radius=0){add_rect(d,x,y,w,h,c,radius);};
        const auto front_rect=[&](double x,double y,double w,double h,ii::gui::Color c,double radius=0){add_rect(foreground,x,y,w,h,c,radius);};
        rect(0,0,o.width,o.height,surface);rect(0,ruler-1,o.width,1,divider);rect(header-1,0,1,o.height,divider);
        std::unordered_map<std::string,const EditorTimelineTrack*> tracks;tracks.reserve(data.tracks.size());
        for(const auto& t:data.tracks){tracks.emplace(t.id,&t);const auto y=ruler+t.top-o.scroll_y;
            if(y>=o.height||y+t.height<=ruler)continue;
            rect(0,y+t.height-1,o.width,1,divider);
            const auto label_y=(std::max)(ruler,y);const auto label_bottom=(std::min)(o.height,y+t.height);
            if(header>0&&label_bottom>label_y)foreground->texts.push_back({.bounds={8,float(label_y),float((std::min)(header-16,o.width)),float(label_bottom-label_y)},
                .text=t.label,.color=ink,.font_size=11,.alignment=ii::gui::TextAlignment::left,.line_height=11,.elide_right=true});}
        const auto px=[&](std::int64_t frame){return header+double(frame)*o.pixels_per_frame-viewport.configuration().scroll;};
        for(const auto& c:data.clips){const auto* t=tracks.at(c.track_id);const auto y=ruler+t->top-o.scroll_y;
            const auto left=(std::max)(header,px(c.first)),right=(std::min)(o.width,px(c.end));
            const auto top=(std::max)(ruler,y+4),bottom=(std::min)(o.height,y+t->height-4);
            if(right<=left||bottom<=top)continue;rect(left,top,right-left,bottom-top,c.selected?selected:clip,4);
            if(right-left>12)foreground->texts.push_back({.bounds={float(left+6),float(top),float(right-left-12),float(bottom-top)},.text=c.label,.color=ink,
                .font_size=11,.alignment=ii::gui::TextAlignment::left,.line_height=11,.elide_right=true});}
        for(const auto& tile:data.tiles){const auto* track=tracks.at(tile.track_id);
            const auto left=px(tile.first),right=px(tile.end),top=ruler+track->top-o.scroll_y+tile.top,bottom=top+tile.height;
            const auto x0=(std::max)(header,left),x1=(std::min)(o.width,right),y0=(std::max)(ruler,top),y1=(std::min)(o.height,bottom);
            if(x1<=x0||y1<=y0)continue;
            d->images.push_back({.bounds={float(x0),float(y0),float(x1-x0),float(y1-y0)},.image=tile.image,
                .source={float((x0-left)/(right-left)),float((y0-top)/(bottom-top)),float((x1-x0)/(right-left)),float((y1-y0)/(bottom-top))}});}
        for(const auto& k:data.keys){const auto* t=tracks.at(k.track_id);const auto x=px(k.frame),y=ruler+t->top-o.scroll_y+t->height/2;
            if(x>=header&&x<o.width&&y>=ruler&&y<o.height)front_rect(x-3,y-3,6,6,accent,1);}
        const auto range=viewport.visible_frames();std::int64_t step=1;
        while(double(step)*o.pixels_per_frame<50&&step<100'000'000'000LL)step*=10;
        if(range.end>range.first){auto tick=((range.first+step-1)/step)*step;
            for(;tick<range.end&&tick>=0;tick+=step){const auto x=px(tick);front_rect(x,ruler-8,1,8,ink);
                if(x+30<=o.width)foreground->texts.push_back({.bounds={float(x+3),4,48,16},.text=std::to_string(tick),.color=ink,.font_size=11,.line_height=11});}}
        return {std::move(d),std::move(foreground)};
    }
    [[nodiscard]] static ii::core::Result<void> cancel(const std::shared_ptr<State>& s){
        if(!s->active)return {};s->active=false;s->gesture.phase=EditorTimelinePhase::cancel;
        if(s->edge_active){s->edge_active=false;if(s->live)if(auto r=s->root.set_frame_handler(s->edge_frame,false);!r)return r;}
        if(s->callbacks.input){auto callback=s->callbacks.input;auto current=s->gesture;return callback(current);}return {};
    }
    [[nodiscard]] static double edge_velocity(const State& s)noexcept{
        constexpr double zone=24,speed=360;const auto right=s.options.width;
        if(right<=header+2*zone)return 0;
        if(s.pointer_x<header+zone)return -speed*std::clamp((header+zone-s.pointer_x)/zone,0.,1.);
        if(s.pointer_x>right-zone)return speed*std::clamp((s.pointer_x-(right-zone))/zone,0.,1.);
        return 0;
    }
    [[nodiscard]] static ii::core::Result<void> schedule_edge(const std::shared_ptr<State>& s){
        const auto velocity=s->active&&s->gesture.kind!=EditorTimelineGesture::scrub?edge_velocity(*s):0.;
        auto next=s->viewport;const auto before=next.configuration().scroll;
        if(velocity!=0)(void)next.scroll_by(velocity*.01);
        if(velocity==0||next.configuration().scroll==before){if(s->edge_active){s->edge_active=false;return s->root.set_frame_handler(s->edge_frame,false);}return {};}
        if(!s->edge_active){s->edge_time=ii::runtime::Clock::now();s->edge_active=true;
            if(auto started=s->root.request_frames();!started){s->edge_active=false;return {};}}
        return {};
    }
    [[nodiscard]] static ii::core::Result<bool> input(const std::shared_ptr<State>& s,const ii::gui::WidgetInput& e){
        using K=ii::gui::WidgetInputKind;if(!s->live||!s->options.enabled)return false;
        if(e.kind==K::pointer_cancel||e.kind==K::focus_lost||e.kind==K::key_cancel){bool active=s->active;
            if(auto r=cancel(s);!r)return std::unexpected(r.error());return active;}
        const auto point=e.local_position.value_or(ii::gui::WidgetPoint{e.x,e.y});
        if(e.kind==K::scroll){if(s->active||!std::isfinite(e.scroll_x)||!std::isfinite(e.scroll_y))return false;
            auto view=s->viewport;const auto before=view.configuration();
            if(e.control||e.meta){const auto factor=ii::gui::pinch_scale_factor(e).value_or(std::exp(std::clamp(-e.scroll_y/240.,-4.,4.)));
                const auto target=std::clamp(before.pixels_per_frame*factor,.0001,8192.);
                const auto anchor=std::clamp(point.x-header,0.,before.width);if(!view.zoom_at(target/before.pixels_per_frame,anchor))return false;
            }else if(e.shift||e.scroll_x!=0){const auto delta=e.scroll_x!=0?e.scroll_x:e.scroll_y;
                if(!view.scroll_by(e.device==ii::gui::PointerDevice::touch?-delta:delta))return false;}
            else return false;
            if(view.configuration()==before)return false;auto options=s->options;
            options.pixels_per_frame=view.configuration().pixels_per_frame;options.scroll_x=view.configuration().scroll;
            if(auto r=apply(s,options,false);!r)return std::unexpected(r.error());
            if(s->live&&s->callbacks.viewport_changed){auto callback=s->callbacks.viewport_changed;auto current=s->options;callback(current);}return true;
        }
        if(e.kind==K::pointer_down){if(s->active||point.x<header)return false;auto frame=s->viewport.frame_at(point.x-header);if(!frame)return false;
            EditorTimelineEvent next{.phase=EditorTimelinePhase::begin,.kind=EditorTimelineGesture::scrub,.frame=*frame,.initial_frame=*frame};
            if(point.y>=ruler){bool hit=false;
                for(const auto& t:s->projection->tracks){const auto y=ruler+t.top-s->options.scroll_y;if(point.y<y||point.y>=y+t.height)continue;
                    const auto px=[&](std::int64_t f){return header+double(f)*s->options.pixels_per_frame-s->viewport.configuration().scroll;};
                    for(const auto& key:s->projection->keys)if(key.track_id==t.id&&std::abs(point.x-px(key.frame))<=6&&std::abs(point.y-(y+t.height/2))<=8){
                        next.kind=EditorTimelineGesture::move_key;next.id=key.id;hit=true;break;}
                    if(!hit)for(const auto& clip:s->projection->clips)if(clip.track_id==t.id&&!clip.locked&&point.x>=px(clip.first)&&point.x<px(clip.end)){
                        next.id=clip.id;next.kind=point.x-px(clip.first)<=6?EditorTimelineGesture::trim_start:
                            px(clip.end)-point.x<=6?EditorTimelineGesture::trim_end:EditorTimelineGesture::move_clip;hit=true;break;}
                    break;}
                if(!hit)return false;}
            s->gesture=std::move(next);s->device_id=e.device_id;s->active=true;
            if(s->callbacks.input){auto callback=s->callbacks.input;auto current=s->gesture;auto r=callback(current);if(!r){s->active=false;return std::unexpected(r.error());}}return true;}
        if(!s->active||(s->device_id&&e.device_id&&s->device_id!=e.device_id)||(e.kind!=K::pointer_move&&e.kind!=K::pointer_up))return false;
        if(auto frame=s->viewport.frame_at(point.x-header))s->gesture.frame=*frame;
        s->gesture.phase=e.kind==K::pointer_up?EditorTimelinePhase::end:EditorTimelinePhase::update;
        if(e.kind==K::pointer_up){s->active=false;if(auto r=schedule_edge(s);!r)return std::unexpected(r.error());}
        if(s->callbacks.input){auto callback=s->callbacks.input;auto current=s->gesture;if(auto r=callback(current);!r)return std::unexpected(r.error());}
        if(s->live&&s->active&&e.kind==K::pointer_move){s->pointer_x=point.x;if(auto r=schedule_edge(s);!r)return std::unexpected(r.error());}
        return true;
    }
    [[nodiscard]] static ii::core::Result<void> apply(const std::shared_ptr<State>& s,EditorTimelineOptions value,bool initial){
        if(s->changing)return ii::core::fail(ii::core::ErrorCode::busy,"Editor timeline query already running");
        if(!valid(value))return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid editor timeline configuration");
        auto v=viewport(value);value.scroll_x=v.configuration().scroll;
        if(!initial&&value==s->options)return {};
        s->changing=true;struct Guard{bool& v;~Guard(){v=false;}}guard{s->changing};
        const auto request=query_for(value,v);auto projection=s->projection;
        const bool queried=initial||request!=s->query||value.frame_count!=s->options.frame_count;
        if(queried){auto callback=s->callbacks.query;auto result=callback(request);if(!result)return std::unexpected(result.error());
            if(!projection_valid(*result,value.frame_count))return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid timeline projection");
            projection=std::make_shared<const EditorTimelineProjection>(std::move(*result));
            if(!s->live)return ii::core::fail(ii::core::ErrorCode::closed,"Editor timeline retired during query");}
        const bool redraw=queried||value.width!=s->options.width||value.height!=s->options.height||
            value.scroll_x!=s->options.scroll_x||value.scroll_y!=s->options.scroll_y||value.pixels_per_frame!=s->options.pixels_per_frame||value.palette!=s->options.palette;
        auto drawing=redraw?draw(value,v,*projection):s->drawing;
        if(!value.enabled&&s->options.enabled)if(auto r=cancel(s);!r)return r;
        if(!s->live)return ii::core::fail(ii::core::ErrorCode::closed,"Editor timeline retired during cancellation");
        if(initial||value.width!=s->options.width||value.height!=s->options.height)if(auto r=s->root.set_extent({value.width,value.height});!r)return r;
        if(initial||value.width!=s->options.width||value.height!=s->options.height)
            if(auto r=s->ink.set_frame({0,0,float(value.width),float(value.height)});!r)return r;
        if(initial||value.enabled!=s->options.enabled)if(auto r=s->root.set_enabled(value.enabled);!r)return r;
        if(initial||value.height!=s->options.height||value.palette.accent!=s->options.palette.accent){ii::gui::WidgetDrawing line;auto ink=color(value.palette.accent);
            line.panels.push_back({{0,0,1,float(value.height)},ink,ink,0});if(auto r=s->playhead.set_drawing(std::move(line));!r)return r;}
        const auto x=header+double(value.playhead)*value.pixels_per_frame-v.configuration().scroll;
        if(auto r=s->playhead.set_visible(value.frame_count>0&&x>=header&&x<value.width);!r)return r;
        if(auto r=s->playhead.set_frame({float(std::clamp(x,0.,value.width)),0,1,float(value.height)});!r)return r;
        s->options=value;s->viewport=v;s->query=request;s->projection=std::move(projection);s->drawing=std::move(drawing);
        if(redraw){if(auto r=s->ink.request_repaint();!r)return r;return s->root.request_repaint();}return {};
    }
    [[nodiscard]] static ii::core::Result<bool> edge_tick(const std::shared_ptr<State>& s,ii::runtime::Clock::time_point now){
        if(!s->live||!s->active||!s->edge_active||!s->options.enabled)return false;
        auto visibility=s->root.interaction();if(!visibility)return std::unexpected(visibility.error());
        if(!visibility->visible||!visibility->enabled){if(auto r=cancel(s);!r)return std::unexpected(r.error());return false;}
        const auto velocity=edge_velocity(*s);if(velocity==0){s->edge_active=false;return false;}
        const auto elapsed=std::clamp(std::chrono::duration<double>(now-s->edge_time).count(),0.,.1);s->edge_time=(std::max)(s->edge_time,now);
        auto moved=s->viewport;const auto previous=moved.configuration().scroll;(void)moved.scroll_by(velocity*elapsed);
        if(moved.configuration().scroll==previous){if(elapsed==0)return true;s->edge_active=false;return false;}
        auto options=s->options;options.scroll_x=moved.configuration().scroll;
        if(auto result=apply(s,options,false);!result){s->edge_active=false;return std::unexpected(result.error());}
        if(!s->live||!s->active)return false;
        if(s->callbacks.viewport_changed){auto callback=s->callbacks.viewport_changed;auto current=s->options;callback(current);}
        if(!s->live||!s->active)return false;
        if(auto frame=s->viewport.frame_at(s->pointer_x-header))s->gesture.frame=*frame;
        s->gesture.phase=EditorTimelinePhase::update;
        if(s->callbacks.input){auto callback=s->callbacks.input;auto current=s->gesture;if(auto r=callback(current);!r){s->edge_active=false;return std::unexpected(r.error());}}
        if(!s->live||!s->active||!s->edge_active)return false;
        auto probe=s->viewport;const auto current=probe.configuration().scroll;(void)probe.scroll_by(velocity*.01);
        if(probe.configuration().scroll==current){s->edge_active=false;return false;}return true;
    }
public:
    [[nodiscard]] static ii::core::Result<NativeEditorTimeline> create(ii::gui::WidgetBuilder& builder,ii::gui::Widget parent,
            std::string_view id,EditorTimelineOptions options,NativeEditorTimelineCallbacks callbacks){
        if(!valid(options)||!callbacks.query)return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Editor timeline requires a valid viewport and source");
        try{auto root=builder.absolute_stack(parent,id,{options.width,options.height});if(!root)return std::unexpected(root.error());
            auto s=std::make_shared<State>();s->root=*root;s->callbacks=std::move(callbacks);std::weak_ptr<State> weak=s;
            s->edge_frame=[weak](ii::runtime::Clock::time_point now)->ii::core::Result<bool>{auto s=weak.lock();return s?edge_tick(s,now):ii::core::Result<bool>(false);};
            if(auto r=root->set_frame_handler(s->edge_frame,false);!r)return std::unexpected(r.error());
            auto ink=builder.absolute_stack(*root,std::string(id)+".ink",{options.width,options.height});
            if(!ink)return std::unexpected(ink.error());s->ink=*ink;
            auto playhead=builder.absolute_stack(*root,std::string(id)+".playhead",{1,options.height});
            if(!playhead)return std::unexpected(playhead.error());s->playhead=*playhead;
            if(auto r=builder.retain(s);!r)return std::unexpected(r.error());
            if(auto r=builder.on_retire([weak]{if(auto s=weak.lock()){s->live=false;try{(void)cancel(s);}catch(...){}s->callbacks={};s->drawing={};s->projection={};}});!r)return std::unexpected(r.error());
            if(auto r=root->set_paint_handler([weak](const auto&)->ii::core::Result<std::shared_ptr<const ii::gui::WidgetDrawing>>{
                auto s=weak.lock();if(!s||!s->live||!s->drawing.background)return ii::core::fail(ii::core::ErrorCode::closed,"Editor timeline retired");return s->drawing.background;});!r)return std::unexpected(r.error());
            if(auto r=ink->set_paint_handler([weak](const auto&)->ii::core::Result<std::shared_ptr<const ii::gui::WidgetDrawing>>{
                auto s=weak.lock();if(!s||!s->live||!s->drawing.foreground)return ii::core::fail(ii::core::ErrorCode::closed,"Editor timeline retired");return s->drawing.foreground;});!r)return std::unexpected(r.error());
            if(auto r=root->set_input_handler([weak](const auto& e)->ii::core::Result<bool>{auto s=weak.lock();return s?input(s,e):ii::core::Result<bool>(false);});!r)return std::unexpected(r.error());
            if(auto r=apply(s,options,true);!r)return std::unexpected(r.error());NativeEditorTimeline result;result.root_=*root;result.state_=s;return result;
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Editor timeline creation failed",std::current_exception());}
    }
    [[nodiscard]] ii::gui::Widget root()const noexcept{return root_;}
    [[nodiscard]] ii::core::Result<EditorTimelineOptions> options()const{auto s=lock();if(!s)return std::unexpected(s.error());return (*s)->options;}
    [[nodiscard]] ii::core::Result<void> set_options(EditorTimelineOptions value)const{auto s=lock();if(!s)return std::unexpected(s.error());
        try{return apply(*s,value,false);}catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Editor timeline update failed",std::current_exception());}}
};
} // namespace ii::gui
