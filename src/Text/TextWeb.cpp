#include <iiGUI/NativeText.h>
#include <iiGUI/TextLineNative.h>
#include <iiGUI/FontDatabase.h>
#include <emscripten.h>
namespace ii::gui::detail {
namespace {
EM_JS(double,text_measure,(const char* family,int weight,double size,const char* text,int height),{
    const canvas=Module['iiGUITextCanvas']||(Module['iiGUITextCanvas']=new OffscreenCanvas(1,1));const c=canvas.getContext('2d');
    c.font=weight+' '+size+'px '+JSON.stringify(UTF8ToString(family));const lines=UTF8ToString(text).split('\n');
    return height?lines.length*size*1.25:Math.max(0,...lines.map(line=>c.measureText(line).width));
});
EM_JS(void,text_raster,(unsigned char* output,int width,int height,double scale,const char* family,int weight,double size,const char* text,double x,double y,double w,double h,double clipx,double clipy,double clipw,double cliph,int align,double lineheight,int elide,int underline,int red,int green,int blue,double alpha),{
    const canvas=new OffscreenCanvas(width,height);const c=canvas.getContext('2d',{willReadFrequently:true});c.scale(scale,scale);c.beginPath();c.rect(clipx,clipy,clipw,cliph);c.clip();
    c.font=weight+' '+size+'px '+JSON.stringify(UTF8ToString(family));c.textBaseline='top';c.textAlign=align===1?'center':align===2?'right':'left';c.fillStyle='rgba('+red+','+green+','+blue+','+alpha+')';
    for(let line of UTF8ToString(text).split('\n')){if(elide&&c.measureText(line).width>w){const scalars=Array.from(line);let a=0,b=scalars.length;while(a<b){const n=Math.ceil((a+b)/2);if(c.measureText(scalars.slice(0,n).join("")+'…').width<=w)a=n;else b=n-1;}line=scalars.slice(0,a).join("")+'…';}
        const px=x+(align===1?w/2:align===2?w:0);c.fillText(line,px,y);if(underline){const ink=c.measureText(line).width;c.fillRect(px-(align===1?ink/2:align===2?ink:0),y+size,ink,Math.max(1/scale,size/14));}y+=lineheight;}
    const pixels=c.getImageData(0,0,width,height).data;HEAPU8.set(pixels,output);
});
std::string family(const TextRun& run){return FontDatabase::application()->resolve_family(run.font_family);}
}
float measure_text_width(const TextRun& run){const auto name=family(run);return float(text_measure(name.c_str(),int(run.font_weight),run.font_size,run.text.c_str(),0));}
float measure_text(const TextRun& run){return float((run.line_height>0?run.line_height:run.font_size*1.25)*(1+std::count(run.text.begin(),run.text.end(),'\n')));}
PixelBuffer rasterize_text(const CanvasFrame& frame,double scale){const auto [width,height]=canvas_pixels(frame,scale);PixelBuffer result{width,height,std::vector<std::uint8_t>(std::size_t(width)*height*4)};std::vector<std::uint8_t> run_pixels(result.rgba.size());
    for(const auto& run:frame.texts){const auto name=family(run);auto clip=run.bounds;if(run.clip){const auto right=std::min(clip.x+clip.width,run.clip->x+run.clip->width),bottom=std::min(clip.y+clip.height,run.clip->y+run.clip->height);clip.x=std::max(clip.x,run.clip->x);clip.y=std::max(clip.y,run.clip->y);clip.width=std::max(0.f,right-clip.x);clip.height=std::max(0.f,bottom-clip.y);}
        text_raster(run_pixels.data(),int(width),int(height),scale,name.c_str(),int(run.font_weight),run.font_size,run.text.c_str(),run.bounds.x,run.bounds.y,run.bounds.width,run.bounds.height,clip.x,clip.y,clip.width,clip.height,int(run.alignment),run.line_height>0?run.line_height:run.font_size*1.25,run.elide_right,run.underline,int(std::lround(encoded_srgb(run.color.red)*255)),int(std::lround(encoded_srgb(run.color.green)*255)),int(std::lround(encoded_srgb(run.color.blue)*255)),run.color.alpha);
        for(std::size_t i=0;i<result.rgba.size();i+=4){const auto a=unsigned(run_pixels[i+3]),b=unsigned(result.rgba[i+3]);const auto total=a*255+b*(255-a);if(!total)continue;for(int c=0;c<3;++c)result.rgba[i+c]=std::uint8_t((unsigned(run_pixels[i+c])*a*255+unsigned(result.rgba[i+c])*b*(255-a)+total/2)/total);result.rgba[i+3]=std::uint8_t((total+127)/255);}}
    return result;
}
std::unique_ptr<NativeTextLine> native_text_line(TextIndex,TextLineStyle){throw std::runtime_error("Browser IME/editor integration is not implemented; query host_capabilities().text_input");}
}
