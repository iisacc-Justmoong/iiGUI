#include <iiGUI/NativeText.h>
#include <iiGUI/WebSurface.h>
#include <emscripten/html5.h>
#include <GLES3/gl3.h>
#include <cstring>

namespace ii::gui::detail {
namespace {
GLuint shader(GLenum type,const char* source){const auto result=glCreateShader(type);glShaderSource(result,1,&source,nullptr);glCompileShader(result);GLint valid=0;glGetShaderiv(result,GL_COMPILE_STATUS,&valid);if(!valid){char log[2048];glGetShaderInfoLog(result,sizeof(log),nullptr,log);glDeleteShader(result);throw std::runtime_error(std::string("WebGL2 shader failed: ")+log);}return result;}
GLuint program(const char* vertex,const char* fragment){const auto v=shader(GL_VERTEX_SHADER,vertex),f=shader(GL_FRAGMENT_SHADER,fragment);const auto p=glCreateProgram();glAttachShader(p,v);glAttachShader(p,f);glLinkProgram(p);glDeleteShader(v);glDeleteShader(f);GLint valid=0;glGetProgramiv(p,GL_LINK_STATUS,&valid);if(!valid){glDeleteProgram(p);throw std::runtime_error("WebGL2 program link failed");}return p;}
constexpr const char* panel_vertex=R"(#version 300 es
precision highp float;
layout(location=0) in vec4 bounds;layout(location=1) in vec4 topColor;layout(location=2) in vec4 bottomColor;layout(location=3) in vec4 params;
uniform vec2 extent;out vec2 point;flat out vec4 box;flat out vec4 top;flat out vec4 bottom;flat out vec2 style;
const vec2 corners[6]=vec2[](vec2(0,0),vec2(1,0),vec2(0,1),vec2(0,1),vec2(1,0),vec2(1,1));
void main(){vec2 q=corners[gl_VertexID];point=q*bounds.zw;box=bounds;top=topColor;bottom=bottomColor;style=params.xy;vec2 pixel=bounds.xy+point;gl_Position=vec4(pixel.x/extent.x*2.-1.,1.-pixel.y/extent.y*2.,0,1);})";
constexpr const char* panel_fragment=R"(#version 300 es
precision highp float;in vec2 point;flat in vec4 box;flat in vec4 top;flat in vec4 bottom;flat in vec2 style;out vec4 color;
void main(){float radius=min(style.x,min(box.z,box.w)*.5);vec2 q=abs(point-box.zw*.5)-(box.zw*.5-radius);float d=length(max(q,0.))+min(max(q.x,q.y),0.)-radius;float aa=max(fwidth(d),.0001);float alpha=1.-smoothstep(-aa,aa,d);if(style.y>0.)alpha*=smoothstep(-style.y-aa,-style.y+aa,d);vec4 c=mix(top,bottom,clamp(point.y/box.w,0.,1.));color=vec4(c.rgb*c.a,c.a)*alpha;})";
constexpr const char* quad_vertex=R"(#version 300 es
precision highp float;out vec2 uv;const vec2 corners[6]=vec2[](vec2(0,0),vec2(1,0),vec2(0,1),vec2(0,1),vec2(1,0),vec2(1,1));void main(){uv=corners[gl_VertexID];gl_Position=vec4(uv.x*2.-1.,1.-uv.y*2.,0,1);})";
constexpr const char* ink_vertex=R"(#version 300 es
precision highp float;out vec2 uv;uniform vec4 region;uniform vec2 targetExtent;
const vec2 corners[6]=vec2[](vec2(0,0),vec2(1,0),vec2(0,1),vec2(0,1),vec2(1,0),vec2(1,1));
void main(){uv=corners[gl_VertexID];vec2 p=region.xy+uv*region.zw;gl_Position=vec4(p.x/targetExtent.x*2.-1.,1.-p.y/targetExtent.y*2.,0,1);})";
constexpr const char* ink_fragment=R"(#version 300 es
precision highp float;in vec2 uv;uniform sampler2D image;out vec4 color;void main(){vec4 p=texture(image,uv);color=vec4(p.rgb*p.a,p.a);})";
constexpr const char* output_fragment=R"(#version 300 es
precision highp float;in vec2 uv;uniform sampler2D image;out vec4 color;void main(){vec4 p=texture(image,vec2(uv.x,1.-uv.y));vec3 c=p.a>0.?p.rgb/p.a:vec3(0);c=mix(c*12.92,1.055*pow(max(c,vec3(0)),vec3(1./2.4))-.055,step(vec3(.0031308),c));color=vec4(c*p.a,p.a);})";
void texture(GLuint& value){glGenTextures(1,&value);glBindTexture(GL_TEXTURE_2D,value);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);}
}
struct PlatformCanvas::Impl final {
    EMSCRIPTEN_WEBGL_CONTEXT_HANDLE context=0;GLuint panels=0,ink=0,output=0,vao=0,instances=0,fbo=0,target=0;
    std::uint32_t width=0,height=0;GraphicsInfo info;bool half=false;
    struct Layer{ForegroundKey key;GLuint texture=0;PixelClip region{};std::uint64_t bytes=0;};std::vector<Layer> layers;
    explicit Impl(Acceleration policy){
        EmscriptenWebGLContextAttributes attributes;emscripten_webgl_init_context_attributes(&attributes);attributes.majorVersion=2;attributes.minorVersion=0;attributes.alpha=true;attributes.antialias=false;attributes.depth=false;attributes.stencil=false;attributes.premultipliedAlpha=true;attributes.preserveDrawingBuffer=true;
        context=emscripten_webgl_create_context("#canvas",&attributes);if(context<=0)throw std::runtime_error("WebGL2 with OffscreenCanvas is required");
        try{current();half=emscripten_webgl_enable_extension(context,"EXT_color_buffer_float");const auto* renderer=reinterpret_cast<const char*>(glGetString(GL_RENDERER));info.adapter=renderer?renderer:"WebGL2";info.api=GraphicsApi::webgl2;
            info.hardware=false; // Browser adapter masking is not hardware proof.
            if(policy==Acceleration::require_hardware)throw std::runtime_error("Browser GPU hardware identity cannot be guaranteed by WebGL2");
            info.srgb_attachment=false; // Linear target plus explicit final sRGB encoding.
            panels=program(panel_vertex,panel_fragment);ink=program(ink_vertex,ink_fragment);output=program(quad_vertex,output_fragment);
            glGenVertexArrays(1,&vao);glGenBuffers(1,&instances);glGenFramebuffers(1,&fbo);texture(target);
        }catch(...){release();throw;}
    }
    void current(){if(emscripten_webgl_make_context_current(context)!=EMSCRIPTEN_RESULT_SUCCESS)throw std::runtime_error("WebGL2 context is unavailable or lost");}
    void release(){if(context){emscripten_webgl_make_context_current(context);for(auto& layer:layers)if(layer.texture)glDeleteTextures(1,&layer.texture);if(target)glDeleteTextures(1,&target);if(fbo)glDeleteFramebuffers(1,&fbo);if(instances)glDeleteBuffers(1,&instances);if(vao)glDeleteVertexArrays(1,&vao);if(panels)glDeleteProgram(panels);if(ink)glDeleteProgram(ink);if(output)glDeleteProgram(output);emscripten_webgl_destroy_context(context);context=0;}}
    ~Impl(){release();}
    void resize(std::uint32_t w,std::uint32_t h){if(w==width&&h==height)return;width=w;height=h;emscripten_set_canvas_element_size("#canvas",int(w),int(h));glBindTexture(GL_TEXTURE_2D,target);glTexImage2D(GL_TEXTURE_2D,0,half?GL_RGBA16F:GL_RGBA8,w,h,0,GL_RGBA,half?GL_HALF_FLOAT:GL_UNSIGNED_BYTE,nullptr);glBindFramebuffer(GL_FRAMEBUFFER,fbo);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,target,0);if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE)throw std::runtime_error("WebGL2 linear composition target is incomplete");info.pixel_width=w;info.pixel_height=h;info.cached_texture_bytes=std::uint64_t(w)*h*(half?8:4);for(auto& layer:layers)layer.key.valid=false;}
    void draw(const CanvasFrame& frame,double scale){current();++info.frame_requests;const auto [w,h]=canvas_pixels(frame,scale);resize(w,h);glBindFramebuffer(GL_FRAMEBUFFER,fbo);glViewport(0,0,w,h);glDisable(GL_SCISSOR_TEST);glClearColor(frame.background.red*frame.background.alpha,frame.background.green*frame.background.alpha,frame.background.blue*frame.background.alpha,frame.background.alpha);glClear(GL_COLOR_BUFFER_BIT);glEnable(GL_BLEND);glBlendFunc(GL_ONE,GL_ONE_MINUS_SRC_ALPHA);glBindVertexArray(vao);
        auto source=canvas_layers(frame);while(layers.size()>source.size()){auto& layer=layers.back();if(layer.texture)glDeleteTextures(1,&layer.texture);layers.pop_back();}layers.resize(source.size());
        for(std::size_t index=0;index<source.size();++index){const auto layer=source[index];auto& cache=layers[index];const auto clip=clip_pixels(layer,scale,w,h);glEnable(GL_SCISSOR_TEST);glScissor(clip.x,h-clip.y-clip.height,clip.width,clip.height);
            if(!layer.panels.empty()){const auto packed=pack_panels(layer.panels);glUseProgram(panels);glUniform2f(glGetUniformLocation(panels,"extent"),float(frame.viewport.width),float(frame.viewport.height));glBindBuffer(GL_ARRAY_BUFFER,instances);glBufferData(GL_ARRAY_BUFFER,GLsizeiptr(packed.size()*sizeof(GpuPanel)),packed.data(),GL_STREAM_DRAW);for(GLuint a=0;a<4;++a){glEnableVertexAttribArray(a);glVertexAttribPointer(a,4,GL_FLOAT,GL_FALSE,sizeof(GpuPanel),reinterpret_cast<void*>(std::uintptr_t(a*16)));glVertexAttribDivisor(a,1);}glDrawArraysInstanced(GL_TRIANGLES,0,6,GLsizei(packed.size()));for(GLuint a=0;a<4;++a){glDisableVertexAttribArray(a);glVertexAttribDivisor(a,0);}}
            if(layer.foreground()){if(!cache.key.matches(frame.viewport,scale,layer)){auto raster=rasterize_foreground_region(frame.viewport,layer,scale);auto& pixels=raster.pixels;cache.region=raster.region;cache.bytes=pixels.rgba.size();if(!cache.texture)texture(cache.texture);glBindTexture(GL_TEXTURE_2D,cache.texture);glTexImage2D(GL_TEXTURE_2D,0,GL_SRGB8_ALPHA8,pixels.width,pixels.height,0,GL_RGBA,GL_UNSIGNED_BYTE,pixels.rgba.data());cache.key.assign(frame.viewport,scale,layer);++info.foreground_rasterizations;}
                glUseProgram(ink);glUniform4f(glGetUniformLocation(ink,"region"),float(cache.region.x),float(cache.region.y),float(cache.region.width),float(cache.region.height));glUniform2f(glGetUniformLocation(ink,"targetExtent"),float(w),float(h));glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,cache.texture);glUniform1i(glGetUniformLocation(ink,"image"),0);glDrawArrays(GL_TRIANGLES,0,6);}}
        glDisable(GL_SCISSOR_TEST);glDisable(GL_BLEND);glBindFramebuffer(GL_FRAMEBUFFER,0);glViewport(0,0,w,h);glUseProgram(output);glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,target);glUniform1i(glGetUniformLocation(output,"image"),0);glDrawArrays(GL_TRIANGLES,0,6);if(glGetError()!=GL_NO_ERROR)throw std::runtime_error("WebGL2 draw failed");++info.scene_renders;++info.presentations;info.foreground_texture_bytes=0;for(const auto& layer:layers)info.foreground_texture_bytes+=layer.bytes;
    }
    PixelBuffer capture(){current();if(!width||!height)throw std::runtime_error("WebGL2 canvas has not rendered");PixelBuffer pixels{width,height,std::vector<std::uint8_t>(std::size_t(width)*height*4)};glBindFramebuffer(GL_FRAMEBUFFER,0);glReadPixels(0,0,width,height,GL_RGBA,GL_UNSIGNED_BYTE,pixels.rgba.data());const auto stride=std::size_t(width)*4;for(std::size_t y=0;y<height/2;++y)std::swap_ranges(pixels.rgba.begin()+y*stride,pixels.rgba.begin()+(y+1)*stride,pixels.rgba.begin()+(height-1-y)*stride);straight_rgba(pixels,false);return pixels;}
};
PlatformCanvas::PlatformCanvas(void*,void*,Acceleration acceleration):impl_(std::make_unique<Impl>(acceleration)){}
PlatformCanvas::~PlatformCanvas()=default;
void PlatformCanvas::draw(const CanvasFrame& frame,double scale,double){impl_->draw(frame,scale);}
PixelBuffer PlatformCanvas::capture(){return impl_->capture();}
GraphicsInfo PlatformCanvas::info()const{return impl_->info;}
bool PlatformCanvas::native_event(const void*){return false;}
}
