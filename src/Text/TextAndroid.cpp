#include <iiGUI/AndroidSurface.h>
#include <iiGUI/AndroidLabelFont.h>
#include <iiGUI/FontNative.h>
#include <iiGUI/NativeText.h>
#include <iiGUI/TextLineNative.h>
#include <android/asset_manager.h>
#include <mutex>

namespace ii::gui::detail {
namespace {
std::mutex font_mutex;
std::shared_ptr<AndroidLabelFont> font_for(const TextRun& run){
    auto database=FontDatabase::application();auto loaded=database->load_bundled_fonts();if(!loaded)throw std::runtime_error(loaded.error().message);
    return native_android_font(database->resolve_family(run.font_family),int(run.font_weight));
}
float line_height(const TextRun& run,const AndroidLabelFont& font){return run.line_height>0?run.line_height:font.height(run.font_size);}
template<class F>void lines(std::string_view text,F&& visit){for(;;){const auto end=text.find('\n');visit(text.substr(0,end));if(end==text.npos)break;text.remove_prefix(end+1);}}
}
float measure_text_width(const TextRun& run){std::lock_guard lock(font_mutex);const auto font=font_for(run);float width=0;lines(run.text,[&](auto line){width=std::max(width,font->width(run.font_size,line));});return width;}
float measure_text(const TextRun& run){std::lock_guard lock(font_mutex);const auto font=font_for(run);lines(run.text,[&](auto line){(void)font->width(run.font_size,line);});return line_height(run,*font)*(1+std::count(run.text.begin(),run.text.end(),'\n'));}
PixelBuffer rasterize_text(const CanvasFrame& frame,double scale){
    std::lock_guard lock(font_mutex);const auto [width,height]=canvas_pixels(frame,scale);
    PixelBuffer result{width,height,std::vector<std::uint8_t>(std::size_t(width)*height*4)};
    for(const auto& run:frame.texts){
        const auto font=font_for(run);
        const auto r=run.bounds;auto clip=r;
        const float colors[]={encoded_srgb(run.color.red),encoded_srgb(run.color.green),encoded_srgb(run.color.blue)};
        if(run.clip){const auto c=*run.clip;const auto right=std::min(r.x+r.width,c.x+c.width),bottom=std::min(r.y+r.height,c.y+c.height);clip.x=std::max(r.x,c.x);clip.y=std::max(r.y,c.y);clip.width=std::max(0.f,right-clip.x);clip.height=std::max(0.f,bottom-clip.y);}
        const auto left=std::max(0,int(std::floor(clip.x*scale))),top=std::max(0,int(std::floor(clip.y*scale)));
        const auto right=std::min(int(width),int(std::ceil((clip.x+clip.width)*scale))),bottom=std::min(int(height),int(std::ceil((clip.y+clip.height)*scale)));
        float y=r.y+font->ascent(run.font_size);
        lines(run.text,[&](auto text){
            const auto advance=font->width(run.font_size,text);double x=(r.x+(run.alignment==TextAlignment::center?(r.width-advance)/2:run.alignment==TextAlignment::right?r.width-advance:0))*scale;
            unsigned previous=0;
            for(unsigned char codepoint:text){
                if(previous)x+=font->kerning(float(run.font_size*scale),previous,codepoint);
                const auto glyph=font->glyph(float(run.font_size*scale),codepoint);const auto gx=int(std::lround(x))+glyph.x,gy=int(std::lround(y*scale))+glyph.y;
                for(int row=std::max(0,top-gy);row<std::min(glyph.height,bottom-gy);++row)for(int col=std::max(0,left-gx);col<std::min(glyph.width,right-gx);++col){
                    const auto alpha=glyph.coverage[std::size_t(row)*glyph.width+col]/255.f*run.color.alpha;auto* p=result.rgba.data()+(std::size_t(gy+row)*width+gx+col)*4;
                    for(int c=0;c<3;++c)p[c]=std::uint8_t(std::clamp(std::lround(colors[c]*alpha*255+p[c]*(1-alpha)),0l,255l));
                    p[3]=std::uint8_t(std::clamp(std::lround(alpha*255+p[3]*(1-alpha)),0l,255l));
                }
                x+=glyph.advance;previous=codepoint;
            }
            y+=line_height(run,*font);
        });
    }
    straight_rgba(result,false);return result;
}
std::unique_ptr<NativeTextLine> native_text_line(TextIndex,TextLineStyle){throw std::runtime_error("Android shaped text editor is not implemented in the minimal label backend");}
}
