#pragma once

#include <iiGUI/Graphics.h>
#include <array>

namespace ii::gui::detail {
enum class ForegroundPass { text,images };
[[nodiscard]] constexpr std::array<ForegroundPass,2> foreground_order(bool images_before_text) noexcept {
    return images_before_text ? std::array{ForegroundPass::images,ForegroundPass::text}
                              : std::array{ForegroundPass::text,ForegroundPass::images};
}

// Metal's packed constant layout: all entries have explicit 16-byte alignment.
struct alignas(16) GpuImageUniform final {
    float bounds[4],source[4],tint[4],border[4];
    float top[4],middle[4],bottom[4],style[4],viewport[4];
};
static_assert(sizeof(GpuImageUniform)==144);
[[nodiscard]] inline GpuImageUniform gpu_image_uniform(const ImageRun& image,const ContentTransform& transform,
                                                       double scale,Rect viewport) noexcept {
    const auto factor=float(scale)*transform.scale;
    const auto x=(transform.origin_x+(image.bounds.x-transform.origin_x)*transform.scale+transform.offset_x)*float(scale);
    const auto y=(transform.origin_y+(image.bounds.y-transform.origin_y)*transform.scale+transform.offset_y)*float(scale);
    const auto tint=image.tint.value_or(Color{0,0,0,0});
    const auto gradient=image.gradient.value_or(ImageGradient{});
    const auto flag=float((image.tint?1:0)|(image.gradient?2:0));
    return {{x,y,image.bounds.width*factor,image.bounds.height*factor},
            {image.source.x,image.source.y,image.source.width,image.source.height},
            {tint.red,tint.green,tint.blue,tint.alpha},
            {image.border.red,image.border.green,image.border.blue,image.border.alpha},
            {gradient.top.red,gradient.top.green,gradient.top.blue,gradient.top.alpha},
            {gradient.middle.red,gradient.middle.green,gradient.middle.blue,gradient.middle.alpha},
            {gradient.bottom.red,gradient.bottom.green,gradient.bottom.blue,gradient.bottom.alpha},
            {image.radius*factor,image.border_width*factor,image.opacity*transform.opacity,flag},
            {viewport.width*float(scale),viewport.height*float(scale),gradient.height*factor,gradient.middle_position}};
}
} // namespace ii::gui::detail
