#include "Text.h"
#include "NativeText.h"
#include <iiSDKCore/Core/Text/Utf8.h>

namespace ii::gui {
core::Result<TextBitmap> render_text(TextRun text,double scale){
    if(!ii::core::valid_utf8(text.text)||text.text.size()>65536||text.font_family.size()>1024||!ii::core::valid_utf8(text.font_family)||text.font_family.find('\0')!=text.font_family.npos)
        return core::fail(core::ErrorCode::invalid_argument,"Text rasterization requires bounded canonical UTF-8");
    if(!std::isfinite(scale)||scale<=0||scale>8)return core::fail(core::ErrorCode::invalid_argument,"Text scale must be finite and within (0,8]");
    const auto bounds=text.bounds;text.bounds.x=0;text.bounds.y=0;
    if(text.clip){text.clip->x-=bounds.x;text.clip->y-=bounds.y;}
    CanvasFrame frame;frame.viewport={0,0,bounds.width,bounds.height};frame.texts={std::move(text)};
    if(!valid_canvas(frame)||bounds.width*scale>8192||bounds.height*scale>8192)return core::fail(core::ErrorCode::invalid_argument,"Text bounds or paint are invalid or exceed 8192 pixels");
    try{auto pixels=detail::rasterize_text(frame,scale);return TextBitmap{pixels.width,pixels.height,std::move(pixels.rgba)};}
    catch(...){return core::fail(core::ErrorCode::failed,"Native text rasterization failed",std::current_exception());}
}
}
