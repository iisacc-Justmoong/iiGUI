#pragma once
#include <iiGUI/Graphics.h>
#include <iiGUI/Image.h>
#include <iiGUI/NativeText.h>
#include <algorithm>
#include <optional>
#include <vector>

namespace ii::gui::detail {
// CPU-only admission plan. The D3D11 renderer owns the actual resources and
// performs its passes in dependency order on one immediate context.
struct Direct3DScenePlan final {
    CanvasScenePlan order;
    bool effects=false,captures=false;
    std::uint64_t scene_bytes=0,snapshot_bytes=0,asset_bytes=0;
    std::vector<std::shared_ptr<const RasterImage>> assets;
    [[nodiscard]] std::uint64_t effect_bytes()const noexcept{return scene_bytes+snapshot_bytes+asset_bytes;}
};
inline std::optional<Direct3DScenePlan> direct3d_scene_plan(const CanvasFrame& frame,std::uint32_t width,std::uint32_t height){
    if(width<1||height<1||width>8192||height>8192||!valid_canvas(frame))return std::nullopt;
    const auto order=plan_canvas_scenes(frame);if(!order)return std::nullopt;
    Direct3DScenePlan result;result.order=*order;
    const auto add=[&](const auto& image){if(image&&std::ranges::find(result.assets,image)==result.assets.end())result.assets.push_back(image);};
    const auto account=[&](const auto& layers){for(const auto& layer:layers){
        if(layer.backdrop){result.effects=true;result.captures|=layer.backdrop->capture;add(layer.backdrop->mask);add(layer.backdrop->underlay);}
        if(layer.shadow){result.effects=true;add(layer.shadow->mask);}
    }};
    for(std::size_t i=0;i<order->count;++i)account(frame.scenes[order->order[i]].layers);
    account(frame.overlays);
    result.scene_bytes=std::uint64_t(width)*height*4*order->count;
    result.snapshot_bytes=result.effects?std::uint64_t(width)*height*8:0;
    for(const auto& image:result.assets)result.asset_bytes+=image->rgba().size();
    return result;
}
inline bool direct3d_scene_budget_ok(const Direct3DScenePlan& plan,std::uint32_t width,std::uint32_t height)noexcept {
    // Preserve the existing ordinary-frame asset cap; a source graph also
    // accounts for every retained transparent target and shared snapshot.
    return (plan.order.count?plan.effect_bytes():plan.asset_bytes)<=effect_texture_budget(width,height);
}
} // namespace ii::gui::detail
