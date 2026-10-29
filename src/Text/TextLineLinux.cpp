#include <iiGUI/TextLineNative.h>
#include <iiGUI/NativeText.h>
#include <iiGUI/FontDatabase.h>
#include <pango/pangocairo.h>
#include <limits>

namespace ii::gui::detail {
namespace {
class Line final: public NativeTextLine {
    PangoLayout* line_ = nullptr;
    TextIndex display_;
    TextLineMetrics metrics_;
    double natural_baseline_ = 0;
public:
    Line(TextIndex display, const TextLineStyle& style): display_(std::move(display)) {
        // Pango coordinates are signed, scaled integers. Admit a conservative
        // advance budget before shaping instead of allowing integer overflow
        // in its line/caret APIs. This is a platform bound below the byte cap.
        const auto tabs=std::count(display_.text().begin(),display_.text().end(),'\t');
        const auto advance=double(display_.utf16_size()-std::size_t(tabs))*style.font_size*4+double(tabs)*style.tab_stop_distance;
        if(advance>double(std::numeric_limits<int>::max())/PANGO_SCALE-8192)throw std::length_error("Paragraph exceeds Pango's native coordinate admission budget");
        auto map = pango_cairo_font_map_get_default(); auto context = pango_font_map_create_context(map);
        line_ = pango_layout_new(context); g_object_unref(context);
        auto font = pango_font_description_new();
        const auto family=style.font_family.empty()?FontDatabase::application()->resolve_family(""):style.font_family;
        pango_font_description_set_family(font,family.c_str());
        pango_font_description_set_absolute_size(font, style.font_size*PANGO_SCALE); pango_font_description_set_weight(font, PangoWeight(style.font_weight));
        pango_layout_set_font_description(line_,font); pango_font_description_free(font);
        pango_layout_set_single_paragraph_mode(line_,TRUE); pango_layout_set_width(line_,-1);
        pango_layout_set_text(line_,display_.text().data(),int(display_.text().size()));
        auto stops = pango_tab_array_new(1,FALSE); pango_tab_array_set_tab(stops,0,PANGO_TAB_LEFT,int(style.tab_stop_distance*PANGO_SCALE));
        pango_layout_set_tabs(line_,stops); pango_tab_array_free(stops);
        PangoRectangle logical{}; pango_layout_get_extents(line_,nullptr,&logical);
        const auto natural = double(logical.height)/PANGO_SCALE;
        natural_baseline_ = double(pango_layout_get_baseline(line_))/PANGO_SCALE;
        metrics_ = {double(logical.width)/PANGO_SCALE,style.line_height > 0 ? style.line_height : std::max(1.,natural),0};
        metrics_.baseline = (metrics_.height-natural)/2+natural_baseline_;
    }
    ~Line() override { if (line_) g_object_unref(line_); }
    TextLineMetrics metrics() const override { return metrics_; }
    double caret(std::size_t position) const override {
        const auto byte = display_.byte_for_utf16(position); if (!byte) throw std::runtime_error(byte.error().message);
        PangoRectangle strong{}; pango_layout_get_cursor_pos(line_,int(*byte),&strong,nullptr); return double(strong.x)/PANGO_SCALE;
    }
    std::size_t hit(double x) const override {
        int byte = 0, trailing = 0;
        const auto coordinate = int(std::clamp(x*PANGO_SCALE,double(std::numeric_limits<int>::min()),double(std::numeric_limits<int>::max())));
        pango_layout_xy_to_index(line_,coordinate,int(natural_baseline_*PANGO_SCALE),&byte,&trailing);
        auto text = display_.text(); std::size_t offset = std::min(text.size(),std::size_t(std::max(0,byte)));
        while (trailing-- > 0 && offset < text.size()) { ++offset; while (offset < text.size() && (static_cast<unsigned char>(text[offset])&0xc0)==0x80) ++offset; }
        const auto result = display_.utf16_for_byte(offset); if (!result) throw std::runtime_error(result.error().message); return *result;
    }
    std::vector<TextLineSpan> selection(std::size_t first, std::size_t last) const override {
        std::vector<TextLineSpan> result; if (first == last) return result;
        auto a = display_.byte_for_utf16(first), b = display_.byte_for_utf16(last);
        if (!a || !b) throw std::runtime_error("Invalid Pango selection boundary");
        auto line = pango_layout_get_line_readonly(line_,0); int* ranges = nullptr; int count = 0;
        pango_layout_line_get_x_ranges(line,int(*a),int(*b),&ranges,&count);
        struct Free final { int* p; ~Free(){g_free(p);} } release{ranges};
        for (int i=0;i<count;++i) result.push_back({double(ranges[i*2])/PANGO_SCALE,double(ranges[i*2+1]-ranges[i*2])/PANGO_SCALE}); return result;
    }
    TextBitmap render(double x, double width, Color color, double scale) const override {
        const auto w = std::uint32_t(std::ceil(width*scale)), h = std::uint32_t(std::ceil(metrics_.height*scale));
        auto surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32,int(w),int(h)); auto drawing = cairo_create(surface);
        struct Release final { cairo_t* d; cairo_surface_t* s; ~Release(){cairo_destroy(d);cairo_surface_destroy(s);} } release{drawing,surface};
        cairo_scale(drawing,scale,scale); cairo_move_to(drawing,-x,metrics_.baseline-natural_baseline_);
        cairo_set_source_rgba(drawing,encoded_srgb(color.red),encoded_srgb(color.green),encoded_srgb(color.blue),color.alpha);
        pango_cairo_show_layout(drawing,line_);
        if (cairo_status(drawing)!=CAIRO_STATUS_SUCCESS || cairo_surface_status(surface)!=CAIRO_STATUS_SUCCESS) throw std::runtime_error("Pango text viewport rasterization failed");
        cairo_surface_flush(surface); PixelBuffer pixels{w,h,std::vector<std::uint8_t>(std::size_t(w)*h*4)};
        auto data = cairo_image_surface_get_data(surface); const auto stride = cairo_image_surface_get_stride(surface);
        for (std::size_t y=0;y<h;++y) for(std::size_t col=0;col<w;++col) {
            const auto argb = reinterpret_cast<const std::uint32_t*>(data+y*stride)[col]; auto p = pixels.rgba.data()+(y*w+col)*4;
            p[0]=std::uint8_t(argb>>16);p[1]=std::uint8_t(argb>>8);p[2]=std::uint8_t(argb);p[3]=std::uint8_t(argb>>24);
        }
        straight_rgba(pixels,false); return {w,h,std::move(pixels.rgba)};
    }
};
}
std::unique_ptr<NativeTextLine> native_text_line(TextIndex display, TextLineStyle style) { return std::make_unique<Line>(std::move(display),style); }
} // namespace ii::gui::detail
