#include <iiGUI/NativeText.h>
#include <iiGUI/TextLineNative.h>
#include <iiGUI/FontDatabase.h>
#include "WebTextWrap.h"
#include <emscripten.h>
#include <array>
#include <span>
namespace ii::gui::detail {
namespace {
EM_JS(double,text_width,(const char* family,int weight,double size,double spacing,const char* text),{
    const canvas=Module['iiGUITextCanvas']||(Module['iiGUITextCanvas']=typeof OffscreenCanvas==='function'?new OffscreenCanvas(1,1):document.createElement('canvas'));const c=canvas.getContext('2d');
    const decoder=Module['iiGUITextDecoder']||(Module['iiGUITextDecoder']=new TextDecoder('utf-8'));
    const decode=pointer=>{let end=pointer;while(HEAPU8[end])++end;return decoder.decode(HEAPU8.slice(pointer,end));};
    const name=decode(family);const cssFamily=['sans-serif','serif','monospace'].includes(name)?name:JSON.stringify(name);
    c.font=weight+' '+size+'px '+cssFamily;if(!('letterSpacing' in c)&&spacing!==0)return NaN;c.letterSpacing=spacing+'px';let result=0;
    const newline=String.fromCharCode(10),carriage=String.fromCharCode(13);
    const content=decode(text).split(carriage+newline).join(newline).split(carriage).join(newline);
    for(const line of content.split(newline))result=Math.max(result,c.measureText(line).width);
    return result;
});
EM_JS(int,text_raster,(unsigned char* output,int width,int height,double scale,const char* family,int weight,double size,double spacing,const char* text,double x,double y,double w,double h,double clipx,double clipy,double clipw,double cliph,int align,double lineheight,int elide,int underline,int red,int green,int blue,double alpha,double tab_stop),{
    const canvas=typeof OffscreenCanvas==='function'?new OffscreenCanvas(width,height):document.createElement('canvas');canvas.width=width;canvas.height=height;const c=canvas.getContext('2d',{willReadFrequently:true});c.scale(scale,scale);c.beginPath();c.rect(clipx,clipy,clipw,cliph);c.clip();
    const decoder=Module['iiGUITextDecoder']||(Module['iiGUITextDecoder']=new TextDecoder('utf-8'));
    const decode=pointer=>{let end=pointer;while(HEAPU8[end])++end;return decoder.decode(HEAPU8.slice(pointer,end));};
    const name=decode(family);const cssFamily=['sans-serif','serif','monospace'].includes(name)?name:JSON.stringify(name);
    c.font=weight+' '+size+'px '+cssFamily;if(!('letterSpacing' in c)&&spacing!==0)return 0;c.letterSpacing=spacing+'px';
    const metrics=c.measureText('Mg'),ascent=metrics.fontBoundingBoxAscent,descent=metrics.fontBoundingBoxDescent;
    const hasFontMetrics=Number.isFinite(ascent)&&Number.isFinite(descent)&&ascent>=0&&descent>=0&&ascent+descent>0;
    c.textBaseline=hasFontMetrics?'alphabetic':'top';
    const baseline=hasFontMetrics?(lineheight-ascent-descent)/2+ascent:Math.max(0,(lineheight-size)/2);
    c.textAlign=align===1?'center':align===2?'right':'left';c.fillStyle='rgba('+red+','+green+','+blue+','+alpha+')';
    for(let line of decode(text).split('\n')){if(elide&&c.measureText(line).width>w){const scalars=Array.from(line);let a=0,b=scalars.length;while(a<b){const n=Math.ceil((a+b)/2);if(c.measureText(scalars.slice(0,n).join("")+'…').width<=w)a=n;else b=n-1;}line=scalars.slice(0,a).join("")+'…';}
        const px=x+(align===1?w/2:align===2?w:0);
        if (tab_stop > 0) {
            c.direction = 'ltr';
            let advance = 0;
            for (const segment of line.split('\t')) {
                c.fillText(segment, px + advance, y + baseline);
                advance = (Math.floor((advance + c.measureText(segment).width) / tab_stop) + 1) * tab_stop;
            }
        } else {
            c.fillText(line,px,y+baseline);
        }
        if(underline){const ink=c.measureText(line).width;const underlineY=y+baseline+(hasFontMetrics?Math.max(1/scale,size/16):size);c.fillRect(px-(align===1?ink/2:align===2?ink:0),underlineY,ink,Math.max(1/scale,size/14));}y+=lineheight;}
    const pixels=c.getImageData(0,0,width,height).data;HEAPU8.set(pixels,output);return 1;
});

// Use the browser's shaped ranges for UTF-16 geometry. The transient
// measurement element is never interactive and does not paint the widget.
EM_JS(int,web_line_geometry,(const char* family,int weight,double size,double height,double tab_stop,
    const char* text,int first,int last,const std::uint32_t* offsets,int offset_count,double* output,int capacity),{
    if (typeof document === 'undefined' || !document.body || !document.createRange) return -1;
    const decoder = Module['iiGUITextDecoder'] || (Module['iiGUITextDecoder'] = new TextDecoder('utf-8'));
    const decode = pointer => {
        let end = pointer;
        while (HEAPU8[end]) ++end;
        return decoder.decode(HEAPU8.slice(pointer, end));
    };
    const name = decode(family);
    const cssFamily = ['sans-serif','serif','monospace'].includes(name) ? name : JSON.stringify(name);
    const font = weight + ' ' + size + 'px ' + cssFamily;
    const node = document.createTextNode(decode(text));
    const span = document.createElement('span');
    Object.assign(span.style, {
        position: 'fixed', left: '0px', top: '0px', width: 'max-content',
        visibility: 'hidden', pointerEvents: 'none', whiteSpace: 'pre',
        font, lineHeight: height + 'px', letterSpacing: '0px', tabSize: tab_stop + 'px',
        direction: 'ltr', unicodeBidi: 'isolate'
    });
    span.setAttribute('aria-hidden', 'true');
    span.append(node);
    document.body.append(span);
    try {
        const box = span.getBoundingClientRect();
        const start = output / 8;
        if (first < 0) {
            const canvas = Module['iiGUITextCanvas'] || (Module['iiGUITextCanvas'] =
                typeof OffscreenCanvas === 'function' ? new OffscreenCanvas(1, 1) : document.createElement('canvas'));
            const context = canvas.getContext('2d');
            context.font = font;
            const metrics = context.measureText('Mg');
            const ascent = metrics.fontBoundingBoxAscent;
            const descent = metrics.fontBoundingBoxDescent;
            HEAPF64[start] = box.width;
            HEAPF64[start + 1] = Number.isFinite(ascent + descent) && ascent + descent > 0 ?
                (height - ascent - descent) / 2 + ascent : Math.max(0, (height - size) / 2);
            return 1;
        }
        const range = document.createRange();
        if (offset_count) {
            for (let i = 0; i < offset_count; ++i) {
                const offset = HEAPU32[offsets / 4 + i];
                range.setStart(node, offset);
                range.setEnd(node, offset);
                HEAPF64[start + i] = node.length === 0 ? 0 : range.getBoundingClientRect().left - box.left;
            }
            return offset_count;
        }
        range.setStart(node, first);
        range.setEnd(node, last);
        const rects = range.getClientRects();
        if (rects.length <= capacity) {
            for (let i = 0; i < rects.length; ++i) {
                HEAPF64[start + 2 * i] = rects[i].left - box.left;
                HEAPF64[start + 2 * i + 1] = rects[i].width;
            }
        }
        return rects.length;
    } finally {
        span.remove();
    }
});

class WebNativeTextLine final : public NativeTextLine {
public:
    WebNativeTextLine(TextIndex index, TextLineStyle style)
        : index_(std::move(index)), style_(std::move(style)),
          family_(FontDatabase::application()->resolve_family(style_.font_family)) {
        metrics_.height = style_.line_height > 0 ? style_.line_height : style_.font_size * 1.25;
        std::array<double, 2> values{};
        geometry(-1, 0, {}, values.data(), 0);
        metrics_.width = values[0];
        metrics_.baseline = values[1];
    }

    TextLineMetrics metrics() const override { return metrics_; }

    double caret(std::size_t position) const override {
        if (!index_.utf16_size()) return 0;
        const auto offset = std::uint32_t(position);
        double x = 0;
        geometry(0, 0, {&offset, 1}, &x, 0);
        return x;
    }

    std::size_t hit(double x) const override {
        const auto length = index_.utf16_size();
        if (!length) return 0;
        if (positions_.empty()) {
            std::vector<std::uint32_t> offsets{0};
            while (offsets.back() < length)
                offsets.push_back(std::uint32_t(index_.next_grapheme(offsets.back())));
            std::vector<double> positions(offsets.size());
            geometry(0, 0, offsets, positions.data(), 0);
            offsets_ = std::move(offsets);
            positions_ = std::move(positions);
        }
        const auto nearest = std::min_element(positions_.begin(), positions_.end(), [x](double a, double b) {
            return std::abs(a - x) < std::abs(b - x);
        });
        return offsets_[std::size_t(nearest - positions_.begin())];
    }

    std::vector<TextLineSpan> selection(std::size_t first, std::size_t last) const override {
        if (first == last) return {};
        std::vector<double> bounds(8);
        int count = geometry(int(first), int(last), {}, bounds.data(), int(bounds.size() / 2));
        if (std::size_t(count) > bounds.size() / 2) {
            bounds.resize(2 * std::size_t(count));
            count = geometry(int(first), int(last), {}, bounds.data(), count);
            if (std::size_t(count) > bounds.size() / 2)
                throw std::runtime_error("Browser text range changed during measurement");
        }
        std::vector<TextLineSpan> spans;
        for (int i = 0; i < count; ++i)
            if (bounds[2 * i + 1] > 0) spans.push_back({bounds[2 * i], bounds[2 * i + 1]});
        return spans;
    }

    TextBitmap render(double x, double width, Color color, double scale) const override {
        const auto w = std::uint32_t(std::ceil(width * scale));
        const auto h = std::uint32_t(std::ceil(metrics_.height * scale));
        TextBitmap bitmap{w, h, std::vector<std::uint8_t>(std::size_t(w) * h * 4)};
        if (index_.text().empty()) return bitmap;
        const auto channel = [](float value) { return int(std::lround(encoded_srgb(value) * 255)); };
        if (!text_raster(bitmap.rgba.data(), int(w), int(h), scale, family_.c_str(), int(style_.font_weight),
                style_.font_size, 0, index_.text().data(), -x, 0, width, metrics_.height,
                0, 0, width, metrics_.height, 0, metrics_.height, 0, 0,
                channel(color.red), channel(color.green), channel(color.blue), color.alpha, style_.tab_stop_distance))
            throw std::runtime_error("Browser text line could not be drawn");
        return bitmap;
    }

private:
    int geometry(int first, int last, std::span<const std::uint32_t> offsets, double* output, int capacity) const {
        const auto text = index_.text();
        const int count = web_line_geometry(family_.c_str(), int(style_.font_weight), style_.font_size,
            metrics_.height, style_.tab_stop_distance, text.empty() ? "" : text.data(), first, last,
            offsets.data(), int(offsets.size()), output, capacity);
        if (count < 0) throw NativeTextUnavailable("Browser text lines require a DOM with range geometry");
        return count;
    }

    TextIndex index_;
    TextLineStyle style_;
    std::string family_;
    TextLineMetrics metrics_;
    mutable std::vector<std::uint32_t> offsets_;
    mutable std::vector<double> positions_;
};

std::string family(const TextRun& run){return FontDatabase::application()->resolve_family(run.font_family);}
auto wrapped(const TextRun& run,const std::string& name){
    return web_text_wrap(run.text,run.bounds.width,!run.elide_right,[&](std::string_view line){
        const std::string terminated(line);
        return text_width(name.c_str(),int(run.font_weight),run.font_size,run.letter_spacing,terminated.c_str());
    });
}
std::string drawing_text(const TextRun& run,const std::string& name){
    const auto lines=wrapped(run,name);std::string result;
    result.reserve(run.text.size()+lines.size());
    for(std::size_t i=0;i<lines.size();++i){if(i)result.push_back('\n');result.append(lines[i]);}
    return result;
}
}
float measure_text_width(const TextRun& run){const auto name=family(run);const auto width=text_width(name.c_str(),int(run.font_weight),run.font_size,run.letter_spacing,run.text.c_str());if(!std::isfinite(width))throw std::runtime_error("Browser Canvas letter spacing is unavailable");return float(width);}
float measure_text(const TextRun& run){const auto name=family(run);const auto lines=wrapped(run,name);return std::min(8192.f,float((run.line_height>0?run.line_height:run.font_size*1.25)*lines.size()));}
PixelBuffer rasterize_text(const CanvasFrame& frame,double scale){const auto [width,height]=canvas_pixels(frame,scale);PixelBuffer result{width,height,std::vector<std::uint8_t>(std::size_t(width)*height*4)};std::vector<std::uint8_t> run_pixels(result.rgba.size());
    for(const auto& run:frame.texts){const auto name=family(run);auto clip=run.bounds;if(run.clip){const auto right=std::min(clip.x+clip.width,run.clip->x+run.clip->width),bottom=std::min(clip.y+clip.height,run.clip->y+run.clip->height);clip.x=std::max(clip.x,run.clip->x);clip.y=std::max(clip.y,run.clip->y);clip.width=std::max(0.f,right-clip.x);clip.height=std::max(0.f,bottom-clip.y);}
        const auto text=drawing_text(run,name);
        if(!text_raster(run_pixels.data(),int(width),int(height),scale,name.c_str(),int(run.font_weight),run.font_size,run.letter_spacing,text.c_str(),run.bounds.x,run.bounds.y,run.bounds.width,run.bounds.height,clip.x,clip.y,clip.width,clip.height,int(run.alignment),run.line_height>0?run.line_height:run.font_size*1.25,run.elide_right,run.underline,int(std::lround(encoded_srgb(run.color.red)*255)),int(std::lround(encoded_srgb(run.color.green)*255)),int(std::lround(encoded_srgb(run.color.blue)*255)),run.color.alpha,0))throw std::runtime_error("Browser Canvas letter spacing is unavailable");
        for(std::size_t i=0;i<result.rgba.size();i+=4){const auto a=unsigned(run_pixels[i+3]),b=unsigned(result.rgba[i+3]);const auto total=a*255+b*(255-a);if(!total)continue;for(int c=0;c<3;++c)result.rgba[i+c]=std::uint8_t((unsigned(run_pixels[i+c])*a*255+unsigned(result.rgba[i+c])*b*(255-a)+total/2)/total);result.rgba[i+3]=std::uint8_t((total+127)/255);}}
    return result;
}
std::unique_ptr<NativeTextLine> native_text_line(TextIndex index, TextLineStyle style) {
    return std::make_unique<WebNativeTextLine>(std::move(index), std::move(style));
}
}
