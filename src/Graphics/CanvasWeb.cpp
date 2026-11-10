#include <iiGUI/NativeText.h>
#include <iiGUI/WebSurface.h>
#include <iiGUI/GpuEffects.h>
#include <iiGUI/WebGLScenePlan.h>
#include <emscripten/html5.h>
#include <GLES3/gl3.h>
#include <array>
#include <cstring>
#include <optional>

namespace ii::gui::detail {
namespace {
GLuint shader(GLenum type,const char* source){
    const auto result=glCreateShader(type);glShaderSource(result,1,&source,nullptr);glCompileShader(result);
    GLint valid=0;glGetShaderiv(result,GL_COMPILE_STATUS,&valid);
    if(!valid){char log[2048]{};glGetShaderInfoLog(result,sizeof(log),nullptr,log);glDeleteShader(result);throw std::runtime_error(std::string("WebGL2 shader failed: ")+log);}
    return result;
}
GLuint program(const char* vertex,const char* fragment){
    const auto v=shader(GL_VERTEX_SHADER,vertex);GLuint f=0,p=0;
    try{
        f=shader(GL_FRAGMENT_SHADER,fragment);p=glCreateProgram();glAttachShader(p,v);glAttachShader(p,f);glLinkProgram(p);
        GLint valid=0;glGetProgramiv(p,GL_LINK_STATUS,&valid);
        if(!valid){char log[2048]{};glGetProgramInfoLog(p,sizeof(log),nullptr,log);throw std::runtime_error(std::string("WebGL2 program link failed: ")+log);}
    }catch(...){if(p)glDeleteProgram(p);if(f)glDeleteShader(f);glDeleteShader(v);throw;}
    glDeleteShader(v);glDeleteShader(f);return p;
}
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
constexpr const char* effect_fragment=R"(#version 300 es
precision highp float;precision highp int;
uniform sampler2D source_image,shape_image,underlay_image,background_scene,content_scene;
uniform vec4 view,bounds,tint,border,style,underlay,base,highlight,underlay_tint;
uniform float target_height;
out vec4 color;
bool flag(int n){return (int(style.w)&n)!=0;}
float distance_to_shape(vec2 point,vec4 rect,float radius){vec2 q=abs(point-rect.xy-rect.zw*.5)-(rect.zw*.5-radius);return length(max(q,0.))+min(max(q.x,q.y),0.)-radius;}
float shape_mask(vec2 point){if(flag(2)){vec2 uv=(point-bounds.xy)/bounds.zw;if(any(lessThan(uv,vec2(0)))||any(greaterThan(uv,vec2(1))))return 0.;return textureLod(shape_image,uv,0.).a;}return clamp(.5-distance_to_shape(point,bounds,style.x)*view.z,0.,1.);}
vec4 over(vec4 a,vec4 b){return a+b*(1.-a.a);}
vec4 input_at(vec2 point){
    vec4 value=vec4(base.rgb*base.a,base.a);vec2 scene_uv=vec2(point.x/view.x,1.-point.y/view.y);
    // Framebuffer textures are linear and already premultiplied.
    if(flag(8))value=over(textureLod(source_image,scene_uv,0.),value);
    if(flag(32))value=over(textureLod(background_scene,scene_uv,0.),value);
    if(flag(64))value=over(textureLod(content_scene,scene_uv,0.),value);
    if(flag(4)){vec2 uv=(point-underlay.xy)/underlay.zw;if(all(greaterThanEqual(uv,vec2(0)))&&all(lessThanEqual(uv,vec2(1)))){vec4 image=textureLod(underlay_image,uv,0.);if(flag(16))image=vec4(underlay_tint.rgb,image.a*underlay_tint.a);image.rgb*=image.a;value=over(image,value);}}
    return value;
}
void main(){
    float scale=view.z,opacity=view.w,blur=style.y;vec2 point=vec2(gl_FragCoord.x,target_height-gl_FragCoord.y)/scale;bool shadow=flag(1);
    if(opacity<=0.||distance_to_shape(point,bounds,flag(2)?0.:style.x)>(shadow?blur+1.:1.)){color=vec4(0);return;}
    float coverage=shape_mask(point);vec4 paint;
    if(shadow){if(blur>.01){coverage=0.;float total=0.,sigma=blur/3.,step_size=blur/6.;for(int y=-6;y<=6;++y)for(int x=-6;x<=6;++x){vec2 offset=vec2(float(x),float(y))*step_size;float weight=exp(-dot(offset,offset)/(2.*sigma*sigma));coverage+=shape_mask(point+offset)*weight;total+=weight;}coverage/=total;}float alpha=coverage*tint.a*opacity;color=vec4(tint.rgb*alpha,alpha);return;}
    if(blur>.01){paint=vec4(0);float total=0.,sigma=blur/3.,step_size=blur/6.;for(int y=-6;y<=6;++y)for(int x=-6;x<=6;++x){vec2 offset=vec2(float(x),float(y))*step_size;float weight=exp(-dot(offset,offset)/(2.*sigma*sigma));paint+=input_at(point+offset)*weight;total+=weight;}paint/=total;}else paint=input_at(point);
    paint=over(vec4(tint.rgb*tint.a,tint.a),paint);
    if(style.z>0.&&border.a>0.){vec4 inner=bounds+vec4(style.z,style.z,-2.*style.z,-2.*style.z);float inside=clamp(.5-distance_to_shape(point,inner,max(0.,style.x-style.z))*scale,0.,1.);float alpha=clamp((coverage-inside)/max(coverage,.0001),0.,1.)*border.a;paint=over(vec4(border.rgb*alpha,alpha),paint);}
    float light=highlight.x*clamp((coverage-shape_mask(point-vec2(0,highlight.y)))/max(coverage,.0001),0.,1.);paint=over(vec4(light),paint);color=paint*(coverage*opacity);
})";
void gl_checked(const char* operation){const auto error=glGetError();if(error!=GL_NO_ERROR)throw std::runtime_error(std::string(operation)+" (WebGL error "+std::to_string(error)+")");}
void texture(GLuint& value){
    glGenTextures(1,&value);if(!value){gl_checked("Cannot allocate WebGL2 texture");throw std::runtime_error("Cannot allocate WebGL2 texture");}
    glBindTexture(GL_TEXTURE_2D,value);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
}
}
struct PlatformCanvas::Impl final {
    EMSCRIPTEN_WEBGL_CONTEXT_HANDLE context=0;
    GLuint panels=0,ink=0,output=0,effect=0,vao=0,transparent=0;
    GLint panel_extent=-1,ink_region=-1,ink_extent=-1,ink_image=-1,output_image=-1,effect_height=-1,effect_underlay_tint=-1;
    std::array<GLint,8> effect_parameters_locations{};
    std::uint32_t width=0,height=0;GraphicsInfo info;bool half=false;
    struct Layer{ForegroundKey key;GLuint texture=0;PixelClip region{};std::uint64_t bytes=0;PanelKey geometry;GLuint buffer=0;};
    struct Target{GLuint fbo=0,texture=0;std::vector<Layer> layers;};
    struct EffectAsset{std::shared_ptr<const RasterImage> image;GLuint texture=0;};
    Target window,content_snapshot,current_snapshot;
    std::vector<Target> scenes;
    std::optional<CanvasFrame> previous;
    double previous_scale=0;
    std::vector<EffectAsset> effect_assets;
    explicit Impl(Acceleration policy){
        EmscriptenWebGLContextAttributes attributes;emscripten_webgl_init_context_attributes(&attributes);
        attributes.majorVersion=2;attributes.minorVersion=0;attributes.alpha=true;attributes.antialias=false;attributes.depth=false;attributes.stencil=false;attributes.premultipliedAlpha=true;attributes.preserveDrawingBuffer=true;
        context=emscripten_webgl_create_context("#canvas",&attributes);if(context<=0)throw std::runtime_error("WebGL2 with OffscreenCanvas is required");
        try{
            current();half=emscripten_webgl_enable_extension(context,"EXT_color_buffer_float")||
                emscripten_webgl_enable_extension(context,"EXT_color_buffer_half_float");
            const auto* renderer=reinterpret_cast<const char*>(glGetString(GL_RENDERER));info.adapter=renderer?renderer:"WebGL2";info.api=GraphicsApi::webgl2;
            info.hardware=false; // Browser adapter masking is not hardware proof.
            if(policy==Acceleration::require_hardware)throw std::runtime_error("Browser GPU hardware identity cannot be guaranteed by WebGL2");
            info.srgb_attachment=false; // Linear target plus explicit final sRGB encoding.
            panels=program(panel_vertex,panel_fragment);ink=program(ink_vertex,ink_fragment);output=program(quad_vertex,output_fragment);effect=program(quad_vertex,effect_fragment);
            panel_extent=glGetUniformLocation(panels,"extent");ink_region=glGetUniformLocation(ink,"region");ink_extent=glGetUniformLocation(ink,"targetExtent");ink_image=glGetUniformLocation(ink,"image");output_image=glGetUniformLocation(output,"image");
            const char* names[]{"view","bounds","tint","border","style","underlay","base","highlight"};
            for(std::size_t i=0;i<effect_parameters_locations.size();++i)effect_parameters_locations[i]=glGetUniformLocation(effect,names[i]);
            effect_height=glGetUniformLocation(effect,"target_height");effect_underlay_tint=glGetUniformLocation(effect,"underlay_tint");
            const char* samplers[]{"source_image","shape_image","underlay_image","background_scene","content_scene"};
            glUseProgram(effect);for(int i=0;i<5;++i)glUniform1i(glGetUniformLocation(effect,samplers[i]),i);
            glGenVertexArrays(1,&vao);gl_checked("Cannot create WebGL2 drawing resources");
            info.backdrop_supported=info.shadow_supported=info.content_transform_supported=info.scene_sources_supported=true;
        }catch(...){release();throw;}
    }
    void current(){if(emscripten_webgl_make_context_current(context)!=EMSCRIPTEN_RESULT_SUCCESS)throw std::runtime_error("WebGL2 context is unavailable or lost");}
    void unbind_images(){for(int i=0;i<5;++i){glActiveTexture(GL_TEXTURE0+i);glBindTexture(GL_TEXTURE_2D,0);}glActiveTexture(GL_TEXTURE0);}
    static void release_foreground(Layer& layer){if(layer.texture)glDeleteTextures(1,&layer.texture);layer.texture=0;layer.key={};layer.region={};layer.bytes=0;}
    static void release_geometry(Layer& layer){if(layer.buffer)glDeleteBuffers(1,&layer.buffer);layer.buffer=0;layer.geometry.clear();}
    static void release_layer(Layer& layer){release_foreground(layer);release_geometry(layer);}
    static void release_attachment(Target& target){if(target.texture)glDeleteTextures(1,&target.texture);if(target.fbo)glDeleteFramebuffers(1,&target.fbo);target.texture=target.fbo=0;}
    static void release_target(Target& target){for(auto& layer:target.layers)release_layer(layer);target.layers.clear();release_attachment(target);}
    void release(){
        if(!context)return;
        emscripten_webgl_make_context_current(context);unbind_images();glBindFramebuffer(GL_FRAMEBUFFER,0);
        release_target(window);release_target(content_snapshot);release_target(current_snapshot);for(auto& scene:scenes)release_target(scene);scenes.clear();
        for(auto& asset:effect_assets)if(asset.texture)glDeleteTextures(1,&asset.texture);effect_assets.clear();
        if(transparent)glDeleteTextures(1,&transparent);if(vao)glDeleteVertexArrays(1,&vao);
        if(panels)glDeleteProgram(panels);if(ink)glDeleteProgram(ink);if(output)glDeleteProgram(output);if(effect)glDeleteProgram(effect);
        emscripten_webgl_destroy_context(context);context=0;
    }
    ~Impl(){release();}
    void allocate(Target& target,std::uint32_t w,std::uint32_t h){
        if(target.texture)return;Target made;
        try{
            glActiveTexture(GL_TEXTURE0);texture(made.texture);glTexImage2D(GL_TEXTURE_2D,0,half?GL_RGBA16F:GL_RGBA8,w,h,0,GL_RGBA,half?GL_HALF_FLOAT:GL_UNSIGNED_BYTE,nullptr);
            gl_checked("Cannot allocate WebGL2 target texture");
            glGenFramebuffers(1,&made.fbo);if(!made.fbo){gl_checked("Cannot allocate WebGL2 framebuffer");throw std::runtime_error("Cannot allocate WebGL2 framebuffer");}
            glBindFramebuffer(GL_FRAMEBUFFER,made.fbo);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,made.texture,0);
            gl_checked("Cannot attach WebGL2 composition target");
            if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE)throw std::runtime_error("WebGL2 linear composition target is incomplete");
        }catch(...){release_target(made);throw;}
        target.texture=made.texture;target.fbo=made.fbo;
    }
    void resize(std::uint32_t w,std::uint32_t h){
        if(w==width&&h==height&&window.texture)return;
        unbind_images();glBindFramebuffer(GL_FRAMEBUFFER,0);release_attachment(window);release_target(content_snapshot);release_target(current_snapshot);
        for(auto& scene:scenes)release_attachment(scene);width=height=0;
        if(emscripten_set_canvas_element_size("#canvas",int(w),int(h))!=EMSCRIPTEN_RESULT_SUCCESS)throw std::runtime_error("Cannot resize WebGL2 canvas");
        allocate(window,w,h);width=w;height=h;info.pixel_width=w;info.pixel_height=h;
    }
    GLuint effect_image(const std::shared_ptr<const RasterImage>& image){
        if(!image)return transparent;
        for(const auto& asset:effect_assets)if(asset.image==image)return asset.texture;
        GLuint value=0;
        try{glActiveTexture(GL_TEXTURE0);texture(value);glTexImage2D(GL_TEXTURE_2D,0,GL_SRGB8_ALPHA8,image->width(),image->height(),0,GL_RGBA,GL_UNSIGNED_BYTE,image->rgba().data());gl_checked("Cannot upload WebGL2 effect image");effect_assets.push_back({image,value});}
        catch(...){if(value)glDeleteTextures(1,&value);throw;}
        ++info.effect_image_uploads;return value;
    }
    void prepare_effects(const WebGLScenePlan& plan){
        if(plan.effects&&!transparent){
            const std::uint8_t pixel[4]{};GLuint value=0;
            try{glActiveTexture(GL_TEXTURE0);texture(value);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,1,1,0,GL_RGBA,GL_UNSIGNED_BYTE,pixel);gl_checked("Cannot allocate WebGL2 transparent sampler");}
            catch(...){if(value)glDeleteTextures(1,&value);throw;}transparent=value;
        }
        for(auto it=effect_assets.begin();it!=effect_assets.end();){
            if(std::ranges::find(plan.assets,it->image)==plan.assets.end()){glDeleteTextures(1,&it->texture);it=effect_assets.erase(it);}else ++it;
        }
        for(const auto& image:plan.assets)effect_image(image);
        if(plan.content_snapshot)allocate(content_snapshot,width,height);else release_target(content_snapshot);
        if(plan.previous_snapshot)allocate(current_snapshot,width,height);else release_target(current_snapshot);
        if(!plan.effects){if(transparent)glDeleteTextures(1,&transparent);transparent=0;}
        info.cached_texture_bytes=std::uint64_t(width)*height*(half?8:4)+(plan.effects?plan.snapshot_bytes:0);
        info.backdrop_texture_bytes=plan.captures?plan.snapshot_bytes:0;
        info.scene_texture_bytes=plan.scene_bytes;info.effect_image_texture_bytes=plan.asset_bytes;
    }
    void snapshot(Target& source,Target& destination){
        glDisable(GL_SCISSOR_TEST);glBindFramebuffer(GL_READ_FRAMEBUFFER,source.fbo);glBindFramebuffer(GL_DRAW_FRAMEBUFFER,destination.fbo);
        glBlitFramebuffer(0,0,width,height,0,0,width,height,GL_COLOR_BUFFER_BIT,GL_NEAREST);
        glBindFramebuffer(GL_FRAMEBUFFER,source.fbo);++info.backdrop_copies;
    }
    void draw_effect(const CanvasLayerView& layer,const CanvasFrame& frame,double scale,bool shadow){
        const auto parameters=effect_parameters(layer,frame.viewport,scale,shadow);
        // The shared push-constant representation packs tint bits in z/w.
        // WebGL uses a separate full-precision tint uniform, avoiding NaN bit
        // patterns in otherwise unused floating-point uniform components.
        const float light[]{parameters.highlight[0],parameters.highlight[1],0,0};
        const float* values[]{parameters.view,parameters.bounds,parameters.tint,parameters.border,parameters.style,parameters.underlay,parameters.base,light};
        glUseProgram(effect);for(std::size_t i=0;i<effect_parameters_locations.size();++i)glUniform4fv(effect_parameters_locations[i],1,values[i]);
        glUniform1f(effect_height,float(height));
        const auto* material=shadow?nullptr:&*layer.backdrop;
        const auto color=material&&material->underlay_tint?*material->underlay_tint:Color{0,0,0,0};
        glUniform4f(effect_underlay_tint,color.red,color.green,color.blue,color.alpha);
        const auto scene=[&](const std::optional<std::size_t>& index){return index?scenes[*index].texture:transparent;};
        const GLuint source=material&&material->capture?(material->source==BackdropSource::previous_layers?current_snapshot.texture:content_snapshot.texture):transparent;
        const GLuint inputs[]{source,effect_image(shadow?layer.shadow->mask:material->mask),effect_image(shadow?std::shared_ptr<const RasterImage>{}:material->underlay),material?scene(material->background_scene):transparent,material?scene(material->content_scene):transparent};
        for(int i=0;i<5;++i){glActiveTexture(GL_TEXTURE0+i);glBindTexture(GL_TEXTURE_2D,inputs[i]);}
        glDrawArrays(GL_TRIANGLES,0,6);unbind_images();
        if(shadow){++info.shadow_masks;if(layer.shadow->blur_radius>.01)++info.shadow_blurs;}
        else if(layer.backdrop->blur_radius>.01)++info.backdrop_blurs;
    }
    void draw_pass(const CanvasFrame& frame,Target& target,std::optional<Rect> crop,double scale){
        unbind_images();glBindFramebuffer(GL_FRAMEBUFFER,target.fbo);glViewport(0,0,width,height);glDisable(GL_SCISSOR_TEST);
        glClearColor(frame.background.red*frame.background.alpha,frame.background.green*frame.background.alpha,frame.background.blue*frame.background.alpha,frame.background.alpha);glClear(GL_COLOR_BUFFER_BIT);
        glEnable(GL_BLEND);glBlendFunc(GL_ONE,GL_ONE_MINUS_SRC_ALPHA);glBindVertexArray(vao);
        const auto batches=canvas_batch_layers(frame);const auto& views=batches.views;
        const bool needs_content=std::ranges::any_of(views,[](const auto& layer){return layer.backdrop&&layer.backdrop->capture&&layer.backdrop->source==BackdropSource::content;});
        while(target.layers.size()>views.size()){release_layer(target.layers.back());target.layers.pop_back();}target.layers.resize(views.size());
        for(std::size_t index=0;index<views.size();++index){
            const auto layer=views[index];auto& cache=target.layers[index];
            if(index==1&&needs_content)snapshot(target,content_snapshot);
            if(layer.backdrop&&layer.backdrop->capture&&layer.backdrop->source==BackdropSource::previous_layers)snapshot(target,current_snapshot);
            const auto clip=scene_clip_pixels(layer,crop,scale,width,height);
            if(!clip.width||!clip.height){release_foreground(cache);if(!batches.panel_count(index)||!cache.geometry.matches(batches.panel_layers(index)))release_geometry(cache);continue;}
            glEnable(GL_SCISSOR_TEST);glScissor(clip.x,height-clip.y-clip.height,clip.width,clip.height);
            if(layer.shadow)draw_effect(layer,frame,scale,true);if(layer.backdrop)draw_effect(layer,frame,scale,false);
            const auto count=batches.panel_count(index);const auto geometry=batches.panel_layers(index);
            if(count){
                if(!cache.buffer||!cache.geometry.matches(geometry)){
                    const auto packed=pack_panel_batch(geometry,count);GLuint replacement=0;PanelKey next;
                    try{
                        next.assign(geometry);glGenBuffers(1,&replacement);gl_checked("Cannot create WebGL2 geometry buffer");
                        if(!replacement)throw std::runtime_error("Cannot create WebGL2 geometry buffer");
                        glBindBuffer(GL_ARRAY_BUFFER,replacement);glBufferData(GL_ARRAY_BUFFER,GLsizeiptr(packed.size()*sizeof(GpuPanel)),packed.data(),GL_STATIC_DRAW);
                        gl_checked("Cannot upload WebGL2 geometry");
                    }catch(...){if(replacement)glDeleteBuffers(1,&replacement);throw;}
                    release_geometry(cache);cache.buffer=replacement;cache.geometry=std::move(next);++info.geometry_uploads;
                }
                glUseProgram(panels);glUniform2f(panel_extent,float(frame.viewport.width),float(frame.viewport.height));
                glBindBuffer(GL_ARRAY_BUFFER,cache.buffer);
                for(GLuint a=0;a<4;++a){glEnableVertexAttribArray(a);glVertexAttribPointer(a,4,GL_FLOAT,GL_FALSE,sizeof(GpuPanel),reinterpret_cast<void*>(std::uintptr_t(a*16)));glVertexAttribDivisor(a,1);}
                glDrawArraysInstanced(GL_TRIANGLES,0,6,GLsizei(count));++info.geometry_draw_batches;
                for(GLuint a=0;a<4;++a){glDisableVertexAttribArray(a);glVertexAttribDivisor(a,0);}
            }else release_geometry(cache);
            if(layer.foreground()){
                if(!cache.key.matches(frame.viewport,scale,layer,true)){
                    auto raster=rasterize_foreground_region(frame.viewport,layer,scale,true);const auto& pixels=raster.pixels;
                    try{if(!cache.texture)texture(cache.texture);glBindTexture(GL_TEXTURE_2D,cache.texture);glTexImage2D(GL_TEXTURE_2D,0,GL_SRGB8_ALPHA8,pixels.width,pixels.height,0,GL_RGBA,GL_UNSIGNED_BYTE,pixels.rgba.data());gl_checked("Cannot upload WebGL2 foreground");}
                    catch(...){release_foreground(cache);throw;}
                    cache.region=raster.region;cache.bytes=pixels.rgba.size();cache.key.assign(frame.viewport,scale,layer,true);++info.foreground_rasterizations;
                }
                info.foreground_texture_bytes+=cache.bytes;glUseProgram(ink);glUniform4f(ink_region,float(cache.region.x),float(cache.region.y),float(cache.region.width),float(cache.region.height));glUniform2f(ink_extent,float(width),float(height));
                glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,cache.texture);glUniform1i(ink_image,0);glDrawArrays(GL_TRIANGLES,0,6);glBindTexture(GL_TEXTURE_2D,0);
            }else release_foreground(cache);
        }
        glDisable(GL_SCISSOR_TEST);glBindFramebuffer(GL_FRAMEBUFFER,0);++info.scene_renders;
    }
    void draw(const CanvasFrame& frame,double scale){
        current();++info.frame_requests;const auto [w,h]=canvas_pixels(frame,scale);
        const auto plan=webgl_scene_plan(frame,w,h,half);if(!plan)throw std::invalid_argument("Invalid WebGL2 source scene graph");
        info.effect_texture_budget_bytes=effect_texture_budget(w,h);
        if(!webgl_scene_budget_ok(*plan,w,h))throw std::runtime_error("WebGL2 source scene and effect textures exceed the effect budget");
        bool committed=false;
        struct Guard final {std::optional<CanvasFrame>& previous;bool& committed;~Guard(){if(!committed)previous.reset();}} guard{previous,committed};
        const bool target_changed=width!=w||height!=h||!window.texture||previous_scale!=scale;
        resize(w,h);
        while(scenes.size()>frame.scenes.size()){release_target(scenes.back());scenes.pop_back();}scenes.resize(frame.scenes.size());
        std::array<bool,maximum_canvas_scenes> active{};for(std::size_t i=0;i<plan->order.count;++i)active[plan->order.order[i]]=true;
        for(std::size_t i=0;i<scenes.size();++i)if(!active[i])release_target(scenes[i]);
        prepare_effects(*plan);
        std::array<bool,maximum_canvas_scenes> available{};
        for(std::size_t i=0;i<plan->order.count;++i){const auto index=plan->order.order[i];available[index]=scenes[index].texture!=0;}
        const auto dirty=scene_rebuilds(frame,plan->order,previous?&*previous:nullptr,available,target_changed);
        info.foreground_texture_bytes=0;
        for(std::size_t i=0;i<plan->order.count;++i){
            const auto index=plan->order.order[i];auto& target=scenes[index];
            if(dirty[index]){allocate(target,w,h);const auto source=canvas_scene_frame(frame.scenes[index]);draw_pass(source,target,frame.scenes[index].bounds,scale);}
            else for(const auto& layer:target.layers)if(layer.texture)info.foreground_texture_bytes+=layer.bytes;
        }
        draw_pass(frame,window,{},scale);
        glDisable(GL_BLEND);glBindFramebuffer(GL_FRAMEBUFFER,0);glViewport(0,0,w,h);glUseProgram(output);glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,window.texture);glUniform1i(output_image,0);glDrawArrays(GL_TRIANGLES,0,6);glBindTexture(GL_TEXTURE_2D,0);
        gl_checked("WebGL2 draw failed");++info.presentations;previous=frame;previous_scale=scale;committed=true;
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
