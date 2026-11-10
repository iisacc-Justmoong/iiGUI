#include <iiGUI/NativeEditorTimelineView.h>
#include <iiGUI/WidgetState.h>
#include <iiSDKCore/Runtime/FrameLoop.h>
#include <chrono>
#include <cmath>
#include <stdexcept>
#include <vector>
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
template<class T>T take(ii::core::Result<T> value){if(!value)throw std::runtime_error(value.error().message);return std::move(*value);}
void good(ii::core::Result<void> value){if(!value)throw std::runtime_error(value.error().message);}
void edge_scroll(){
    using namespace ii::gui;using namespace std::chrono_literals;
    ii::runtime::EventLoop events;ii::runtime::FrameLoop frames(events);detail::WidgetFrameHooks hooks;
    hooks.frames=[&]()->ii::core::Result<ii::runtime::FrameLoop*>{return &frames;};
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();},WidgetAppearance{},std::move(hooks));
    auto host=take(tree->content_host({},"host",{400,160}));std::optional<NativeEditorTimeline> timeline;int queries=0,canceled=0,viewport_changes=0;bool retire_on_viewport=false;
    EditorTimelineOptions options{.width=400,.height=160,.frame_count=1000,.pixels_per_frame=2};NativeEditorTimelineCallbacks cb;
    cb.query=[&](const EditorTimelineQuery&)->ii::core::Result<EditorTimelineProjection>{++queries;return EditorTimelineProjection{
        .tracks={{"t","Images",0,40}},.clips={{"c","t","Clip",10,40}}};};
    cb.input=[&](const EditorTimelineEvent& e)->ii::core::Result<void>{if(e.phase==EditorTimelinePhase::cancel)++canceled;return {};};
    cb.viewport_changed=[&](const EditorTimelineOptions&){++viewport_changes;if(retire_on_viewport)good(host.clear_content());};
    good(host.set_content([&](WidgetBuilder& b)->ii::core::Result<void>{auto v=NativeEditorTimeline::create(b,{},"timeline",options,cb);
        if(!v)return std::unexpected(v.error());timeline=*v;return {};}));(void)take(tree->drawing_snapshot({400,160}));
    good(tree->input({.kind=WidgetInputKind::pointer_down,.x=230,.y=64}));
    const auto start=ii::runtime::Clock::now();good(tree->input({.kind=WidgetInputKind::pointer_move,.x=399,.y=64}));
    (void)frames.step(start+100ms);check(take(timeline->options()).scroll_x>0&&queries>1,"drag edge did not request bounded scrolling");
    options=take(timeline->options());options.enabled=false;good(timeline->set_options(options));
    check(canceled==1,"disabling timeline did not cancel drag");const auto idle=frames.stats().callbacks;
    (void)frames.step(start+300ms);check(frames.stats().callbacks==idle,"canceled drag kept requesting frames");
    options.enabled=true;good(timeline->set_options(options));const auto old=take(timeline->options());
    const auto anchor=(old.scroll_x+58)/old.pixels_per_frame;
    good(tree->input({.kind=WidgetInputKind::scroll,.x=250,.y=30,.scroll_y=-50,.control=true}));
    const auto zoomed=take(timeline->options());check(zoomed.pixels_per_frame>old.pixels_per_frame&&
        std::abs((zoomed.scroll_x+58)/zoomed.pixels_per_frame-anchor)<1e-6&&viewport_changes>0,
        "timeline wheel zoom did not preserve its anchor or report the viewport");
    good(tree->input({.kind=WidgetInputKind::scroll,.x=250,.y=30,.scroll_y=std::log(2.)*pinch_scroll_units,
                      .control=true,.precise_scroll=true,.device=PointerDevice::touch}));
    const auto pinched=take(timeline->options());
    check(std::abs(pinched.pixels_per_frame-2*zoomed.pixels_per_frame)<1e-6,
          "pinch-out did not enlarge the timeline");
    good(tree->input({.kind=WidgetInputKind::scroll,.x=250,.y=30,.scroll_x=-8,
                      .precise_scroll=true,.device=PointerDevice::touch}));
    check(std::abs(take(timeline->options()).scroll_x-pinched.scroll_x-8)<1e-6,
          "timeline did not follow the touch pan");
    retire_on_viewport=true;good(tree->input({.kind=WidgetInputKind::scroll,.x=250,.y=30,.scroll_x=10}));
    check(!timeline->options(),"viewport callback did not safely retire the timeline");tree->close();
}
int main(){
    using namespace ii::gui;
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{400,160}));std::optional<NativeEditorTimeline> timeline;
    EditorTimelineOptions options{.width=400,.height=160,.frame_count=1000,.pixels_per_frame=2,.playhead=0,.revision=1};
    int queries=0;bool fail=false;std::vector<EditorTimelineEvent> events;
    auto tile=take(rgba_image(2,1,{255,0,0,255,0,0,255,255}));
    NativeEditorTimelineCallbacks callbacks;
    callbacks.query=[&](const EditorTimelineQuery& q)->ii::core::Result<EditorTimelineProjection>{++queries;
        if(fail)return ii::core::fail(ii::core::ErrorCode::failed,"query failed");
        check(q.frames.first==0&&q.frames.end==104,"visible frames not bounded");
        return EditorTimelineProjection{.tracks={{"track","Images",0,40}},.clips={{"clip","track","Clip",10,40,false,false}},
            .keys={{"key","track",25}},.tiles={{"tile","track",10,40,0,40,tile}}};};
    callbacks.input=[&](const EditorTimelineEvent& e)->ii::core::Result<void>{events.push_back(e);return {};};
    good(host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{auto v=NativeEditorTimeline::create(builder,{},"timeline",options,callbacks);
        if(!v)return std::unexpected(v.error());timeline=*v;return {};}));
    auto frame=take(tree->drawing_snapshot({400,160}));check(queries==1,"initial query missing");
    const auto tile_present=[&](const auto& image_frame){auto has=[&](const auto& images){return std::ranges::any_of(images,[&](const auto& image){return image.image==tile;});};
        return has(image_frame.images)||std::ranges::any_of(image_frame.overlays,[&](const auto& layer){return has(layer.images);});};
    check(tile_present(frame),"visible timeline media tile was not drawn");
    const auto key_after_tile=[&](const auto& image_frame){std::size_t image_layer=0,key_layer=0;bool image_seen=false,key_seen=false;
        auto inspect=[&](const auto& panels,const auto& images,std::size_t index){for(const auto& image:images)if(image.image==tile){image_seen=true;image_layer=index;}
            for(const auto& p:panels)if(p.bounds.width==6&&p.bounds.height==6){key_seen=true;key_layer=index;}};
        inspect(image_frame.panels,image_frame.images,0);std::size_t index=1;for(const auto& layer:image_frame.overlays)inspect(layer.panels,layer.images,index++);
        return image_seen&&key_seen&&key_layer>image_layer;};
    check(key_after_tile(frame),"timeline image obscured the keyframe marker");
    (void)take(tree->drawing_snapshot({400,160}));check(queries==1,"redraw queried data");
    options.palette.surface={1,0,0,1};good(timeline->set_options(options));
    auto themed=take(tree->drawing_snapshot({400,160}));
    const auto red=[](const auto& panels){return std::ranges::any_of(panels,[](const auto& p){return p.bounds.width==400&&p.bounds.height==160&&p.top.red==1&&p.top.green==0;});};
    check((red(themed.panels)||std::ranges::any_of(themed.overlays,[&](const auto& layer){return red(layer.panels);}))&&queries==1,"Palette update did not redraw without querying");
    auto bad=options;bad.palette.surface.alpha=2;check(!timeline->set_options(bad)&&take(timeline->options()).palette.surface.alpha==1,"Invalid palette replaced state");
    options.playhead=7;good(timeline->set_options(options));check(queries==1&&tile_present(take(tree->drawing_snapshot({400,160}))),"playhead change replaced or queried media tiles");
    fail=true;++options.revision;check(!timeline->set_options(options)&&take(timeline->options()).revision==1,"failed query replaced state");
    fail=false;good(timeline->set_options(options));check(queries==3,"revision did not query");
    good(tree->input({.kind=WidgetInputKind::pointer_down,.x=212,.y=20}));
    good(tree->input({.kind=WidgetInputKind::pointer_move,.x=232,.y=20}));
    good(tree->input({.kind=WidgetInputKind::pointer_up,.x=232,.y=20}));
    check(events.size()==3&&events[0].kind==EditorTimelineGesture::scrub&&events[0].frame==10&&
          events[2].phase==EditorTimelinePhase::end&&events[2].frame==20,"scrub events incorrect");
    good(tree->input({.kind=WidgetInputKind::pointer_down,.x=213,.y=64}));
    check(events.back().kind==EditorTimelineGesture::trim_start&&events.back().id=="clip","clip edge hit incorrect");
    good(host.clear_content());check(events.back().phase==EditorTimelinePhase::cancel&&!timeline->set_options(options),"retirement did not cancel");tree->close();edge_scroll();
}
