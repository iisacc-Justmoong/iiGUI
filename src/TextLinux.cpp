#include "NativeText.h"
#include <pango/pangocairo.h>
#include <memory>

namespace ii::gui::detail {
namespace {
using Surface=std::unique_ptr<cairo_surface_t,decltype(&cairo_surface_destroy)>;
using Drawing=std::unique_ptr<cairo_t,decltype(&cairo_destroy)>;
using Layout=std::unique_ptr<PangoLayout,decltype(&g_object_unref)>;
Layout layout(cairo_t* drawing,const TextRun& run) {
    Layout result(pango_cairo_create_layout(drawing),g_object_unref);
    auto font=pango_font_description_new();
    pango_font_description_set_family(font,"sans");
    pango_font_description_set_absolute_size(font,run.font_size*PANGO_SCALE);
    pango_font_description_set_weight(font,run.font_weight==700?PANGO_WEIGHT_BOLD:PANGO_WEIGHT_NORMAL);
    pango_layout_set_font_description(result.get(),font); pango_font_description_free(font);
    pango_layout_set_text(result.get(),run.text.data(),static_cast<int>(run.text.size()));
    pango_layout_set_width(result.get(),int(run.bounds.width*PANGO_SCALE));
    pango_layout_set_wrap(result.get(),PANGO_WRAP_WORD_CHAR);
    pango_layout_set_alignment(result.get(),run.alignment==TextAlignment::center?PANGO_ALIGN_CENTER:
        run.alignment==TextAlignment::right?PANGO_ALIGN_RIGHT:PANGO_ALIGN_LEFT);
    return result;
}
}
float measure_text(const TextRun& run) {
    Surface surface(cairo_image_surface_create(CAIRO_FORMAT_ARGB32,1,1),cairo_surface_destroy);
    Drawing drawing(cairo_create(surface.get()),cairo_destroy);
    auto text=layout(drawing.get(),run); int height=0;
    pango_layout_get_pixel_size(text.get(),nullptr,&height);
    return std::max(run.font_size,float(height));
}
PixelBuffer rasterize_text(const CanvasFrame& frame,double scale) {
    const auto [width,height]=canvas_pixels(frame,scale);
    Surface surface(cairo_image_surface_create(CAIRO_FORMAT_ARGB32,int(width),int(height)),cairo_surface_destroy);
    Drawing drawing(cairo_create(surface.get()),cairo_destroy);
    cairo_scale(drawing.get(),scale,scale);
    for (const auto& run:frame.texts) {
        cairo_save(drawing.get()); auto r=run.bounds;
        cairo_rectangle(drawing.get(),r.x,r.y,r.width,r.height); cairo_clip(drawing.get());
        cairo_move_to(drawing.get(),r.x,r.y);
        auto c=run.color;
        cairo_set_source_rgba(drawing.get(),encoded_srgb(c.red),encoded_srgb(c.green),encoded_srgb(c.blue),c.alpha);
        auto text=layout(drawing.get(),run); pango_cairo_show_layout(drawing.get(),text.get());
        cairo_restore(drawing.get());
    }
    if (cairo_status(drawing.get())!=CAIRO_STATUS_SUCCESS || cairo_surface_status(surface.get())!=CAIRO_STATUS_SUCCESS)
        throw std::runtime_error("Native Pango/Cairo text rasterization failed");
    cairo_surface_flush(surface.get());
    PixelBuffer result{width,height,std::vector<std::uint8_t>(std::size_t(width)*height*4)};
    auto data=cairo_image_surface_get_data(surface.get()); auto stride=cairo_image_surface_get_stride(surface.get());
    // Cairo ARGB32 is a native-endian 0xAARRGGBB integer, including on big-endian hosts.
    for (std::size_t y=0;y<height;++y) for (std::size_t x=0;x<width;++x) {
        auto argb=reinterpret_cast<const std::uint32_t*>(data+y*stride)[x];
        auto* p=result.rgba.data()+(y*width+x)*4;
        p[0]=std::uint8_t(argb>>16); p[1]=std::uint8_t(argb>>8); p[2]=std::uint8_t(argb); p[3]=std::uint8_t(argb>>24);
    }
    straight_rgba(result,false); return result;
}
} // namespace ii::gui::detail
