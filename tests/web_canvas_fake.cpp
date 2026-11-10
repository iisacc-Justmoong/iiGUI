#include <iiGUI/NativeCanvas.h>
#include <iiGUI/Image.h>
#include <GLES3/gl3.h>
#include <emscripten/html5.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <iostream>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

using namespace ii::gui;
using ii::gui::detail::GpuPanel;
namespace fake {
struct Draw {GLuint framebuffer=0;std::string program;std::array<GLuint,5> textures{};int flags=0;std::array<int,4> scissor{};std::array<float,4> region{},view{},tint{};float height=0;int instances=1;};
struct Clear {GLuint framebuffer=0;std::array<float,4> color{};};
struct Copy {GLuint source=0,destination=0;bool scissored=false;};
GLuint next=1,read_fbo=0,draw_fbo=0,program=0,bound_buffer=0;unsigned active=0;
bool half=true,half_only=false,scissor_enabled=false,feedback=false,invalid_copy=false;
GLenum pending_error=GL_NO_ERROR;bool fail_next_srgb_upload=false;
bool fail_next_target_allocation=false,fail_next_buffer_upload=false;
GLuint failed_upload=0,failed_buffer=0;
std::array<GLuint,16> bound{};std::array<int,4> scissor{};std::array<float,4> clear{};
std::map<GLuint,GLuint> attachments;
std::map<GLuint,GLint> formats;
std::set<GLuint> incomplete_targets;
std::map<GLuint,std::string> sources,programs;
std::map<GLuint,std::vector<GLuint>> attached;
std::map<std::pair<GLuint,std::string>,GLint> locations;
std::map<GLint,std::pair<GLuint,std::string>> location_names;
std::map<std::pair<GLuint,std::string>,std::array<float,4>> uniforms;
std::set<GLuint> textures,framebuffers,buffers,vaos,live_programs;
std::vector<Draw> draws;std::vector<Clear> clears;std::vector<Copy> copies;std::vector<GpuPanel> packed;
unsigned texture_creations=0,canvas_resizes=0;
void require(bool value,const char* what){if(!value)throw std::runtime_error(what);}
GLuint allocate(std::set<GLuint>& set){const auto id=next++;set.insert(id);return id;}
void reset_events(){draws.clear();clears.clear();copies.clear();packed.clear();feedback=invalid_copy=false;}
void draw(){
    const auto& name=programs[program];Draw event{.framebuffer=draw_fbo,.program=name,.scissor=scissor};
    if(name=="effect"){
        std::copy_n(bound.begin(),5,event.textures.begin());event.flags=int(uniforms[{program,"style"}][3]);
        event.view=uniforms[{program,"view"}];event.tint=uniforms[{program,"underlay_tint"}];event.height=uniforms[{program,"target_height"}][0];
        for(auto texture:event.textures)if(texture&&attachments[draw_fbo]==texture)feedback=true;
    }else if(name=="ink"){
        event.region=uniforms[{program,"region"}];
        if(bound[0]&&attachments[draw_fbo]==bound[0])feedback=true;
    }
    draws.push_back(event);
}
}
extern "C" {
void emscripten_webgl_init_context_attributes(EmscriptenWebGLContextAttributes* a){*a={};}
EMSCRIPTEN_WEBGL_CONTEXT_HANDLE emscripten_webgl_create_context(const char*,const EmscriptenWebGLContextAttributes*){return 1;}
EMSCRIPTEN_RESULT emscripten_webgl_make_context_current(EMSCRIPTEN_WEBGL_CONTEXT_HANDLE){return 0;}
int emscripten_webgl_enable_extension(EMSCRIPTEN_WEBGL_CONTEXT_HANDLE,const char* name){
    if(std::string(name)=="EXT_color_buffer_float")return fake::half&&!fake::half_only;
    if(std::string(name)=="EXT_color_buffer_half_float")return fake::half&&fake::half_only;
    return false;
}
EMSCRIPTEN_RESULT emscripten_webgl_destroy_context(EMSCRIPTEN_WEBGL_CONTEXT_HANDLE){return 0;}
EMSCRIPTEN_RESULT emscripten_set_canvas_element_size(const char*,int,int){++fake::canvas_resizes;return 0;}
GLuint glCreateShader(GLenum){return fake::next++;}
void glShaderSource(GLuint id,GLsizei count,const GLchar* const* source,const GLint*){for(int i=0;i<count;++i)fake::sources[id]+=source[i];}
void glCompileShader(GLuint){}
void glGetShaderiv(GLuint,GLenum,GLint* value){*value=1;}
void glGetShaderInfoLog(GLuint,GLsizei,GLsizei*,GLchar*){}
void glDeleteShader(GLuint){}
GLuint glCreateProgram(){return fake::allocate(fake::live_programs);}
void glAttachShader(GLuint program,GLuint shader){fake::attached[program].push_back(shader);}
void glLinkProgram(GLuint program){
    std::string combined;for(const auto shader:fake::attached[program])combined+=fake::sources[shader];
    fake::programs[program]=combined.find("uniform sampler2D source_image")!=std::string::npos?"effect":
        combined.find("targetExtent")!=std::string::npos?"ink":
        combined.find("layout(location=0) in vec4 bounds")!=std::string::npos?"panels":"output";
}
void glGetProgramiv(GLuint,GLenum,GLint* value){*value=1;}
void glGetProgramInfoLog(GLuint,GLsizei,GLsizei*,GLchar*){}
void glDeleteProgram(GLuint program){fake::live_programs.erase(program);}
void glUseProgram(GLuint program){fake::program=program;}
GLint glGetUniformLocation(GLuint program,const GLchar* name){
    auto key=std::make_pair(program,std::string(name));const auto found=fake::locations.find(key);if(found!=fake::locations.end())return found->second;
    const auto location=GLint(fake::locations.size()+1);fake::locations[key]=location;fake::location_names[location]=key;return location;
}
void glUniform1i(GLint location,GLint value){fake::uniforms[fake::location_names[location]]={float(value),0,0,0};}
void glUniform1f(GLint location,GLfloat value){fake::uniforms[fake::location_names[location]]={value,0,0,0};}
void glUniform2f(GLint location,GLfloat x,GLfloat y){fake::uniforms[fake::location_names[location]]={x,y,0,0};}
void glUniform4f(GLint location,GLfloat x,GLfloat y,GLfloat z,GLfloat w){fake::uniforms[fake::location_names[location]]={x,y,z,w};}
void glUniform4fv(GLint location,GLsizei,const GLfloat* value){std::copy_n(value,4,fake::uniforms[fake::location_names[location]].begin());}
const GLubyte* glGetString(GLenum){return reinterpret_cast<const GLubyte*>("headless fake");}
GLenum glGetError(){const auto value=fake::pending_error;fake::pending_error=GL_NO_ERROR;return value;}
void glGenTextures(GLsizei n,GLuint* out){for(int i=0;i<n;++i){out[i]=fake::allocate(fake::textures);++fake::texture_creations;}}
void glDeleteTextures(GLsizei n,const GLuint* values){for(int i=0;i<n;++i){fake::textures.erase(values[i]);fake::formats.erase(values[i]);fake::incomplete_targets.erase(values[i]);for(auto& texture:fake::bound)if(texture==values[i])texture=0;}}
void glActiveTexture(GLenum unit){fake::active=unit-GL_TEXTURE0;}
void glBindTexture(GLenum,GLuint texture){fake::bound.at(fake::active)=texture;}
void glTexParameteri(GLenum,GLenum,GLint){}
void glTexImage2D(GLenum,GLint,GLint internal,GLsizei,GLsizei,GLint,GLenum,GLenum,const void*){
    fake::formats[fake::bound[fake::active]]=internal;
    if(fake::fail_next_target_allocation&&(internal==GL_RGBA8||internal==GL_RGBA16F)){fake::fail_next_target_allocation=false;fake::incomplete_targets.insert(fake::bound[fake::active]);fake::pending_error=GL_OUT_OF_MEMORY;}
    if(fake::fail_next_srgb_upload&&internal==GL_SRGB8_ALPHA8){fake::fail_next_srgb_upload=false;fake::failed_upload=fake::bound[fake::active];fake::pending_error=GL_OUT_OF_MEMORY;}
}
void glGenFramebuffers(GLsizei n,GLuint* out){for(int i=0;i<n;++i)out[i]=fake::allocate(fake::framebuffers);}
void glDeleteFramebuffers(GLsizei n,const GLuint* values){for(int i=0;i<n;++i){fake::framebuffers.erase(values[i]);fake::attachments.erase(values[i]);}}
void glBindFramebuffer(GLenum target,GLuint value){if(target!=GL_DRAW_FRAMEBUFFER)fake::read_fbo=value;if(target!=GL_READ_FRAMEBUFFER)fake::draw_fbo=value;}
void glFramebufferTexture2D(GLenum,GLenum,GLenum,GLuint texture,GLint){fake::attachments[fake::draw_fbo]=texture;}
GLenum glCheckFramebufferStatus(GLenum){return fake::incomplete_targets.contains(fake::attachments[fake::draw_fbo])?GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:GL_FRAMEBUFFER_COMPLETE;}
void glBlitFramebuffer(GLint,GLint,GLint,GLint,GLint,GLint,GLint,GLint,GLbitfield,GLenum){
    fake::copies.push_back({fake::read_fbo,fake::draw_fbo,fake::scissor_enabled});
    if(!fake::read_fbo||!fake::draw_fbo||fake::attachments[fake::read_fbo]==fake::attachments[fake::draw_fbo])fake::invalid_copy=true;
}
void glGenBuffers(GLsizei n,GLuint* out){for(int i=0;i<n;++i)out[i]=fake::allocate(fake::buffers);}
void glDeleteBuffers(GLsizei n,const GLuint* values){for(int i=0;i<n;++i)fake::buffers.erase(values[i]);}
void glBindBuffer(GLenum,GLuint buffer){fake::bound_buffer=buffer;}
void glBufferData(GLenum,GLsizeiptr size,const void* data,GLenum){
    if(fake::fail_next_buffer_upload){fake::fail_next_buffer_upload=false;fake::failed_buffer=fake::bound_buffer;fake::pending_error=GL_OUT_OF_MEMORY;return;}
    if(data&&size>=GLsizeiptr(sizeof(GpuPanel))){GpuPanel panel;std::memcpy(&panel,data,sizeof(panel));fake::packed.push_back(panel);}
}
void glGenVertexArrays(GLsizei n,GLuint* out){for(int i=0;i<n;++i)out[i]=fake::allocate(fake::vaos);}
void glDeleteVertexArrays(GLsizei n,const GLuint* values){for(int i=0;i<n;++i)fake::vaos.erase(values[i]);}
void glBindVertexArray(GLuint){}
void glEnableVertexAttribArray(GLuint){}
void glDisableVertexAttribArray(GLuint){}
void glVertexAttribPointer(GLuint,GLint,GLenum,GLboolean,GLsizei,const void*){}
void glVertexAttribDivisor(GLuint,GLuint){}
void glViewport(GLint,GLint,GLsizei,GLsizei){}
void glEnable(GLenum value){if(value==GL_SCISSOR_TEST)fake::scissor_enabled=true;}
void glDisable(GLenum value){if(value==GL_SCISSOR_TEST)fake::scissor_enabled=false;}
void glScissor(GLint x,GLint y,GLsizei w,GLsizei h){fake::scissor={x,y,w,h};}
void glBlendFunc(GLenum,GLenum){}
void glClearColor(GLfloat r,GLfloat g,GLfloat b,GLfloat a){fake::clear={r,g,b,a};}
void glClear(GLbitfield){fake::clears.push_back({fake::draw_fbo,fake::clear});}
void glDrawArrays(GLenum,GLint,GLsizei){fake::draw();}
void glDrawArraysInstanced(GLenum,GLint,GLsizei,GLsizei count){fake::draw();fake::draws.back().instances=count;}
void glReadPixels(GLint,GLint,GLsizei,GLsizei,GLenum,GLenum,void*){}
}
namespace {
CanvasFrame graph(){
    CanvasFrame frame{.viewport={0,0,100,80},.background={.1f,.2f,.3f,1}};
    frame.scenes.resize(2);for(auto& scene:frame.scenes)scene.viewport=frame.viewport;
    frame.scenes[0].bounds=Rect{10,10,30,20};
    frame.scenes[0].layers.push_back({.panels={{{0,0,60,50},{1,0,0,1},{1,0,0,1}}}});
    frame.scenes[1].layers.push_back({.backdrop=BackdropMaterial{.bounds={0,0,40,30},.capture=false,.content_scene=0}});
    auto image=rgba_image(1,1,{255,255,255,128});fake::require(bool(image),"Cannot create effect image");
    CanvasLayer layer;layer.panels.push_back({{20,10,20,10},{0,1,0,1},{0,1,0,1}});
    layer.backdrop=BackdropMaterial{.bounds={0,0,60,40},.blur_radius=2,.source=BackdropSource::previous_layers,
        .mask=*image,.underlay=*image,.underlay_bounds={0,0,20,20},.capture=true,.underlay_tint=Color{1,0,0,.5},.background_scene=0,.content_scene=1};
    layer.shadow=BoxShadow{.bounds={0,0,20,20},.blur_radius=2,.mask=*image};
    layer.content_transform={.scale=.5f,.opacity=.75f,.offset_x=5};frame.overlays.push_back(std::move(layer));
    return frame;
}
void run(bool half,bool half_only=false){
    fake::half=half;fake::half_only=half_only;fake::reset_events();fake::texture_creations=0;fake::canvas_resizes=0;
    {
        ii::gui::detail::PlatformCanvas canvas(nullptr,nullptr,Acceleration::prefer_hardware);
        auto info=canvas.info();fake::require(info.backdrop_supported&&info.shadow_supported&&info.content_transform_supported&&info.scene_sources_supported,"WebGL did not advertise implemented material capabilities");
        auto frame=graph();canvas.draw(frame,1,80);info=canvas.info();
        fake::require(fake::clears.size()==3&&fake::clears[0].color[3]==0&&fake::clears[1].color[3]==0&&fake::clears[2].color[3]==1,"WebGL did not clear source scenes transparently before the opaque window");
        const auto source0=fake::attachments[fake::clears[0].framebuffer],source1=fake::attachments[fake::clears[1].framebuffer];
        std::vector<fake::Draw> effects;for(const auto& draw:fake::draws)if(draw.program=="effect")effects.push_back(draw);
        fake::require(effects.size()==3&&effects[0].flags==64&&effects[0].textures[4]==source0,"Nested source was not sampled in dependency order");
        fake::require(effects[1].flags==3&&effects[2].flags==126&&effects[2].textures[3]==source0&&effects[2].textures[4]==source1,"Window material lost a source, mask, underlay or shadow");
        fake::require(effects[2].view==std::array<float,4>{100,80,1,.75f}&&effects[2].height==80&&effects[2].tint==std::array<float,4>{1,0,0,.5f},"WebGL lost the logical extent, opacity or underlay tint");
        fake::require(fake::copies.size()==1&&effects[2].textures[0]==fake::attachments[fake::copies[0].destination],"WebGL copied unused snapshots or lost the previous-layer input");
        fake::require(effects[2].textures[1]==effects[2].textures[2]&&fake::formats[effects[2].textures[1]]==GL_SRGB8_ALPHA8&&fake::formats[source0]==(half?GL_RGBA16F:GL_RGBA8),"WebGL used the wrong image or scene texture format");
        fake::require(std::ranges::all_of(effects,[](const auto& effect){return std::ranges::all_of(effect.textures,[](auto texture){return texture!=0;});}),"WebGL left an active sampler incomplete");
        fake::require(std::ranges::all_of(fake::copies,[](const auto& copy){return !copy.scissored;})&&!fake::invalid_copy&&!fake::feedback,"WebGL created a framebuffer feedback loop or clipped a snapshot");
        fake::require(std::ranges::any_of(fake::draws,[](const auto& d){return d.program=="panels"&&d.scissor==std::array<int,4>{10,50,30,20};}),"WebGL source crop used the wrong framebuffer coordinates");
        fake::require(std::ranges::any_of(fake::packed,[](const auto& panel){return std::abs(panel.bounds[0]-15)<.001&&std::abs(panel.bounds[1]-5)<.001&&std::abs(panel.top[3]-.75)<.001;}),"WebGL lost the panel transform or opacity");
        const auto bytes=std::uint64_t(100)*80*(half?8:4);
        fake::require(info.scene_renders==3&&info.presentations==1&&info.scene_texture_bytes==2*bytes&&info.backdrop_texture_bytes==bytes&&info.effect_image_uploads==1&&info.effect_image_texture_bytes==8,"WebGL resource accounting or shared image reuse is incorrect");
        const auto created=fake::texture_creations;fake::reset_events();canvas.draw(frame,1,80);
        fake::require(fake::texture_creations==created&&canvas.info().effect_image_uploads==1&&!fake::feedback,"WebGL reallocated a stable graph or introduced a feedback loop");
        fake::require(canvas.info().scene_renders==info.scene_renders+1&&fake::clears.size()==1,
            "WebGL redrew unchanged source scenes");
        auto root_change=frame;root_change.background={.3f,.2f,.1f,1};
        auto before=canvas.info().scene_renders;fake::reset_events();canvas.draw(root_change,1,80);
        fake::require(canvas.info().scene_renders==before+1&&fake::clears.size()==1,
            "A window-only change rebuilt the WebGL source graph");
        const auto retained=std::ranges::find_if(fake::draws,[](const auto& draw){return draw.program=="effect"&&draw.flags==126;});
        fake::require(retained!=fake::draws.end()&&retained->textures[3]==source0&&retained->textures[4]==source1&&!fake::feedback,
            "Window compositing lost the retained source textures");
        auto source_change=root_change;source_change.scenes[0].layers[0].panels[0].top={0,1,0,1};
        before=canvas.info().scene_renders;fake::reset_events();canvas.draw(source_change,1,80);
        fake::require(canvas.info().scene_renders==before+3&&fake::clears.size()==3,
            "A changed WebGL dependency did not rebuild its consumers");
        auto failed_source=source_change;auto added=rgba_image(1,1,{255,0,0,128});
        fake::require(bool(added),"Cannot create changed source image");
        failed_source.scenes[0].layers[0].images.push_back({.bounds={1,1,4,4},.image=*added});
        fake::fail_next_srgb_upload=true;bool source_failed=false;
        try{canvas.draw(failed_source,1,80);}catch(const std::runtime_error&){source_failed=true;}
        fake::require(source_failed,"Changed source upload did not fail as requested");
        before=canvas.info().scene_renders;fake::reset_events();canvas.draw(source_change,1,80);
        fake::require(canvas.info().scene_renders==before+3&&fake::clears.size()==3,
            "A failed source update left a partially rendered texture reusable");
        auto cached_foreground=source_change;
        cached_foreground.scenes[0].layers[0].images.push_back({.bounds={15,15,4,4},.image=*added});
        canvas.draw(cached_foreground,1,80);const auto cached_info=canvas.info();
        fake::require(cached_info.foreground_texture_bytes>0,"Source foreground was not accounted");
        cached_foreground.background={.2f,.3f,.1f,1};canvas.draw(cached_foreground,1,80);
        fake::require(canvas.info().scene_renders==cached_info.scene_renders+1&&
            canvas.info().foreground_rasterizations==cached_info.foreground_rasterizations&&
            canvas.info().foreground_texture_bytes==cached_info.foreground_texture_bytes,
            "Retained source foreground was rerasterized or omitted from resource accounting");
        fake::reset_events();canvas.draw(frame,2,160);
        fake::require(canvas.info().pixel_width==200&&canvas.info().pixel_height==160&&
            std::ranges::any_of(fake::draws,[](const auto& d){return d.program=="panels"&&d.scissor==std::array<int,4>{20,100,60,40};})&&
            std::ranges::any_of(fake::draws,[](const auto& d){return d.program=="effect"&&d.flags==126&&d.view==std::array<float,4>{100,80,2,.75f}&&d.height==160;}),
            "WebGL lost the backing scale in its scissor or material coordinates");
        auto resized=frame;resized.viewport={0,0,120,90};for(auto& scene:resized.scenes)scene.viewport=resized.viewport;
        fake::reset_events();canvas.draw(resized,1,90);
        fake::require(fake::canvas_resizes==3&&canvas.info().scene_texture_bytes==std::uint64_t(120)*90*(half?8:4)*2&&!fake::feedback,"WebGL retained stale scene targets after a resize");
        auto huge=frame;huge.viewport={0,0,8192,8192};for(auto& scene:huge.scenes)scene.viewport=huge.viewport;
        const auto allocations=fake::texture_creations,resizes=fake::canvas_resizes;bool rejected=false;
        try{canvas.draw(huge,1,8192);}catch(const std::runtime_error&){rejected=true;}
        fake::require(rejected&&fake::texture_creations==allocations&&fake::canvas_resizes==resizes,"WebGL allocated an over-budget graph");
        CanvasFrame plain{.viewport={0,0,120,90}};canvas.draw(plain,1,90);
        info=canvas.info();fake::require(info.scene_texture_bytes==0&&info.effect_image_texture_bytes==0&&fake::textures.size()==1&&fake::framebuffers.size()==1,"WebGL retained unused graph resources");
        fake::fail_next_srgb_upload=true;bool failed=false;
        try{canvas.draw(resized,1,90);}catch(const std::runtime_error&){failed=true;}
        fake::require(failed&&!fake::textures.contains(fake::failed_upload),"A failed WebGL effect upload leaked its texture");
        canvas.draw(resized,1,90);
        fake::require(canvas.info().effect_image_uploads==2,"WebGL did not recover the effect image after an upload failure");
        canvas.draw(plain,1,90);
        auto image=rgba_image(1,1,{255,0,0,128});fake::require(bool(image),"Cannot make foreground image");
        plain.images.push_back({.bounds={1,1,4,4},.image=*image});
        const auto rasterizations=canvas.info().foreground_rasterizations;fake::fail_next_srgb_upload=true;failed=false;
        try{canvas.draw(plain,1,90);}catch(const std::runtime_error&){failed=true;}
        fake::require(failed&&!fake::textures.contains(fake::failed_upload),"A failed WebGL foreground upload leaked its texture");
        canvas.draw(plain,1,90);fake::require(canvas.info().foreground_rasterizations==rasterizations+1,"WebGL did not rerasterize a failed foreground upload");
        CanvasFrame transformed{.viewport={0,0,120,90}};CanvasLayer foreground;
        foreground.images.push_back({.bounds={20,10,20,10},.image=*image});foreground.content_transform={.scale=.5f,.offset_x=5};
        transformed.overlays.push_back(foreground);fake::reset_events();canvas.draw(transformed,1,90);
        fake::require(std::ranges::any_of(fake::draws,[](const auto& d){return d.program=="ink"&&d.region==std::array<float,4>{15,5,10,5};}),"WebGL did not apply the foreground transform");
        const auto previous=canvas.info().foreground_rasterizations;transformed.overlays[0].content_transform.offset_x=7;
        fake::reset_events();canvas.draw(transformed,1,90);
        fake::require(canvas.info().foreground_rasterizations==previous+1&&std::ranges::any_of(fake::draws,[](const auto& d){return d.program=="ink"&&d.region[0]==17;}),"WebGL reused stale transformed foreground pixels");
        CanvasFrame captured{.viewport={0,0,120,90}};captured.overlays.push_back({.backdrop=BackdropMaterial{.bounds={0,0,20,20},.capture=true}});
        fake::reset_events();canvas.draw(captured,1,90);
        const auto material=std::ranges::find_if(fake::draws,[](const auto& d){return d.program=="effect";});
        fake::require(material!=fake::draws.end()&&material->flags==8&&fake::copies.size()==1&&material->textures[0]==fake::attachments[fake::copies[0].destination],"WebGL failed to bind the base-content snapshot");
    }
    fake::require(fake::textures.empty()&&fake::framebuffers.empty()&&fake::buffers.empty()&&fake::vaos.empty()&&fake::live_programs.empty(),"WebGL leaked a resource on destruction");
}
void allocation_recovery(){
    fake::half=false;fake::half_only=false;
    {
        ii::gui::detail::PlatformCanvas canvas(nullptr,nullptr,Acceleration::prefer_hardware);
        CanvasFrame frame{.viewport={0,0,100,80}};fake::fail_next_target_allocation=true;bool rejected=false;
        try{canvas.draw(frame,1,80);}catch(const std::runtime_error&){rejected=true;}
        fake::require(rejected&&fake::textures.empty()&&fake::framebuffers.empty(),"Failed WebGL target allocation leaked its resources");
        canvas.draw(frame,1,80);
        fake::require(canvas.info().presentations==1,"WebGL could not retry immediately after target allocation failed");
    }
    fake::require(fake::textures.empty()&&fake::framebuffers.empty(),"WebGL retry leaked target resources");
}
void retained_geometry(){
    fake::half=false;fake::half_only=false;fake::reset_events();
    {
        ii::gui::detail::PlatformCanvas canvas(nullptr,nullptr,Acceleration::prefer_hardware);
        const Panel red{{0,0,10,10},{1,0,0,1},{1,0,0,1}};
        const Panel green{{12,0,10,10},{0,1,0,1},{0,1,0,1}};
        CanvasFrame frame{.viewport={0,0,100,80}};frame.panels.push_back(red);
        frame.overlays={CanvasLayer{.panels={red}},CanvasLayer{.panels={green}},
                        CanvasLayer{.panels={red},.clip=Rect{20,0,20,20}}};
        canvas.draw(frame,1,80);auto info=canvas.info();
        std::vector<int> instances;for(const auto& draw:fake::draws)if(draw.program=="panels")instances.push_back(draw.instances);
        fake::require(instances==std::vector<int>({1,2,1})&&info.geometry_uploads==3&&info.geometry_draw_batches==3,
            "Compatible panel layers were not batched into retained buffers");
        frame.overlays.insert(frame.overlays.begin()+1,CanvasLayer{});
        fake::reset_events();canvas.draw(frame,1,80);auto unchanged=canvas.info();
        instances.clear();for(const auto& draw:fake::draws)if(draw.program=="panels")instances.push_back(draw.instances);
        fake::require(instances==std::vector<int>({1,2,1})&&unchanged.geometry_uploads==info.geometry_uploads,
            "An empty layer changed GPU batches or invalidated retained geometry");
        frame.overlays.erase(frame.overlays.begin()+1);
        canvas.draw(frame,1,80);info=canvas.info();
        frame.background={.2f,.1f,.1f,1};fake::reset_events();canvas.draw(frame,1,80);
        auto after=canvas.info();
        fake::require(after.geometry_uploads==info.geometry_uploads&&after.geometry_draw_batches==info.geometry_draw_batches+3,
            "An unrelated window change reuploaded panel geometry");
        frame.overlays[0].panels[0].bounds.x=3;info=after;canvas.draw(frame,1,80);after=canvas.info();
        fake::require(after.geometry_uploads==info.geometry_uploads+1,"A single changed batch did not upload exactly once");
        frame.overlays[2].clip=Rect{30,0,20,20};info=after;canvas.draw(frame,1,80);after=canvas.info();
        fake::require(after.geometry_uploads==info.geometry_uploads,"A clipping-only change reuploaded geometry");
        frame.overlays[2].content_transform.offset_x=3;info=after;canvas.draw(frame,1,80);after=canvas.info();
        fake::require(after.geometry_uploads==info.geometry_uploads+1,"A transform change did not update its batch");
        frame.viewport={0,0,120,90};info=after;canvas.draw(frame,1,90);after=canvas.info();
        fake::require(after.geometry_uploads==info.geometry_uploads,"A resize reuploaded unchanged geometry");
        frame.overlays[2].clip=Rect{200,0,20,20};info=after;canvas.draw(frame,1,90);after=canvas.info();
        frame.overlays[2].clip=Rect{30,0,20,20};canvas.draw(frame,1,90);after=canvas.info();
        fake::require(after.geometry_uploads==info.geometry_uploads,"An unchanged offscreen batch lost its geometry");
        frame.overlays[0].panels[0].bounds.x=4;info=after;const auto buffers_before=fake::buffers;
        fake::fail_next_buffer_upload=true;bool rejected=false;
        try{canvas.draw(frame,1,90);}catch(const std::runtime_error&){rejected=true;}
        fake::require(rejected&&!fake::buffers.contains(fake::failed_buffer)&&fake::buffers==buffers_before&&
                      canvas.info().geometry_uploads==info.geometry_uploads,"A failed geometry upload lost or leaked a retained buffer");
        canvas.draw(frame,1,90);after=canvas.info();
        fake::require(after.geometry_uploads==info.geometry_uploads+1,"A failed geometry upload was not retried");
        frame.overlays.clear();canvas.draw(frame,1,90);
        fake::require(fake::buffers.size()==1,"Removed geometry retained its GPU buffers");
    }
    fake::require(fake::buffers.empty()&&fake::textures.empty()&&fake::framebuffers.empty(),"Retained geometry leaked on close");
    {
        ii::gui::detail::PlatformCanvas canvas(nullptr,nullptr,Acceleration::prefer_hardware);
        auto frame=graph();canvas.draw(frame,1,80);const auto before=canvas.info();
        frame.viewport={0,0,120,90};for(auto& source:frame.scenes)source.viewport=frame.viewport;
        canvas.draw(frame,1,90);
        fake::require(canvas.info().geometry_uploads==before.geometry_uploads&&
                      canvas.info().scene_renders==before.scene_renders+3,
                      "Resizing source targets did not retain their unchanged geometry");
    }
    fake::require(fake::buffers.empty()&&fake::textures.empty()&&fake::framebuffers.empty(),"Source geometry leaked on close");
}
}
int main(){try{run(false);run(true);run(true,true);allocation_recovery();retained_geometry();std::cout<<"WEBGL_FAKE_OK\n";}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
