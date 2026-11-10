#include <iiGUI/WidgetState.h>
#include <iiSDKCore/Runtime/FrameLoop.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <future>
#include <iostream>
#include <stdexcept>

using namespace ii;
namespace {
void require(bool value,const char* reason){if(!value)throw std::runtime_error(reason);}
template<class T>T take(core::Result<T> value){if(!value){if(value.error().cause)std::rethrow_exception(value.error().cause);throw std::runtime_error(value.error().message);}return std::move(*value);}
void good(core::Result<void> value){if(!value){if(value.error().cause)std::rethrow_exception(value.error().cause);throw std::runtime_error(value.error().message);}}
bool near(float a,float b){return std::abs(a-b)<1e-5f;}
bool near(gui::Rect a,gui::Rect b){return near(a.x,b.x)&&near(a.y,b.y)&&near(a.width,b.width)&&near(a.height,b.height);}
gui::Rect transformed(gui::Rect r,const gui::ContentTransform& t){
    return {t.origin_x+(r.x-t.origin_x)*t.scale+t.offset_x,t.origin_y+(r.y-t.origin_y)*t.scale+t.offset_y,r.width*t.scale,r.height*t.scale};
}
struct Ink final {gui::TextRun text;std::optional<gui::Rect> clip;gui::ContentTransform transform;};
std::optional<Ink> text_in(const gui::CanvasScene& scene,std::string_view name){
    for(const auto& layer:scene.layers)for(const auto& text:layer.texts)if(text.text==name)return Ink{text,layer.clip,layer.content_transform};
    return std::nullopt;
}
std::optional<Ink> text_in(const gui::CanvasFrame& scene,std::string_view name){
    for(const auto& text:scene.texts)if(text.text==name)return Ink{text,{}, {}};
    for(const auto& layer:scene.overlays)for(const auto& text:layer.texts)if(text.text==name)return Ink{text,layer.clip,layer.content_transform};
    return std::nullopt;
}
void same_ink(const gui::CanvasFrame& full,const gui::CanvasScene& selected,std::string_view name){
    const auto expected=text_in(full,name),actual=text_in(selected,name);
    require(expected&&actual,"Selected drawing omitted text");
    require(expected->text==actual->text&&expected->clip==actual->clip&&expected->transform==actual->transform,
            "Selected drawing changed the on-screen text, clipping or transform");
}
struct Fixture final {
    std::shared_ptr<gui::detail::WidgetState> tree=std::make_shared<gui::detail::WidgetState>([](runtime::Task task){task();});
    gui::Widget host=take(tree->content_host({},"host",{400,300})),root,group,inside,slot,embedded_root,embedded,outside,popup,popup_chrome,popup_anchor,popup_static,popup_body,menu,first,second;
    Fixture(){
        good(host.set_content([&](gui::WidgetBuilder& b)->core::Result<void>{
            root=take(b.vstack({},"root",{.spacing=4,.alignment=gui::StackAlignment::stretch}));
            group=take(b.vstack(root,"group",{.spacing=2,.alignment=gui::StackAlignment::stretch}));
            inside=take(b.label(group,"inside",{.text="Inside"}));
            slot=take(b.content_host(group,"slot",{200,30}));
            good(slot.set_content([&](gui::WidgetBuilder& child)->core::Result<void>{
                embedded_root=take(child.vstack({},"embedded-root",{.alignment=gui::StackAlignment::stretch}));
                embedded=take(child.label(embedded_root,"embedded",{.text="Embedded"}));return {};
            }));
            outside=take(b.label(root,"outside",{.text="Outside"}));
            popup=take(b.popover(root,"popup",{.width=160,.height=100,.modal=true,.dim=true,.clip_content=true}));
            popup_chrome=take(b.vstack(popup,"popup-chrome",{.alignment=gui::StackAlignment::stretch}));
            popup_static=take(b.label(popup_chrome,"popup-static",{.text="Popup static"}));
            popup_anchor=take(b.vstack(popup_chrome,"popup-anchor",{.alignment=gui::StackAlignment::stretch}));
            good(popup.set_popup_content_at(popup_anchor,[&](gui::WidgetBuilder& child)->core::Result<void>{popup_body=take(child.label({},"popup-body",{.text="Popup body"}));return {};}));
            menu=take(b.menu(root,"menu"));first=take(b.menu_item(menu,"first",{.label="First item"}));
            second=take(b.menu_item(menu,"second",{.label="Second item"}));
            return {};
        }));
        good(host.set_opacity(.8));good(root.set_opacity(.5));good(group.set_opacity(.5));good(inside.set_opacity(.5));
        good(group.set_clip_content(true));
    }
    ~Fixture(){tree->close();}
    gui::CanvasFrame frame(){return take(tree->drawing_snapshot({400,300}));}
};
void ordinary(){
    Fixture f;const auto full=f.frame();const auto scene=take(f.group.snapshot_scene());
    require(scene.viewport==full.viewport,"Subtree viewport differs from its native window");
    require(scene.bounds&&*scene.bounds==take(f.group.snapshot()).visual_bounds,"Subtree crop differs from its visual bounds");
    same_ink(full,scene,"Inside");same_ink(full,scene,"Embedded");
    require(!text_in(scene,"Outside"),"A sibling entered the selected subtree");
    require(text_in(scene,"Inside")->clip.has_value(),"Ancestor clip was lost");
    require(near(text_in(scene,"Inside")->transform.opacity,.1f),"Ancestor opacity was not retained");
    const auto embedded=take(f.embedded.snapshot_scene());same_ink(full,embedded,"Embedded");
    require(embedded.bounds&&*embedded.bounds==take(f.embedded.snapshot()).visual_bounds,"Embedded crop lost its containing window offset");
    require(!text_in(embedded,"Inside")&&!text_in(embedded,"Outside"),"Embedded selection included its parent or sibling");
    require(take(f.embedded.is_in_scene_of(f.group))&&take(f.embedded.is_in_scene_of(f.host))&&
            take(f.group.is_in_scene_of(f.group))&&!take(f.group.is_in_scene_of(f.embedded))&&
            !take(f.outside.is_in_scene_of(f.group)),"Scene ancestry differs across ordinary or embedded content");
    const auto old=scene;good(f.inside.set_text("Updated"));const auto changed=take(f.group.snapshot_scene());
    require(text_in(old,"Inside")&&!text_in(old,"Updated")&&text_in(changed,"Updated"),"Snapshot did not retain immutable drawing values");
    good(f.host.set_extent({320,240}));const auto resized=take(f.tree->drawing_snapshot({320,240}));const auto resized_source=take(f.embedded.snapshot_scene());
    require(resized_source.viewport==resized.viewport,"Source viewport did not follow resize");same_ink(resized,resized_source,"Embedded");
    good(f.group.set_visible(false));require(take(f.group.snapshot_scene()).layers.empty(),"Hidden subtree produced drawing commands");
    good(f.group.set_visible(true));good(f.group.set_opacity(0));require(take(f.embedded.snapshot_scene()).layers.empty(),"Invisible ancestor produced drawing commands");
}
void popup(){
    Fixture f;good(f.popup.open_at(0,0));const auto full=f.frame();
    const auto body=take(f.popup_body.snapshot_scene());same_ink(full,body,"Popup body");
    require(!text_in(body,"Popup static")&&!text_in(body,"Outside"),"Popup body captured unrelated chrome or content");
    for(const auto& layer:body.layers)require(!layer.backdrop&&!layer.shadow,"Popup body included the popup material");
    const auto outside=take(f.group.snapshot_scene());require(!text_in(outside,"Popup body"),"A sibling popup leaked into the subtree");
    for(const auto source:{f.root,f.host}){
        const auto content=take(source.snapshot_scene());
        require(text_in(content,"Inside")&&!text_in(content,"Popup static")&&!text_in(content,"Popup body"),
                "A separately portaled popup leaked into its logical ancestor's source scene");
        for(const auto& layer:content.layers)require(layer.panels.empty()&&!layer.backdrop&&!layer.shadow,
                "Popup dim or material leaked into an ordinary content source");
    }
    const auto popup=take(f.popup.snapshot_scene());same_ink(full,popup,"Popup body");same_ink(full,popup,"Popup static");
    require(popup.bounds&&*popup.bounds==take(f.popup.snapshot()).visual_bounds,"Popup crop differs from its visual bounds");
    require(!text_in(popup,"Outside"),"Popup selection captured unrelated source content");
    const auto anchor=take(f.popup_anchor.snapshot_scene());same_ink(full,anchor,"Popup body");
    require(!text_in(anchor,"Popup static"),"Popup body anchor captured unrelated chrome");
    const auto chrome=take(f.popup_chrome.snapshot_scene());same_ink(full,chrome,"Popup body");same_ink(full,chrome,"Popup static");
    require(take(f.popup_body.is_in_scene_of(f.popup_anchor))&&take(f.popup_body.is_in_scene_of(f.popup))&&
            !take(f.popup_body.is_in_scene_of(f.root))&&!take(f.popup.is_in_scene_of(f.host)),
            "Scene ancestry crossed a separately portaled popup");
    good(f.popup.close_popup());require(!f.popup_body.snapshot_scene(),"Retired popup body remained readable");
    good(f.menu.open_at(20,20));const auto menu_frame=f.frame();const auto first=take(f.first.snapshot_scene());
    same_ink(menu_frame,first,"First item");require(!text_in(first,"Second item")&&!text_in(first,"Outside"),"Menu item included siblings");
    const auto menu=take(f.menu.snapshot_scene());same_ink(menu_frame,menu,"First item");same_ink(menu_frame,menu,"Second item");
}
void motion(){
    using namespace std::chrono_literals;runtime::EventLoop events;runtime::FrameLoop frames(events);
    gui::detail::WidgetFrameHooks hooks;hooks.frames=[&]()->core::Result<runtime::FrameLoop*>{return &frames;};
    auto tree=std::make_shared<gui::detail::WidgetState>([](runtime::Task task){task();},gui::WidgetAppearance{},std::move(hooks));
    auto host=take(tree->content_host({},"host",{400,300}));gui::Widget popup,static_text,body_text;
    good(host.set_content([&](gui::WidgetBuilder& builder)->core::Result<void>{
        auto root=take(builder.vstack({},"root"));gui::PopoverOptions options{.width=160,.height=100,.clip_content=true};
        options.transition=gui::PopoverTransition{.enter_duration=400ms,.exit_duration=100ms,.collapsed_scale=.5};
        popup=take(builder.popover(root,"popup",options));auto chrome=take(builder.vstack(popup,"chrome"));
        static_text=take(builder.label(chrome,"static",{.text="Animated static"}));auto anchor=take(builder.vstack(chrome,"anchor"));
        return popup.set_popup_content_at(anchor,[&](gui::WidgetBuilder& child)->core::Result<void>{body_text=take(child.label({},"body",{.text="Animated body"}));return {};});
    }));
    good(popup.open_at(20,20));const auto anchor=runtime::Clock::now();(void)frames.step(anchor+120ms);
    const auto full=take(tree->drawing_snapshot({400,300}));const auto first=take(static_text.snapshot_scene());const auto second=take(body_text.snapshot_scene());
    same_ink(full,first,"Animated static");same_ink(full,second,"Animated body");
    require(first.bounds&&*first.bounds==take(static_text.snapshot()).visual_bounds,"Animated popup crop lost its transform");
    require(second.bounds&&*second.bounds==take(body_text.snapshot()).visual_bounds,"Animated embedded popup crop lost its transform");
    require(near(*first.bounds,transformed(take(static_text.snapshot()).bounds,text_in(first,"Animated static")->transform)),
            "Animated static crop does not follow its actual layer transform");
    require(near(*second.bounds,transformed(take(body_text.snapshot()).bounds,text_in(second,"Animated body")->transform)),
            "Animated embedded crop does not follow its actual layer transform");
    const auto transform=text_in(first,"Animated static")->transform;
    require(transform.scale>.5f&&transform.scale<1&&transform.opacity>0&&transform.opacity<1,"Popup transition was not captured at its displayed transform");
    tree->close();
}
void lifecycle(){
    Fixture f;auto selected=f.embedded;
    const auto other=std::async(std::launch::async,[&]{return selected.snapshot_scene();}).get();
    require(!other&&other.error().code==core::ErrorCode::wrong_executor,"Worker captured a native subtree");
    const auto relation=std::async(std::launch::async,[&]{return selected.is_in_scene_of(f.group);}).get();
    require(!relation&&relation.error().code==core::ErrorCode::wrong_executor,"Worker queried native scene ancestry");
    auto foreign=std::make_shared<gui::detail::WidgetState>([](runtime::Task task){task();});
    const auto foreign_root=take(foreign->content_host({},"foreign",{100,80}));
    require(!take(selected.is_in_scene_of(foreign_root))&&!take(foreign_root.is_in_scene_of(selected)),"Different widget domains shared a capture scene");
    foreign->close();
    good(f.slot.clear_content());const auto expired=selected.snapshot_scene();
    require(!expired&&expired.error().code==core::ErrorCode::closed,"Retired source captured its successor");
    require(!selected.is_in_scene_of(f.host)&&!f.inside.is_in_scene_of(selected),"Retired source remained in a capture relation");
    require(take(f.slot.snapshot_scene()).layers.empty(),"Empty content host emitted stale drawing");
}
void base_layer(){
    auto tree=std::make_shared<gui::detail::WidgetState>([](runtime::Task task){task();});
    gui::Widget root;
    good(tree->construct([&](gui::WidgetBuilder& builder)->core::Result<void>{
        root=take(builder.vstack({},"root"));
        (void)take(builder.label(root,"text",{.text="Base text"}));return {};
    }));
    const auto frame=take(tree->drawing_snapshot({400,300}));const auto scene=take(root.snapshot_scene());
    require(!frame.texts.empty()&&scene.first_is_base&&!scene.layers.empty()&&scene.layers.front().texts==frame.texts,
            "Source scene did not preserve the implicit base layer");
    tree->close();
}
void paint_isolation(){
    Fixture f;int paints=0;bool local_busy=false,embedded_busy=false;
    const auto image=take(gui::rgba_image(1,1,{255,0,0,255}));
    good(f.group.set_paint_handler([&](const gui::WidgetPaintState&)->core::Result<std::shared_ptr<const gui::WidgetDrawing>>{
        ++paints;const auto local=f.outside.snapshot_scene(),nested=f.embedded.snapshot_scene();
        local_busy=!local&&local.error().code==core::ErrorCode::busy;
        embedded_busy=!nested&&nested.error().code==core::ErrorCode::busy;
        auto result=std::make_shared<gui::WidgetDrawing>();
        result->panels.push_back({{0,0,10,10},{1,0,0,1},{1,0,0,1}});
        result->images.push_back({{0,0,5,5},image});
        result->backdrop=gui::BackdropMaterial{.bounds={0,0,10,10},.capture=false};
        result->shadow=gui::BoxShadow{.bounds={0,0,10,10},.blur_radius=0};
        return std::shared_ptr<const gui::WidgetDrawing>{std::move(result)};
    }));
    (void)f.frame();paints=0;
    const auto nested=take(f.embedded.snapshot_scene());require(paints==0&&text_in(nested,"Embedded"),"Capturing a descendant repainted its excluded ancestor");
    const auto selected=take(f.group.snapshot_scene());require(paints==1&&local_busy&&embedded_busy,"Paint provider accessed a widget scene during composition");
    const auto material=std::ranges::find_if(selected.layers,[](const auto& layer){return layer.backdrop.has_value();});
    require(material!=selected.layers.end()&&material->shadow&&!material->images.empty()&&material->images.front().image==image,
            "Selected custom drawing lost material, shadow or image");
}
void scope_isolation(){
    Fixture f;bool parent_busy=false;
    good(f.embedded_root.set_paint_handler([&](const gui::WidgetPaintState&)->core::Result<std::shared_ptr<const gui::WidgetDrawing>>{
        const auto parent=f.outside.snapshot_scene();parent_busy=!parent&&parent.error().code==core::ErrorCode::busy;
        return std::make_shared<const gui::WidgetDrawing>();
    }));
    (void)f.frame();const auto scene=take(f.embedded_root.snapshot_scene());
    require(parent_busy&&text_in(scene,"Embedded"),"A nested paint provider accessed a different generation of the same tree");
}
const gui::BackdropMaterial* material(const gui::CanvasFrame& frame,float red){
    for(const auto& layer:frame.overlays)if(layer.backdrop&&near(layer.backdrop->tint.red,red))return &*layer.backdrop;
    return nullptr;
}
const gui::BackdropMaterial* material(const gui::CanvasScene& frame,float red){
    for(const auto& layer:frame.layers)if(layer.backdrop&&near(layer.backdrop->tint.red,red))return &*layer.backdrop;
    return nullptr;
}
void automatic_sources(){
    auto tree=std::make_shared<gui::detail::WidgetState>([](runtime::Task task){task();});
    gui::Widget root,background,content,background_text,content_text,nested,glass,glass_child,slot,retired;
    good(tree->construct([&](gui::WidgetBuilder& b)->core::Result<void>{
        root=take(b.vstack({},"root",{.alignment=gui::StackAlignment::stretch}));
        background=take(b.vstack(root,"background",{.alignment=gui::StackAlignment::stretch}));
        background_text=take(b.label(background,"background-text",{.text="Background"}));
        nested=take(b.zstack(background,"nested"));
        content=take(b.vstack(root,"content",{.alignment=gui::StackAlignment::stretch}));
        content_text=take(b.label(content,"content-text",{.text="Content"}));
        glass=take(b.zstack(root,"glass"));glass_child=take(b.label(glass,"glass-child",{.text="Child"}));
        slot=take(b.content_host(root,"slot",{100,30}));
        return slot.set_content([&](gui::WidgetBuilder& child)->core::Result<void>{
            retired=take(child.label({},"retired",{.text="Retired"}));return {};
        });
    }));
    const auto paint=[&](gui::Widget widget,float tag){
        good(widget.set_extent({120,30}));gui::WidgetDrawing drawing;
        drawing.backdrop=gui::BackdropMaterial{.bounds={0,0,120,30},.tint={tag,0,0,.5f},.capture=false};
        good(widget.set_drawing(std::move(drawing)));
    };
    paint(nested,.1f);paint(content,.2f);paint(glass,.3f);
    good(nested.set_backdrop_sources({},content));
    good(glass.set_backdrop_sources(background,content));
    const auto first=take(tree->drawing_snapshot({400,300}));
    const auto outer=material(first,.3f);require(outer&&outer->background_scene&&outer->content_scene,"Material did not reference both source scenes");
    require(first.scenes.size()==2&&*outer->background_scene!=*outer->content_scene,"Scene sources were not deduplicated");
    const auto& bg=first.scenes[*outer->background_scene];const auto& fg=first.scenes[*outer->content_scene];
    require(text_in(bg,"Background")&&!text_in(bg,"Content")&&text_in(fg,"Content"),"Source captured an unrelated sibling");
    const auto inner=material(bg,.1f);require(inner&&inner->content_scene==outer->content_scene,"Nested material did not reuse its source scene");
    require(gui::valid_canvas(first)&&gui::plan_canvas_scenes(first),"Automatic source frame is not a valid scene graph");
    const auto graph=take(background.snapshot_scene_graph());const auto selected=material(graph.scene,.1f);
    require(selected&&selected->content_scene&&graph.scenes.size()==1&&
        text_in(graph.scenes[*selected->content_scene],"Content"),"Subtree scene graph lost its nested dependency");
    const auto flat=background.snapshot_scene();require(!flat&&flat.error().code==core::ErrorCode::unsupported,
        "Plain subtree snapshot silently discarded scene dependencies");
    good(content_text.set_text("Changed"));const auto changed=take(tree->drawing_snapshot({400,300}));
    const auto changed_outer=material(changed,.3f);require(changed_outer&&changed_outer->content_scene&&
        text_in(changed.scenes[*changed_outer->content_scene],"Changed")&&text_in(fg,"Content"),"Source changes did not produce an independent updated scene");
    require(!glass.set_backdrop_sources(root,{})&&!glass.set_backdrop_sources(glass_child,{}),"Self-containing source was accepted");
    require(!content.set_backdrop_sources({},background),"Indirect source cycle was accepted");
    const auto worker=std::async(std::launch::async,[&]{return glass.set_backdrop_sources(background,content);}).get();
    require(!worker&&worker.error().code==core::ErrorCode::wrong_executor,"A worker changed widget scene sources");
    auto foreign=std::make_shared<gui::detail::WidgetState>([](runtime::Task task){task();});
    auto foreign_root=take(foreign->content_host({},"foreign",{20,20}));
    require(!glass.set_backdrop_sources(foreign_root,{}),"Source from a different window was accepted");
    const auto preserved=take(tree->drawing_snapshot({400,300}));
    require(material(preserved,.3f)->background_scene&&material(preserved,.3f)->content_scene,"Rejected source changed the previous binding");
    good(glass.set_backdrop_sources(content,content));const auto shared=take(tree->drawing_snapshot({400,300}));
    const auto shared_material=material(shared,.3f);
    require(shared_material&&shared_material->background_scene==shared_material->content_scene&&shared.scenes.size()==1,
        "A source used in both material slots was rendered twice");
    good(content.set_visible(false));const auto hidden=take(tree->drawing_snapshot({400,300}));
    require(hidden.scenes.size()==1&&hidden.scenes.front().layers.empty(),"A hidden source retained old drawing commands");
    good(content.set_visible(true));good(glass.set_backdrop_sources(background,content));
    good(glass.set_backdrop_sources(retired,{}));good(slot.clear_content());
    good(slot.set_content([](gui::WidgetBuilder& child)->core::Result<void>{
        auto next=child.label({},"successor",{.text="Successor"});if(!next)return std::unexpected(next.error());return {};
    }));
    const auto after_retirement=take(tree->drawing_snapshot({400,300}));
    require(material(after_retirement,.3f)&&!material(after_retirement,.3f)->background_scene,"Retired source remained bound or targeted its successor");
    foreign->close();tree->close();
}
void staged_sources(){
    auto tree=std::make_shared<gui::detail::WidgetState>([](runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{200,100}));
    good(host.set_content([&](gui::WidgetBuilder& builder)->core::Result<void>{
        auto root=take(builder.zstack({},"root"));auto source=take(builder.label(root,"source",{.text="Staged"}));
        auto target=take(builder.zstack(root,"material"));good(target.set_extent({100,40}));
        gui::WidgetDrawing drawing;drawing.backdrop=gui::BackdropMaterial{.bounds={0,0,100,40},.tint={.4f,0,0,.5f},.capture=false};
        good(target.set_drawing(std::move(drawing)));return target.set_backdrop_sources(source,{});
    }));
    const auto frame=take(tree->drawing_snapshot({200,100}));const auto target=material(frame,.4f);
    require(target&&target->background_scene&&text_in(frame.scenes[*target->background_scene],"Staged"),
        "Staged sibling source was not committed with its content");
    tree->close();
}
void staged_external_sources(){
    auto tree=std::make_shared<gui::detail::WidgetState>([](runtime::Task task){task();});
    gui::Widget external,slot,glass,kept;
    good(tree->construct([&](gui::WidgetBuilder& builder)->core::Result<void>{
        auto root=take(builder.vstack({},"root"));external=take(builder.label(root,"external",{.text="External"}));
        slot=take(builder.content_host(root,"slot",{100,40}));glass=take(builder.zstack(root,"glass"));
        good(glass.set_extent({100,40}));gui::WidgetDrawing ink;
        ink.backdrop=gui::BackdropMaterial{.bounds={0,0,100,40},.tint={.7f,0,0,.5f},.capture=false};
        good(glass.set_drawing(std::move(ink)));return glass.set_backdrop_sources(slot,{});
    }));
    const auto factory=[&](gui::Widget selected,bool remember){return [&,selected,remember](gui::WidgetBuilder& builder)->core::Result<void>{
        auto material=take(builder.zstack({},"inner"));gui::WidgetDrawing ink;
        ink.backdrop=gui::BackdropMaterial{.bounds={0,0,100,40},.tint={.8f,0,0,.5f},.capture=false};
        good(material.set_drawing(std::move(ink)));auto result=material.set_backdrop_sources({},selected);
        if(result&&remember)kept=material;return result;
    };};
    good(slot.set_content(factory(external,true)));
    auto frame=take(tree->drawing_snapshot({200,150}));const auto outer=material(frame,.7f);
    require(outer&&outer->background_scene&&material(frame.scenes[*outer->background_scene],.8f),
        "Staged content could not bind an external sibling source");
    require(!slot.set_content(factory(glass,false)),"Cyclic staged replacement was accepted");
    require(bool(kept.snapshot()),"Rejected staged cycle retired the previous content");
    frame=take(tree->drawing_snapshot({200,150}));require(gui::valid_canvas(frame),"Rejected replacement damaged the active source graph");
    tree->close();
}
void popup_sources(){
    Fixture f;good(f.popup.open_at(10,10));gui::WidgetDrawing drawing;
    drawing.backdrop=gui::BackdropMaterial{.bounds={0,0,80,30},.tint={.6f,0,0,.5f},.capture=false};
    good(f.popup_chrome.set_drawing(std::move(drawing)));good(f.popup_chrome.set_backdrop_sources(f.host,{}));
    const auto frame=f.frame();const auto target=material(frame,.6f);
    require(target&&target->background_scene&&text_in(frame.scenes[*target->background_scene],"Inside")&&
        !text_in(frame.scenes[*target->background_scene],"Popup static"),"Popup source included its own portaled scene");
    good(f.inside.set_text("Live popup source"));const auto updated=f.frame();const auto changed=material(updated,.6f);
    require(changed&&changed->background_scene&&text_in(updated.scenes[*changed->background_scene],"Live popup source"),
        "Popup source did not track an underlying widget change");
}
void paint_failure(){
    Fixture f;
    good(f.group.set_paint_handler([](const gui::WidgetPaintState&)->core::Result<std::shared_ptr<const gui::WidgetDrawing>>{
        return core::fail(core::ErrorCode::failed,"Test paint failure");
    }));
    const auto failed=f.group.snapshot_scene();require(!failed&&failed.error().code==core::ErrorCode::failed,"Paint failure was not reported");
    good(f.outside.set_text("Recovered"));require(bool(text_in(take(f.outside.snapshot_scene()),"Recovered")),"A paint failure left the widget domain locked");
}
}
int main(){try{ordinary();popup();motion();lifecycle();base_layer();paint_isolation();scope_isolation();automatic_sources();staged_sources();staged_external_sources();popup_sources();paint_failure();std::cout<<"SUBTREE_SCENE_OK\n";}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
