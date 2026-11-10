#pragma once
#include <iiGUI/Graphics.h>
#include <iiGUI/Image.h>
#include <iiGUI/NativeText.h>
#include <algorithm>
#include <optional>
#include <vector>

namespace ii::gui::detail {
// One immutable descriptor range per pass prevents descriptor updates for a
// later source from changing commands already recorded for an earlier source.
struct VulkanScenePass final {
    std::optional<std::size_t> source; // Empty denotes the presentable frame.
    std::size_t first_layer=0,layer_count=0;
};
struct VulkanScenePlan final {
    std::vector<VulkanScenePass> passes;
    std::size_t layers=0;
    std::uint64_t scene_bytes=0,snapshot_bytes=0,asset_bytes=0;
    bool effects=false;
    std::vector<std::shared_ptr<const RasterImage>> assets;
    [[nodiscard]] std::uint64_t effect_bytes()const noexcept{return scene_bytes+snapshot_bytes+asset_bytes;}
};
inline std::size_t vulkan_descriptor_capacity(std::size_t current,std::size_t needed)noexcept {
    const auto desired=std::max(maximum_canvas_overlays+1,needed);
    return current>=desired&&current-desired<=desired?current:desired;
}

// This is pure CPU planning; Vulkan handles, memory and synchronization stay
// inside the renderer. The shared snapshot images are reused only in command
// order, while every pass receives its own descriptor sets and target image.
inline std::optional<VulkanScenePlan> vulkan_scene_plan(const CanvasFrame& frame,std::uint32_t width,std::uint32_t height){
    if(width<1||height<1||width>8192||height>8192||!valid_canvas(frame))return std::nullopt;
    const auto order=plan_canvas_scenes(frame);if(!order)return std::nullopt;
    VulkanScenePlan result;
    const auto add_asset=[&](const auto& image){if(image&&std::ranges::find(result.assets,image)==result.assets.end())result.assets.push_back(image);};
    const auto account=[&](const auto& layers){for(const auto& layer:layers){
        if(layer.backdrop){result.effects=true;add_asset(layer.backdrop->mask);add_asset(layer.backdrop->underlay);}
        if(layer.shadow){result.effects=true;add_asset(layer.shadow->mask);}
    }};
    for(std::size_t i=0;i<order->count;++i){
        const auto index=order->order[i];const auto& scene=frame.scenes[index];
        const auto overlays=std::span<const CanvasLayer>(scene.layers).subspan(scene.first_is_base?1:0);
        const auto count=canvas_batch_count(overlays);
        result.passes.push_back({index,result.layers,count});result.layers+=count;account(scene.layers);
    }
    const auto root_count=canvas_batch_count(frame.overlays);
    result.passes.push_back({{},result.layers,root_count});result.layers+=root_count;account(frame.overlays);
    result.scene_bytes=std::uint64_t(width)*height*4*order->count;
    result.snapshot_bytes=result.effects?std::uint64_t(width)*height*8:0;
    if(result.effects)result.assets.push_back({});
    for(const auto& image:result.assets)result.asset_bytes+=image?image->rgba().size():4;
    return result;
}
inline bool vulkan_scene_budget_ok(const VulkanScenePlan& plan,std::uint32_t width,std::uint32_t height)noexcept {
    // Ordinary frames retain their existing image-asset budget. A source graph
    // additionally counts every transparent target and the shared snapshots.
    return (plan.passes.size()>1?plan.effect_bytes():plan.asset_bytes)<=effect_texture_budget(width,height);
}
} // namespace ii::gui::detail
