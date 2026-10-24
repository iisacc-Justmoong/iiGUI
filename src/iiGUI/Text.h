#pragma once
#include "Graphics.h"
#include <iiSDKCore/Core/Result/Result.h>

namespace ii::gui {
struct TextBitmap final {
    std::uint32_t width=0,height=0;
    // Top-left, tightly packed, straight-alpha RGBA8 in encoded sRGB.
    std::vector<std::uint8_t> rgba;
};
struct TextMetrics final {double width=0,height=0;};
[[nodiscard]] IIGUI_EXPORT core::Result<TextMetrics> measure_text_layout(TextRun);
// Native shaping and glyph rasterization, without a window or browser surface.
// Call on the native UI owner while its application/font environment is alive.
[[nodiscard]] IIGUI_EXPORT core::Result<TextBitmap> render_text(TextRun,double scale=1);
}
