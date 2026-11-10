#pragma once
#include <iiGUI/Graphics.h>
#include <iiGUI/Image.h>
#include <iiGUI/NativeText.h>
#include <algorithm>
#include <optional>
#include <vector>

namespace ii::gui::detail {
// WebGL owns its framebuffer resources. This plan accounts only the reachable
// scene targets, the shared snapshots and distinct immutable effect images.
struct WebGLScenePlan final {
    CanvasScenePlan order;
    bool effects=false,captures=false,content_snapshot=false,previous_snapshot=false;
    std::uint64_t scene_bytes=0,snapshot_bytes=0,asset_bytes=0;
    std::vector<std::shared_ptr<const RasterImage>> assets;
    [[nodiscard]] std::uint64_t effect_bytes()const noexcept{return scene_bytes+snapshot_bytes+asset_bytes;}
};
inline std::optional<WebGLScenePlan> webgl_scene_plan(const CanvasFrame& frame,std::uint32_t width,std::uint32_t height,bool half){
    if(width<1||height<1||width>8192||height>8192||!valid_canvas(frame))return std::nullopt;
    const auto order=plan_canvas_scenes(frame);if(!order)return std::nullopt;
    WebGLScenePlan result;result.order=*order;
    const auto add=[&](const auto& image){if(image&&std::ranges::find(result.assets,image)==result.assets.end())result.assets.push_back(image);};
    const auto account=[&](const auto& layers){for(const auto& layer:layers){
        if(layer.backdrop){
            result.effects=true;result.captures|=layer.backdrop->capture;
            if(layer.backdrop->capture){
                if(layer.backdrop->source==BackdropSource::content)result.content_snapshot=true;
                else result.previous_snapshot=true;
            }
            add(layer.backdrop->mask);add(layer.backdrop->underlay);
        }
        if(layer.shadow){result.effects=true;add(layer.shadow->mask);}
    }};
    for(std::size_t i=0;i<order->count;++i)account(frame.scenes[order->order[i]].layers);
    account(frame.overlays);
    const auto bytes=std::uint64_t(width)*height*(half?8:4);
    result.scene_bytes=bytes*order->count;
    result.snapshot_bytes=bytes*(unsigned(result.content_snapshot)+unsigned(result.previous_snapshot));
    result.asset_bytes=result.effects?4:0; // Complete transparent texture for unused samplers.
    for(const auto& image:result.assets)result.asset_bytes+=image->rgba().size();
    return result;
}
inline bool webgl_scene_budget_ok(const WebGLScenePlan& plan,std::uint32_t width,std::uint32_t height)noexcept{
    // This backend previously rejected effects entirely, so bound snapshots
    // for ordinary material frames as well as for explicit source graphs.
    return plan.effect_bytes()<=effect_texture_budget(width,height);
}
} // namespace ii::gui::detail
