#include "NativeCanvas.h"
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <optional>

namespace ii::gui::detail {
namespace {
const char vertex_source[]=R"glsl(#version 300 es
precision highp float;
layout(location=0) in vec4 bounds;
layout(location=1) in vec4 panel_top;
layout(location=2) in vec4 panel_bottom;
layout(location=3) in float panel_radius;
uniform vec2 viewport_size;
out vec2 local, panel_size;
out vec4 top, bottom;
out float radius;
void main() {
    const vec2 corners[6]=vec2[6](vec2(0,0),vec2(1,0),vec2(0,1),vec2(0,1),vec2(1,0),vec2(1,1));
    vec2 uv=corners[gl_VertexID]; vec2 xy=bounds.xy+uv*bounds.zw;
    gl_Position=vec4(xy.x/viewport_size.x*2.0-1.0,1.0-xy.y/viewport_size.y*2.0,0,1);
    local=uv*bounds.zw; panel_size=bounds.zw; top=panel_top; bottom=panel_bottom; radius=panel_radius;
}
)glsl";
const char fragment_source[]=R"glsl(#version 300 es
precision highp float;
in vec2 local, panel_size;
in vec4 top, bottom;
in float radius;
out vec4 output_color;
void main() {
    vec2 q=abs(local-panel_size*0.5)-(panel_size*0.5-radius);
    float d=length(max(q,vec2(0)))+min(max(q.x,q.y),0.0)-radius;
    float coverage=clamp(0.5-d/max(fwidth(d),0.0001),0.0,1.0);
    vec4 color=mix(top,bottom,clamp(local.y/panel_size.y,0.0,1.0));
    color.a*=coverage; output_color=vec4(color.rgb*color.a,color.a);
}
)glsl";
void require(bool value,const char* message) { if (!value) throw std::runtime_error(message); }
bool extension(const char* list,std::string_view name) {
    std::string_view remaining=list ? list : "";
    while (!remaining.empty()) {
        const auto end=remaining.find(' ');
        if (remaining.substr(0,end)==name) return true;
        if (end==std::string_view::npos) break;
        remaining.remove_prefix(end+1);
    }
    return false;
}
void egl_checked(EGLBoolean value,const char* operation) {
    if (!value) throw std::runtime_error(std::string(operation)+" (EGL error "+std::to_string(eglGetError())+")");
}
GLuint compile(GLenum type,const char* source) {
    GLuint shader=glCreateShader(type);
    require(shader!=0,"Cannot allocate GLES3 shader");
    glShaderSource(shader,1,&source,nullptr); glCompileShader(shader);
    GLint success=0; glGetShaderiv(shader,GL_COMPILE_STATUS,&success);
    if (!success) {
        char message[4096]{}; glGetShaderInfoLog(shader,sizeof(message),nullptr,message);
        glDeleteShader(shader); throw std::runtime_error(std::string("GLES3 shader: ")+message);
    }
    return shader;
}
}
struct NativeCanvas::Impl final {
    Display* display=nullptr;
    Window window=0;
    Colormap colormap=0;
    EGLDisplay egl_display=EGL_NO_DISPLAY;
    EGLContext context=EGL_NO_CONTEXT;
    EGLSurface surface=EGL_NO_SURFACE;
    GLuint program=0, vertices=0, instances=0, texture=0, framebuffer=0;
    GLint size_location=-1;
    std::optional<CanvasFrame> previous;
    GraphicsInfo statistics;
    ~Impl() {
        if (context!=EGL_NO_CONTEXT && surface!=EGL_NO_SURFACE &&
            eglMakeCurrent(egl_display,surface,surface,context)) {
            glDeleteFramebuffers(1,&framebuffer); glDeleteTextures(1,&texture);
            glDeleteBuffers(1,&instances); glDeleteVertexArrays(1,&vertices);
            if (program) glDeleteProgram(program);
        }
        if (egl_display!=EGL_NO_DISPLAY) {
            eglMakeCurrent(egl_display,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);
            if (context!=EGL_NO_CONTEXT) eglDestroyContext(egl_display,context);
            if (surface!=EGL_NO_SURFACE) eglDestroySurface(egl_display,surface);
            eglTerminate(egl_display);
        }
        if (window) XDestroyWindow(display,window);
        if (colormap) XFreeColormap(display,colormap);
    }
    void current() { egl_checked(eglMakeCurrent(egl_display,surface,surface,context),"Cannot activate GLES3 canvas"); }
    void initialize(Display* connection,Window parent,Acceleration policy) {
        display=connection;
        egl_display=eglGetDisplay(static_cast<EGLNativeDisplayType>(display));
        require(egl_display!=EGL_NO_DISPLAY,"No X11 EGL display is available");
        EGLint major=0,minor=0;
        egl_checked(eglInitialize(egl_display,&major,&minor),"Cannot initialize EGL");
        const char* extensions=eglQueryString(egl_display,EGL_EXTENSIONS);
        require(major>1 || (major==1 && minor>=5) ||
            extension(extensions,"EGL_KHR_gl_colorspace"),"EGL sRGB window surfaces are required");
        egl_checked(eglBindAPI(EGL_OPENGL_ES_API),"Cannot select the native GLES API");
        const EGLint attributes[]{EGL_SURFACE_TYPE,EGL_WINDOW_BIT,EGL_RENDERABLE_TYPE,EGL_OPENGL_ES3_BIT,
            EGL_RED_SIZE,8,EGL_GREEN_SIZE,8,EGL_BLUE_SIZE,8,EGL_ALPHA_SIZE,8,EGL_NONE};
        EGLConfig config{}; EGLint count=0;
        egl_checked(eglChooseConfig(egl_display,attributes,&config,1,&count),"Cannot select GLES3 window format");
        require(count>0,"No GLES3 native window configuration is available");
        EGLint visual_id=0; egl_checked(eglGetConfigAttrib(egl_display,config,EGL_NATIVE_VISUAL_ID,&visual_id),"Cannot query EGL native visual");
        XVisualInfo query{}; query.visualid=static_cast<VisualID>(visual_id);
        int visuals=0; auto* visual=XGetVisualInfo(display,VisualIDMask,&query,&visuals);
        require(visual && visuals>0,"Cannot find the EGL X11 visual");
        colormap=XCreateColormap(display,parent,visual->visual,AllocNone);
        XSetWindowAttributes native{}; native.colormap=colormap; native.border_pixel=0;
        native.event_mask=ExposureMask | StructureNotifyMask;
        window=XCreateWindow(display,parent,0,0,1,1,0,visual->depth,InputOutput,visual->visual,
            CWColormap|CWBorderPixel|CWEventMask,&native);
        XFree(visual);
        require(window!=0,"Cannot create GLES3 child surface");
        const EGLint surface_attributes[]{EGL_GL_COLORSPACE,EGL_GL_COLORSPACE_SRGB,EGL_NONE};
        surface=eglCreateWindowSurface(egl_display,config,static_cast<EGLNativeWindowType>(window),surface_attributes);
        require(surface!=EGL_NO_SURFACE,"Cannot create an sRGB EGL window surface");
        const EGLint context_attributes[]{EGL_CONTEXT_CLIENT_VERSION,3,EGL_NONE};
        context=eglCreateContext(egl_display,config,EGL_NO_CONTEXT,context_attributes);
        require(context!=EGL_NO_CONTEXT,"Cannot create a GLES3 context");
        current(); egl_checked(eglSwapInterval(egl_display,1),"Cannot set EGL presentation interval");
        const auto* renderer=reinterpret_cast<const char*>(glGetString(GL_RENDERER));
        require(renderer!=nullptr,"Cannot identify the GLES3 renderer");
        statistics.api=GraphicsApi::opengl_es3; statistics.adapter=renderer;
        std::string name=renderer;
        std::transform(name.begin(),name.end(),name.begin(),[](unsigned char c) { return char(std::tolower(c)); });
        const bool software=name.find("llvmpipe")!=std::string::npos || name.find("softpipe")!=std::string::npos ||
            name.find("swrast")!=std::string::npos || name.find("swiftshader")!=std::string::npos ||
            name.find("software")!=std::string::npos;
        auto query_display=reinterpret_cast<PFNEGLQUERYDISPLAYATTRIBEXTPROC>(eglGetProcAddress("eglQueryDisplayAttribEXT"));
        auto query_device=reinterpret_cast<PFNEGLQUERYDEVICESTRINGEXTPROC>(eglGetProcAddress("eglQueryDeviceStringEXT"));
        EGLAttrib device=0;
        // A renderer name alone cannot establish hardware acceleration. Require
        // a driver-reported DRM device and exclude explicitly software devices.
        const char* client=eglQueryString(EGL_NO_DISPLAY,EGL_EXTENSIONS);
        if ((extension(client,"EGL_EXT_device_query") || extension(client,"EGL_EXT_device_base")) &&
            query_display && query_device && query_display(egl_display,EGL_DEVICE_EXT,&device)) {
            auto native=reinterpret_cast<EGLDeviceEXT>(device);
            const char* capabilities=query_device(native,EGL_EXTENSIONS);
            const bool software_device=extension(capabilities,"EGL_MESA_device_software");
            const char* node=nullptr;
            if (extension(capabilities,"EGL_EXT_device_drm_render_node"))
                node=query_device(native,EGL_DRM_RENDER_NODE_FILE_EXT);
            if ((!node || !*node) && extension(capabilities,"EGL_EXT_device_drm"))
                node=query_device(native,EGL_DRM_DEVICE_FILE_EXT);
            statistics.hardware=!software && !software_device && node && *node;
        }
        if (policy==Acceleration::require_hardware)
            require(statistics.hardware,"EGL cannot establish a hardware DRM device; hardware was required");
        GLint encoding=0;
        glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER,GL_BACK,GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING,&encoding);
        require(encoding==GL_SRGB,"EGL default framebuffer must expose sRGB encoding");
        statistics.srgb_attachment=true;
        program=glCreateProgram(); require(program!=0,"Cannot allocate GLES3 program");
        const auto vs=compile(GL_VERTEX_SHADER,vertex_source);
        GLuint fs=0;
        try { fs=compile(GL_FRAGMENT_SHADER,fragment_source); }
        catch (...) { glDeleteShader(vs); throw; }
        glAttachShader(program,vs); glAttachShader(program,fs); glLinkProgram(program);
        glDeleteShader(vs); glDeleteShader(fs);
        GLint linked=0; glGetProgramiv(program,GL_LINK_STATUS,&linked);
        if (!linked) {
            char message[4096]{}; glGetProgramInfoLog(program,sizeof(message),nullptr,message);
            throw std::runtime_error(std::string("Cannot link GLES3 program: ")+message);
        }
        size_location=glGetUniformLocation(program,"viewport_size");
        glGenVertexArrays(1,&vertices); glBindVertexArray(vertices);
        glGenBuffers(1,&instances); glBindBuffer(GL_ARRAY_BUFFER,instances);
        for (GLuint i=0;i<4;++i) {
            glEnableVertexAttribArray(i); glVertexAttribDivisor(i,1);
            glVertexAttribPointer(i,i==3?1:4,GL_FLOAT,GL_FALSE,sizeof(GpuPanel),
                reinterpret_cast<const void*>(std::uintptr_t(i)*16));
        }
        glGenTextures(1,&texture); glGenFramebuffers(1,&framebuffer);
    }
    void present() {
        glBindFramebuffer(GL_READ_FRAMEBUFFER,framebuffer); glBindFramebuffer(GL_DRAW_FRAMEBUFFER,0);
        const auto w=statistics.pixel_width,h=statistics.pixel_height;
        glBlitFramebuffer(0,0,w,h,0,0,w,h,GL_COLOR_BUFFER_BIT,GL_NEAREST);
        require(glGetError()==GL_NO_ERROR,"Cannot blit GLES3 sRGB canvas");
        egl_checked(eglSwapBuffers(egl_display,surface),"Cannot present GLES3 canvas");
        ++statistics.presentations;
    }
    void draw(const CanvasFrame& frame,double scale) {
        const auto [width,height]=canvas_pixels(frame,scale);
        XMoveResizeWindow(display,window,int(std::lround(frame.viewport.x*scale)),
            int(std::lround(frame.viewport.y*scale)),width,height);
        XMapWindow(display,window); XSync(display,False); current();
        bool changed=!previous || *previous!=frame;
        if (statistics.pixel_width!=width || statistics.pixel_height!=height) {
            GLint limit=0; glGetIntegerv(GL_MAX_TEXTURE_SIZE,&limit);
            require(width<=std::uint32_t(limit) && height<=std::uint32_t(limit),"Canvas exceeds this GLES device's texture limit");
            glBindTexture(GL_TEXTURE_2D,texture);
            glTexImage2D(GL_TEXTURE_2D,0,GL_SRGB8_ALPHA8,width,height,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
            glBindFramebuffer(GL_FRAMEBUFFER,framebuffer);
            glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,texture,0);
            require(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE,"Cannot allocate GLES3 sRGB attachment");
            statistics.pixel_width=width; statistics.pixel_height=height;
            statistics.cached_texture_bytes=std::uint64_t(width)*height*4; changed=true;
        }
        if (changed) {
            glBindFramebuffer(GL_FRAMEBUFFER,framebuffer); glViewport(0,0,width,height);
            glClearColor(frame.background.red,frame.background.green,frame.background.blue,1);
            glClear(GL_COLOR_BUFFER_BIT); glEnable(GL_BLEND); glBlendFunc(GL_ONE,GL_ONE_MINUS_SRC_ALPHA);
            glUseProgram(program); glUniform2f(size_location,frame.viewport.width,frame.viewport.height);
            const auto panels=pack_panels(frame);
            if (!panels.empty()) {
                glBindVertexArray(vertices); glBindBuffer(GL_ARRAY_BUFFER,instances);
                glBufferData(GL_ARRAY_BUFFER,panels.size()*sizeof(GpuPanel),panels.data(),GL_STREAM_DRAW);
                glDrawArraysInstanced(GL_TRIANGLES,0,6,static_cast<GLsizei>(panels.size()));
            }
            require(glGetError()==GL_NO_ERROR,"GLES3 canvas rendering failed");
            ++statistics.scene_renders;
        }
        present(); previous=frame;
    }
    PixelBuffer capture() {
        require(statistics.pixel_width!=0,"GLES3 canvas has not rendered"); current();
        const auto w=statistics.pixel_width,h=statistics.pixel_height;
        PixelBuffer result{w,h,std::vector<std::uint8_t>(std::size_t(w)*h*4)};
        glBindFramebuffer(GL_FRAMEBUFFER,framebuffer); glPixelStorei(GL_PACK_ALIGNMENT,1);
        glReadPixels(0,0,w,h,GL_RGBA,GL_UNSIGNED_BYTE,result.rgba.data());
        require(glGetError()==GL_NO_ERROR,"Cannot read GLES3 canvas");
        for (std::size_t y=0;y<h/2;++y)
            std::swap_ranges(result.rgba.begin()+y*w*4,result.rgba.begin()+(y+1)*w*4,result.rgba.begin()+(h-1-y)*w*4);
        return result;
    }
};
NativeCanvas::NativeCanvas(void* parent,void* display,Acceleration policy) : impl_(std::make_unique<Impl>()) {
    impl_->initialize(static_cast<Display*>(display),static_cast<Window>(reinterpret_cast<std::uintptr_t>(parent)),policy);
}
NativeCanvas::~NativeCanvas()=default;
void NativeCanvas::draw(const CanvasFrame& f,double s,double) { impl_->draw(f,s); }
PixelBuffer NativeCanvas::capture() { return impl_->capture(); }
GraphicsInfo NativeCanvas::info() const { return impl_->statistics; }
bool NativeCanvas::native_event(const void* event) {
    const auto& native=*static_cast<const XEvent*>(event);
    if (native.xany.window!=impl_->window) return false;
    if (native.type==Expose && !native.xexpose.count && impl_->previous) { impl_->current(); impl_->present(); }
    // A parent may be destroyed externally before the host observes its event.
    if (native.type==DestroyNotify) impl_->window=0;
    return true;
}
} // namespace ii::gui::detail
