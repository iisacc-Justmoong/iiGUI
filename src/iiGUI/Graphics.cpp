#include <iiGUI/Graphics.h>
#include "Image.h"
#include <algorithm>
#include <cmath>
#include <iiSDKCore/Core/Text/Utf8.h>

namespace ii::gui {
bool valid_canvas(const CanvasFrame& frame) noexcept {
    const auto color = [](Color c) {
        for (float v : {c.red, c.green, c.blue, c.alpha})
            if (!std::isfinite(v) || v < 0 || v > 1) return false;
        return true;
    };
    const auto rect = [](Rect r) {
        return std::isfinite(r.x) && std::isfinite(r.y) && std::abs(r.x) <= 32767 &&
            std::abs(r.y) <= 32767 && std::isfinite(r.width) && std::isfinite(r.height) &&
            r.width > 0 && r.height > 0 && r.width <= 8192 && r.height <= 8192;
    };
    if (!rect(frame.viewport) || !color(frame.background) || frame.background.alpha != 1 ||
        frame.panels.size() > 4096 ||
        (frame.renderer!=Renderer::automatic && frame.renderer!=Renderer::native && frame.renderer!=Renderer::vulkan) ||
        (frame.acceleration != Acceleration::prefer_hardware && frame.acceleration != Acceleration::require_hardware))
        return false;
    if(frame.overlays.size()>64) return false;
    std::size_t panels=0,texts=0,images=0,bytes=0;
    const auto layer=[&](const auto& ps,const auto& ts,const auto& is) {
        panels+=ps.size();texts+=ts.size();images+=is.size();
        if(ps.size()>4096 || panels>16384 || texts>4096 || images>4096) return false;
        for(const auto& panel:ps)
            if(!rect(panel.bounds) || !color(panel.top) || !color(panel.bottom) || !std::isfinite(panel.radius) || panel.radius<0 || panel.radius>std::min(panel.bounds.width,panel.bounds.height)*.5f || !std::isfinite(panel.stroke_width) || panel.stroke_width<0 || panel.stroke_width>std::min(panel.bounds.width,panel.bounds.height)*.5f) return false;
        for(const auto& text:ts) {
            bytes+=text.text.size();
            if(text.clip){const auto r=*text.clip;if(!std::isfinite(r.x)||!std::isfinite(r.y)||!std::isfinite(r.width)||!std::isfinite(r.height)||std::abs(r.x)>32767||std::abs(r.y)>32767||r.width<0||r.height<0||r.width>8192||r.height>8192)return false;}
            if(!rect(text.bounds) || !color(text.color) || !std::isfinite(text.font_size) || text.font_size<1 || text.font_size>256 || !std::isfinite(text.line_height) || text.line_height<0 || text.line_height>8192 ||
                (text.font_weight!=400 && text.font_weight!=500 && text.font_weight!=600 && text.font_weight!=700) ||
                (text.alignment!=TextAlignment::left && text.alignment!=TextAlignment::center && text.alignment!=TextAlignment::right) ||
                text.text.size()>65536 || bytes>1024*1024 || text.text.find('\0')!=text.text.npos || !core::valid_utf8(text.text) ||
                text.font_family.size()>256 || text.font_family.find('\0')!=text.font_family.npos || !core::valid_utf8(text.font_family)) return false;
        }
        for(const auto& image:is)
            if(!rect(image.bounds) || !image.image || !std::isfinite(image.opacity) || image.opacity<0 || image.opacity>1 || (image.tint && !color(*image.tint))) return false;
        return true;
    };
    if(!layer(frame.panels,frame.texts,frame.images)) return false;
    for(const auto& overlay:frame.overlays) {
        const auto& t=overlay.content_transform;
        if(!std::isfinite(t.origin_x)||!std::isfinite(t.origin_y)||std::abs(t.origin_x)>32767||std::abs(t.origin_y)>32767||!std::isfinite(t.scale)||t.scale<1.f/1024||t.scale>4||!std::isfinite(t.opacity)||t.opacity<0||t.opacity>1||!std::isfinite(t.offset_x)||!std::isfinite(t.offset_y)||std::abs(t.offset_x)>32767||std::abs(t.offset_y)>32767)return false;
        if(overlay.shadow){const auto& s=*overlay.shadow;if(!rect(s.bounds)||!std::isfinite(s.radius)||s.radius<0||s.radius>std::min(s.bounds.width,s.bounds.height)/2||!std::isfinite(s.blur_radius)||s.blur_radius<0||s.blur_radius>64||!color(s.color))return false;}
        if(overlay.backdrop){const auto& m=*overlay.backdrop;if(!rect(m.bounds)||!std::isfinite(m.radius)||m.radius<0||m.radius>std::min(m.bounds.width,m.bounds.height)/2||!std::isfinite(m.blur_radius)||m.blur_radius<0||m.blur_radius>64||!std::isfinite(m.border_width)||m.border_width<0||m.border_width>8192||!color(m.tint)||!color(m.border)||static_cast<unsigned>(m.source)>1)return false;}
        if(overlay.clip) {
            auto r=*overlay.clip;
            if(!std::isfinite(r.x) || !std::isfinite(r.y) || !std::isfinite(r.width) || !std::isfinite(r.height) ||
                std::abs(r.x)>32767 || std::abs(r.y)>32767 || r.width<0 || r.height<0 || r.width>8192 || r.height>8192) return false;
        }
        if(!layer(overlay.panels,overlay.texts,overlay.images)) return false;
    }
    return true;
}
} // namespace ii::gui
