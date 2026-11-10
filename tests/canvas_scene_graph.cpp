#include <iiGUI/Graphics.h>
#include <iiGUI/NativeText.h>
#include <iiGUI/GpuEffects.h>
#include <iiGUI/Image.h>
#include <iiGUI/VulkanScenePlan.h>
#include <iiGUI/Direct3DScenePlan.h>
#include <iiGUI/WebGLScenePlan.h>
#include <algorithm>
#include <iostream>
#include <limits>
#include <stdexcept>

using namespace ii::gui;
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
CanvasLayer material(std::optional<std::size_t> background={},std::optional<std::size_t> content={}){
    CanvasLayer layer;
    layer.backdrop=BackdropMaterial{.bounds={0,0,50,50},.capture=false,
        .background_scene=background,.content_scene=content};
    return layer;
}
CanvasFrame graph(){
    CanvasFrame f{.viewport={0,0,200,100}};
    f.scenes.resize(4);
    for(auto& scene:f.scenes)scene.viewport=f.viewport;
    f.scenes[0].layers.push_back(material({},2));
    f.scenes[1].layers.push_back(material(2,{}));
    f.scenes[2].layers.push_back({.panels={{{0,0,10,10},{1,0,0,1},{1,0,0,1}}}});
    f.scenes[3].layers.push_back({.texts={{{0,0,10,10},"Unused",{1,1,1,1}}}});
    f.overlays.push_back(material(0,1));
    return f;
}
void ordering(){
    auto f=graph();const auto plan=plan_canvas_scenes(f);
    require(plan&&plan->count==3,"Scene plan did not exclude unused scenes or reuse a shared source");
    require(plan->order[0]==2&&plan->order[1]==0&&plan->order[2]==1,"Scene dependencies are not in compositing order");
    require(valid_canvas(f),"Valid scene DAG was rejected");
    f.overlays.push_back(material(1,0));const auto repeated=plan_canvas_scenes(f);
    require(repeated&&repeated->count==3&&repeated->order==plan->order,"Shared sources were scheduled more than once");
    auto empty=CanvasFrame{.viewport={0,0,200,100}};const auto no_sources=plan_canvas_scenes(empty);
    require(no_sources&&no_sources->count==0&&valid_canvas(empty),"Ordinary canvases changed");
}
void source_reuse(){
    auto frame=graph();const auto plan=plan_canvas_scenes(frame);
    require(bool(plan),"Missing scene dependency plan");
    std::array<bool,maximum_canvas_scenes> available{};
    auto dirty=detail::scene_rebuilds(frame,*plan,nullptr,available,false);
    require(dirty[0]&&dirty[1]&&dirty[2]&&!dirty[3],"First draw must build every reachable source");
    available[0]=available[1]=available[2]=true;
    const auto previous=frame;
    dirty=detail::scene_rebuilds(frame,*plan,&previous,available,false);
    require(!dirty[0]&&!dirty[1]&&!dirty[2],"Unchanged sources should retain their rendered textures");
    frame.background={.1f,.2f,.3f,1};
    dirty=detail::scene_rebuilds(frame,*plan,&previous,available,false);
    require(!dirty[0]&&!dirty[1]&&!dirty[2],"A window-only change must not rebuild source scenes");
    frame=previous;frame.scenes[0].bounds=Rect{0,0,30,30};
    dirty=detail::scene_rebuilds(frame,*plan,&previous,available,false);
    require(dirty[0]&&!dirty[1]&&!dirty[2],"Only the changed independent source should rebuild");
    frame=previous;frame.scenes[2].layers[0].panels[0].top={0,1,0,1};
    dirty=detail::scene_rebuilds(frame,*plan,&previous,available,false);
    require(dirty[2]&&dirty[0]&&dirty[1]&&!dirty[3],"Changed dependencies must invalidate their consumers");
    frame=previous;available[1]=false;
    dirty=detail::scene_rebuilds(frame,*plan,&previous,available,false);
    require(!dirty[0]&&dirty[1]&&!dirty[2],"An absent cached source must be rebuilt");
    available[1]=true;available[2]=false;
    dirty=detail::scene_rebuilds(frame,*plan,&previous,available,false);
    require(dirty[0]&&dirty[1]&&dirty[2],"An absent dependency must rebuild every consumer");
    available[2]=true;
    dirty=detail::scene_rebuilds(frame,*plan,&previous,available,true);
    require(dirty[0]&&dirty[1]&&dirty[2],"Scale or target changes must rebuild the graph");
}
void geometry_batches(){
    CanvasFrame frame{.viewport={0,0,200,100}};
    const Panel red{{0,0,10,10},{1,0,0,1},{1,0,0,1}};
    const Panel green{{10,0,10,10},{0,1,0,1},{0,1,0,1}};
    frame.panels.push_back(red);
    CanvasLayer first{.panels={red}},second{.panels={green}};
    CanvasLayer foreground{.texts={{{0,0,10,10},"text",{1,1,1,1}}}};
    CanvasLayer clipped{.panels={red},.clip=Rect{0,0,20,20}};
    CanvasLayer effect=material();
    frame.overlays={first,second,foreground,first,second,clipped,effect,first};
    const auto batches=detail::canvas_batch_layers(frame);
    require(batches.views.size()==7&&detail::canvas_batch_count(frame.overlays)==7,
            "Compatible adjacent panel layers were not batched");
    const auto first_batch=batches.panel_layers(1);
    require(batches.panel_count(0)==1&&batches.panel_count(1)==2&&first_batch.size()==2&&
            first_batch[0].panels[0]==red&&first_batch[1].panels[0]==green,
            "Panel batching changed the base boundary or drawing order");
    require(batches.views[2].foreground()&&batches.panel_count(3)==2&&
            batches.views[4].clip==clipped.clip&&batches.views[5].backdrop&&batches.views[6].panels.size()==1,
            "Batching crossed a foreground, clipping or material boundary");
    const auto packed=detail::pack_panel_batch(first_batch,batches.panel_count(1));
    require(packed.size()==2&&packed[0].top[0]==1&&packed[1].top[1]==1,
            "The packed instances changed painter order");
    const auto plan=detail::vulkan_scene_plan(frame,200,100);
    require(plan&&plan->passes.size()==1&&plan->passes[0].layer_count==batches.views.size()&&
            plan->layers==batches.views.size(),"Vulkan descriptor ranges did not use batched layer counts");
    detail::PanelKey combined;combined.assign(first_batch);
    CanvasLayer together{.panels={red,green}};
    const std::array combined_view{detail::canvas_layer_view(together)};
    require(combined.matches(combined_view),"Equivalent geometry with different segment boundaries was reuploaded");
    auto altered=first;altered.content_transform.offset_x=1;
    frame.overlays={first,altered};
    require(detail::canvas_batch_layers(frame).views.size()==3,"Different transforms were batched");
    auto current=detail::canvas_batch_layers(frame);
    detail::PanelKey key;
    require(!key.matches(current.panel_layers(0)),"An empty geometry cache matched");
    key.assign(current.panel_layers(0));
    require(key.matches(current.panel_layers(0)),"Unchanged geometry was not retained");
    frame.panels[0].top.green=.5f;
    require(!key.matches(current.panel_layers(0)),"Changed panel geometry or color was not detected");
    frame.panels[0]=red;
    require(!key.matches(current.panel_layers(2)),"Changed transforms were not detected");
    key.clear();require(!key.matches(current.panel_layers(0)),"Cleared geometry remained valid");
}
void empty_layers(){
    CanvasFrame frame{.viewport={0,0,200,100}};
    const Panel red{{0,0,10,10},{1,0,0,1},{1,0,0,1}};
    const Panel green{{10,0,10,10},{0,1,0,1},{0,1,0,1}};
    frame.panels.push_back(red);
    CanvasLayer first{.panels={red}},second{.panels={green}};
    CanvasLayer empty{.clip=Rect{100,20,10,10},.content_transform={.offset_x=7}};
    CanvasLayer foreground{.texts={{{0,0,10,10},"text",{1,1,1,1}}}};
    CanvasLayer clipped{.panels={red},.clip=Rect{0,0,20,20}};
    CanvasLayer effect=material();
    frame.overlays={empty,first,empty,second,foreground,empty,clipped,effect,empty,second};
    const auto batches=detail::canvas_batch_layers(frame);
    require(batches.views.size()==6&&detail::canvas_batch_count(frame.overlays)==6,
            "Empty layers created rendering batches or blocked compatible panels");
    const auto merged=batches.panel_layers(1);
    require(merged.size()==3&&batches.panel_count(1)==2&&merged[0].panels[0]==red&&
            merged[1].panels.empty()&&merged[2].panels[0]==green,
            "Skipping empty layers changed the panel sequence");
    const auto packed=detail::pack_panel_batch(merged,batches.panel_count(1));
    require(packed.size()==2&&packed[0].top[0]==1&&packed[1].top[1]==1,
            "Skipping empty layers changed GPU instance order");
    require(batches.views[2].foreground()&&batches.views[3].clip==clipped.clip&&
            batches.views[4].backdrop&&batches.views[5].panels.size()==1,
            "Skipping empty layers crossed a foreground, clip or effect boundary");
    const auto plan=detail::vulkan_scene_plan(frame,200,100);
    require(plan&&plan->passes.size()==1&&plan->passes[0].layer_count==batches.views.size(),
            "Scene planning did not use the drawable batch count");
    detail::PanelKey key;key.assign(merged);
    const CanvasLayer combined{.panels={red,green}};
    const std::array views{detail::canvas_layer_view(combined)};
    require(key.matches(views),"An empty layer invalidated retained GPU geometry");
    CanvasLayer shadow{.shadow=BoxShadow{.bounds={0,0,20,20},.blur_radius=2}};
    frame.overlays={first,empty,shadow,empty,second};
    const auto shadow_batches=detail::canvas_batch_layers(frame);
    require(shadow_batches.views.size()==4&&shadow_batches.views[2].shadow&&
            detail::canvas_batch_count(frame.overlays)==4,
            "A shadow-only layer was skipped or reordered across panels");
}
void invalid_graphs(){
    auto f=graph();f.scenes[2].layers.push_back(material({},0));
    require(!plan_canvas_scenes(f)&&!valid_canvas(f),"Indirect capture cycle was admitted");
    f=graph();f.scenes[3].layers.push_back(material(3,{}));
    require(!plan_canvas_scenes(f)&&!valid_canvas(f),"Cycle in an unused source was admitted");
    f=graph();f.overlays[0].backdrop->content_scene=4;
    require(!plan_canvas_scenes(f)&&!valid_canvas(f),"Missing root source was admitted");
    f=graph();f.scenes[3].layers.push_back(material({},99));
    require(!plan_canvas_scenes(f)&&!valid_canvas(f),"Missing nested source was admitted");
    f=graph();f.scenes.resize(maximum_canvas_scenes+1);
    require(!plan_canvas_scenes(f)&&!valid_canvas(f),"Unbounded source graph was admitted");
    f=graph();f.scenes[3].layers.resize(maximum_canvas_overlays+1);
    require(!plan_canvas_scenes(f),"Source planning traversed an unbounded layer list");
    f=graph();f.scenes.resize(9);for(auto& scene:f.scenes){scene.viewport=f.viewport;scene.layers.resize(maximum_canvas_overlays);}
    require(!plan_canvas_scenes(f)&&!valid_canvas(f),"Aggregate source layer limit was ignored");
}
void deep_graph(){
    CanvasFrame f{.viewport={0,0,200,100}};f.scenes.resize(maximum_canvas_scenes);
    for(std::size_t i=0;i<f.scenes.size();++i){f.scenes[i].viewport=f.viewport;
        if(i+1<f.scenes.size())f.scenes[i].layers.push_back(material({},i+1));}
    f.overlays.push_back(material({},0));const auto plan=plan_canvas_scenes(f);
    require(plan&&plan->count==maximum_canvas_scenes&&valid_canvas(f),"Maximum bounded chain was rejected");
    for(std::size_t i=0;i<plan->count;++i)require(plan->order[i]==plan->count-i-1,"Deep source dependencies were reordered");
    f.scenes.back().layers.push_back(material(0,{}));
    require(!plan_canvas_scenes(f),"Maximum-depth capture cycle was admitted");
}
void input_validation(){
    auto f=graph();f.scenes[0].bounds=Rect{0,0,0,0};
    require(valid_canvas(f),"An empty source crop must remain valid");
    f.scenes[0].bounds=Rect{0,0,-1,20};require(!valid_canvas(f),"Negative source crop was admitted");
    f=graph();f.scenes[0].bounds=Rect{0,0,std::numeric_limits<float>::quiet_NaN(),20};
    require(!valid_canvas(f),"Nonfinite source crop was admitted");
    f=graph();f.scenes[0].viewport.width=201;require(!valid_canvas(f),"Source viewport differs from the window");
    f=graph();f.scenes[0].layers[0].backdrop->blur_radius=65;
    require(!valid_canvas(f),"Nested material bypassed regular validation");
    f=graph();f.scenes[3].layers.resize(maximum_canvas_overlays+1);
    require(!valid_canvas(f),"Unbounded source layers were admitted");
    f=graph();f.scenes[3].layers[0].texts.resize(4097,f.scenes[3].layers[0].texts[0]);
    require(!valid_canvas(f),"Nested text bypassed aggregate limits");
    f=graph();f.scenes[2].first_is_base=true;require(valid_canvas(f),"Valid source base layer was rejected");
    f.scenes[2].layers[0].clip=Rect{0,0,10,10};require(!valid_canvas(f),"Base layer with its own clip was admitted");
    f=graph();f.scenes[3].layers.clear();f.scenes[3].first_is_base=true;
    require(!valid_canvas(f),"Missing source base layer was admitted");
    f=graph();f.background.alpha=0;require(!valid_canvas(f),"A source graph changed the opaque window contract");
}
void source_render_inputs(){
    auto f=graph();auto& source=f.scenes[2];source.first_is_base=true;
    source.layers.push_back(material());
    const auto rendered=detail::canvas_scene_frame(source);
    require(rendered.viewport==f.viewport&&rendered.background.alpha==0,"Source render target did not start transparent");
    require(rendered.panels==source.layers[0].panels&&rendered.overlays.size()==1&&rendered.overlays[0]==source.layers[1],
            "Source base and overlay order changed during preparation");
    source.first_is_base=false;const auto all_overlays=detail::canvas_scene_frame(source);
    require(all_overlays.panels.empty()&&all_overlays.overlays==source.layers,"A source without a base gained base content");
    detail::CanvasLayerView view{};view.clip=Rect{10,10,40,20};
    const auto cropped=detail::scene_clip_pixels(view,Rect{30,0,30,40},2,400,200);
    require(cropped.x==60&&cropped.y==20&&cropped.width==40&&cropped.height==40,"Source crop did not intersect the transformed layer clip");
    view.content_transform={.origin_x=0,.origin_y=0,.scale=.5f,.offset_x=8,.offset_y=4};
    const auto transformed=detail::scene_clip_pixels(view,Rect{10,0,30,40},2,400,200);
    require(transformed.x==26&&transformed.y==18&&transformed.width==40&&transformed.height==20,"Source crop was incorrectly transformed with the child");
    const auto empty=detail::scene_clip_pixels(view,Rect{1.25f,0,0,10},2,400,200);
    require(empty.width==0&&empty.height==0,"Fractional empty source crop painted pixels");
    view.clip=Rect{1.25f,0,0,10};
    const auto empty_child=detail::scene_clip_pixels(view,{},2,400,200);
    require(empty_child.width==0&&empty_child.height==0,"Fractional empty layer clip painted pixels");
}
void effect_budget(){
    auto f=graph();const auto plan=plan_canvas_scenes(f);require(bool(plan),"Missing budget plan");
    std::vector<CanvasFrame> sources(f.scenes.size());
    for(std::size_t i=0;i<plan->count;++i)sources[plan->order[i]]=detail::canvas_scene_frame(f.scenes[plan->order[i]]);
    auto usage=detail::scene_graph_requirements(f,*plan,sources,1,200,100);
    require(usage&&usage->scene_bytes==240000&&usage->backdrop_bytes==30000&&usage->shadow_bytes==0&&usage->asset_bytes==4,
            "Scene graph budget did not count shared targets and material crops");
    auto image=ii::gui::rgba_image(1,1,{255,255,255,128});require(bool(image),"Cannot make test image");
    f.overlays[0].backdrop->mask=*image;f.overlays[0].shadow=BoxShadow{.bounds={0,0,20,10},.blur_radius=3,.mask=*image};
    sources[0].overlays[0].backdrop->underlay=*image;sources[0].overlays[0].backdrop->underlay_bounds={0,0,20,20};
    usage=detail::scene_graph_requirements(f,*plan,sources,1,200,100);
    require(usage&&usage->shadow_bytes==1248&&usage->asset_bytes==8,
            "Shared mask/underlay was counted more than once or shadow padding was lost");
    f.overlays[0].backdrop->blur_radius=3;
    usage=detail::scene_graph_requirements(f,*plan,sources,1,200,100);
    require(usage&&usage->backdrop_bytes==53708,"Blurred crop budget ignored viewport clipping or its linear target");
    auto huge=graph();huge.viewport={0,0,8192,8192};for(auto& scene:huge.scenes)scene.viewport=huge.viewport;
    const auto huge_plan=plan_canvas_scenes(huge);std::vector<CanvasFrame> huge_sources(huge.scenes.size());
    for(std::size_t i=0;i<huge_plan->count;++i)huge_sources[huge_plan->order[i]]=detail::canvas_scene_frame(huge.scenes[huge_plan->order[i]]);
    const auto huge_usage=detail::scene_graph_requirements(huge,*huge_plan,huge_sources,1,8192,8192);
    require(huge_usage&&huge_usage->total_bytes()>detail::effect_texture_budget(8192,8192),"Large scene graph bypassed the effect budget");
    huge.overlays[0].shadow=BoxShadow{.bounds={0,0,8192,20},.blur_radius=64};
    require(!detail::scene_graph_requirements(huge,*huge_plan,huge_sources,1,8192,8192),"Oversized shadow target was admitted");
}
void vulkan_graph(){
    require(detail::vulkan_descriptor_capacity(0,8)==maximum_canvas_overlays+1,
            "Vulkan omitted its ordinary-frame descriptor reserve");
    require(detail::vulkan_descriptor_capacity(3000,8)==maximum_canvas_overlays+1,
            "Vulkan retained an oversized descriptor pool after a graph was removed");
    require(detail::vulkan_descriptor_capacity(600,550)==600&&detail::vulkan_descriptor_capacity(600,800)==800,
            "Vulkan descriptor pool reuse/growth policy is incorrect");
    auto f=graph();const auto plan=detail::vulkan_scene_plan(f,200,100);
    require(plan&&plan->passes.size()==4&&plan->layers==8,"Vulkan did not reserve separate layers for every scene");
    const std::optional<std::size_t> expected[]{2,0,1,{}};
    for(std::size_t i=0;i<plan->passes.size();++i){
        require(plan->passes[i].source==expected[i]&&plan->passes[i].first_layer==i*2&&plan->passes[i].layer_count==2,
                "Vulkan pass order or descriptor ranges overlap");
    }
    require(plan->scene_bytes==240000&&plan->snapshot_bytes==160000&&plan->asset_bytes==4,
            "Vulkan source, shared snapshot or transparent image budget is incorrect");
    require(detail::vulkan_scene_budget_ok(*plan,200,100),"A small Vulkan scene graph exceeded the budget");
    f.scenes[2].first_is_base=true;const auto base=detail::vulkan_scene_plan(f,200,100);
    require(base&&base->passes[0].layer_count==1&&base->passes[1].first_layer==1&&base->layers==7,
            "Vulkan treated a selected base as an extra overlay");
    f.scenes[2].first_is_base=false;
    auto image=rgba_image(1,1,{255,255,255,128});require(bool(image),"Cannot make Vulkan mask");
    f.scenes[0].layers[0].backdrop->underlay=*image;f.scenes[0].layers[0].backdrop->underlay_bounds={0,0,20,20};
    f.overlays[0].backdrop->mask=*image;f.overlays[0].shadow=BoxShadow{.bounds={0,0,10,10},.mask=*image};
    const auto shared=detail::vulkan_scene_plan(f,200,100);
    require(shared&&shared->asset_bytes==8&&shared->assets.size()==2,"Vulkan source graph duplicated shared images");
    auto& m=*f.overlays[0].backdrop;m.capture=true;m.underlay=*image;m.underlay_bounds=m.bounds;m.underlay_tint=Color{1,0,0,1};
    const auto flags=detail::effect_parameters(detail::canvas_layers(f)[1],f.viewport,1,false);
    require(flags.style[3]==126,"Vulkan material lost background/content source flags");
    const auto shadow=detail::effect_parameters(detail::canvas_layers(f)[1],f.viewport,1,true);
    require(shadow.style[3]==3,"Vulkan shadow inherited material source flags");
    const auto oversized=detail::vulkan_scene_plan(f,8192,8192);
    require(oversized&&!detail::vulkan_scene_budget_ok(*oversized,8192,8192),"Vulkan accepted oversized graph attachments");
    CanvasFrame ordinary{.viewport={0,0,8192,8192}};ordinary.overlays.push_back(material());
    const auto ordinary_plan=detail::vulkan_scene_plan(ordinary,8192,8192);
    require(ordinary_plan&&detail::vulkan_scene_budget_ok(*ordinary_plan,8192,8192),"Ordinary Vulkan frame budget changed");
    f.scenes[2].layers.push_back(material({},0));
    require(!detail::vulkan_scene_plan(f,200,100),"Vulkan prepared a cyclic source graph");
}
void direct3d_graph(){
    auto f=graph();const auto plan=detail::direct3d_scene_plan(f,200,100);
    require(plan&&plan->order.count==3&&plan->order.order[0]==2&&plan->order.order[1]==0&&plan->order.order[2]==1,
        "Direct3D scene passes are not dependency ordered");
    require(plan->scene_bytes==240000&&plan->snapshot_bytes==160000&&plan->asset_bytes==0&&plan->effects&&!plan->captures,
        "Direct3D omitted source targets or shared snapshots from its budget");
    require(detail::direct3d_scene_budget_ok(*plan,200,100),"A small Direct3D graph exceeded the budget");
    auto image=rgba_image(1,1,{255,255,255,128});require(bool(image),"Cannot make Direct3D mask");
    f.scenes[0].layers[0].backdrop->underlay=*image;f.scenes[0].layers[0].backdrop->underlay_bounds={0,0,20,20};
    f.overlays[0].backdrop->mask=*image;f.overlays[0].backdrop->capture=true;
    f.overlays[0].shadow=BoxShadow{.bounds={0,0,10,10},.mask=*image};
    const auto shared=detail::direct3d_scene_plan(f,200,100);
    require(shared&&shared->asset_bytes==4&&shared->assets.size()==1&&shared->captures,
        "Direct3D duplicated shared assets or lost its capture flag");
    auto huge=detail::direct3d_scene_plan(f,8192,8192);
    require(huge&&!detail::direct3d_scene_budget_ok(*huge,8192,8192),"Direct3D accepted oversized source attachments");
    CanvasFrame ordinary{.viewport={0,0,8192,8192}};ordinary.overlays.push_back(material());
    const auto plain=detail::direct3d_scene_plan(ordinary,8192,8192);
    require(plain&&detail::direct3d_scene_budget_ok(*plain,8192,8192),"Direct3D changed the ordinary asset budget");
    f.scenes[2].layers.push_back(material({},0));
    require(!detail::direct3d_scene_plan(f,200,100),"Direct3D admitted a cyclic source graph");
}
void webgl_graph(){
    auto f=graph();
    const auto half=detail::webgl_scene_plan(f,200,100,true);
    require(half&&half->order.count==3&&half->order.order[0]==2&&half->order.order[1]==0&&half->order.order[2]==1,
        "WebGL source scenes were not scheduled in dependency order");
    require(half->scene_bytes==480000&&half->snapshot_bytes==0&&half->asset_bytes==4&&half->effects&&!half->captures,
        "WebGL allocated snapshots for materials that do not capture their surroundings");
    const auto byte=detail::webgl_scene_plan(f,200,100,false);
    require(byte&&byte->scene_bytes==240000&&byte->snapshot_bytes==0&&detail::webgl_scene_budget_ok(*byte,200,100),
        "WebGL RGBA8 fallback used incorrect texture accounting");
    auto image=rgba_image(1,1,{255,255,255,128});require(bool(image),"Cannot make WebGL test mask");
    f.scenes[0].layers[0].backdrop->underlay=*image;f.scenes[0].layers[0].backdrop->underlay_bounds={0,0,20,20};
    f.overlays[0].backdrop->mask=*image;f.overlays[0].backdrop->capture=true;
    f.overlays[0].shadow=BoxShadow{.bounds={0,0,10,10},.mask=*image};
    const auto shared=detail::webgl_scene_plan(f,200,100,true);
    require(shared&&shared->asset_bytes==8&&shared->assets.size()==1&&shared->captures&&shared->snapshot_bytes==160000,
        "WebGL did not share effect images or reserve exactly one content snapshot");
    f.scenes[0].layers[0].backdrop->capture=true;f.scenes[0].layers[0].backdrop->source=BackdropSource::previous_layers;
    const auto both=detail::webgl_scene_plan(f,200,100,true);
    require(both&&both->snapshot_bytes==320000,"WebGL omitted the second snapshot when both source types were requested");
    const auto huge=detail::webgl_scene_plan(f,8192,8192,true);
    require(huge&&!detail::webgl_scene_budget_ok(*huge,8192,8192),"WebGL accepted oversized scene targets");
    CanvasFrame ordinary{.viewport={0,0,200,100}};ordinary.overlays.push_back(material());
    const auto plain=detail::webgl_scene_plan(ordinary,200,100,true);
    require(plain&&detail::webgl_scene_budget_ok(*plain,200,100),"WebGL rejected a small ordinary effect frame");
    ordinary.overlays[0].backdrop->capture=true;
    const auto huge_plain=detail::webgl_scene_plan(ordinary,8192,8192,true);
    require(huge_plain&&!detail::webgl_scene_budget_ok(*huge_plain,8192,8192),
        "WebGL accepted oversized snapshots without a scene graph");
    f.scenes[2].layers.push_back(material({},0));
    require(!detail::webgl_scene_plan(f,200,100,false),"WebGL admitted a cyclic source graph");
}
}
int main(){try{ordering();source_reuse();geometry_batches();empty_layers();invalid_graphs();deep_graph();input_validation();source_render_inputs();effect_budget();vulkan_graph();direct3d_graph();webgl_graph();std::cout<<"CANVAS_SCENE_GRAPH_OK\n";}
catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
