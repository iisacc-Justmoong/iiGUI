#import <AppKit/AppKit.h>
#import <Metal/Metal.h>
#import <MetalPerformanceShaders/MetalPerformanceShaders.h>
#import <QuartzCore/CAMetalLayer.h>
#include "NativeCanvas.h"
#include "NativeText.h"
#include <array>
#include <optional>

namespace ii::gui::detail {
namespace {
// The same instanced geometry, signed distance and premultiplied linear color
// equations are used by the D3D11 and GLES3 backends.
NSString* const shader = @R"metal(
#include <metal_stdlib>
using namespace metal;
struct Panel { float4 bounds, top, bottom; float radius, stroke, pad1, pad2; };
struct Varying { float4 position [[position]]; float2 local; float2 size;
                 float4 top, bottom; float radius, stroke; };
vertex Varying vertex_main(uint vertex_index [[vertex_id]], uint instance [[instance_id]],
                          const device Panel* panels [[buffer(0)]], constant float2& size [[buffer(1)]]) {
    const float2 corners[] = {float2(0,0),float2(1,0),float2(0,1),float2(0,1),float2(1,0),float2(1,1)};
    Panel p = panels[instance]; float2 uv = corners[vertex_index];
    float2 xy = p.bounds.xy + uv*p.bounds.zw;
    return {float4(xy.x/size.x*2-1, 1-xy.y/size.y*2, 0, 1), uv*p.bounds.zw,
            p.bounds.zw, p.top, p.bottom, p.radius,p.stroke};
}
fragment float4 fragment_main(Varying v [[stage_in]]) {
    float2 q = abs(v.local-v.size*0.5)-(v.size*0.5-v.radius);
    float distance = length(max(q,float2(0)))+min(max(q.x,q.y),0.0)-v.radius;
    float coverage = clamp(0.5-distance/max(fwidth(distance),0.0001),0.0,1.0);
    if(v.stroke>0){float radius=max(0.,v.radius-v.stroke);float2 q=abs(v.local-v.size*.5)-(v.size*.5-v.stroke-radius);float d=length(max(q,float2(0)))+min(max(q.x,q.y),0.)-radius;coverage=max(0.,coverage-clamp(.5-d/max(fwidth(d),.0001),0.,1.));}
    float4 color = mix(v.top,v.bottom,clamp(v.local.y/v.size.y,0.0,1.0));
    color.a *= coverage; return float4(color.rgb*color.a,color.a);
}
vertex float4 text_vertex(uint index [[vertex_id]]) {
    const float2 corners[]={float2(-1,1),float2(1,1),float2(-1,-1),float2(-1,-1),float2(1,1),float2(1,-1)};
    return float4(corners[index],0,1);
}
float4 premultiplied_read(texture2d<float> image,int2 p) {
    if(any(p<0)||p.x>=int(image.get_width())||p.y>=int(image.get_height()))return float4(0);
    float4 c=image.read(uint2(p));return float4(c.rgb*c.a,c.a);
}
struct ForegroundTransform {float4 local,offset;};
fragment float4 text_fragment(float4 position [[position]], texture2d<float> glyphs [[texture(0)]],constant ForegroundTransform& values [[buffer(0)]]) {
    const float4 transform=values.local;const float2 destination=position.xy-values.offset.xy;
    if(transform.z==1&&all(fract(values.offset.xy)==float2(0)))return premultiplied_read(glyphs,int2(destination-values.offset.zw))*transform.w;
    float2 source=(destination-transform.xy)/transform.z+transform.xy-values.offset.zw-.5;
    int2 p=int2(floor(source));float2 f=source-float2(p);
    float4 a=mix(premultiplied_read(glyphs,p),premultiplied_read(glyphs,p+int2(1,0)),f.x);
    float4 b=mix(premultiplied_read(glyphs,p+int2(0,1)),premultiplied_read(glyphs,p+int2(1,1)),f.x);
    return mix(a,b,f.y)*transform.w;
}
struct Material {float4 bounds,source,tint,border;float radius,border_width,pad0,pad1;};
float rounded_distance(float2 local,float2 size,float radius) {
    float2 q=abs(local-size*.5)-(size*.5-radius);
    return length(max(q,float2(0)))+min(max(q.x,q.y),0.)-radius;
}
fragment float4 material_fragment(float4 position [[position]],texture2d<float> image [[texture(0)]],constant Material& m [[buffer(0)]]) {
    float2 local=position.xy-m.bounds.xy;
    float coverage=clamp(.5-rounded_distance(local,m.bounds.zw,m.radius),0.,1.);
    float inner=clamp(.5-rounded_distance(local-m.border_width,m.bounds.zw-2*m.border_width,max(0.,m.radius-m.border_width)),0.,1.);
    inner=min(inner,coverage);
    uint2 uv=uint2(clamp(position.xy-m.source.xy,float2(0),m.source.zw-1));
    float4 base=image.read(uv);
    float3 fill=m.tint.rgb*m.tint.a+base.rgb*(1-m.tint.a);
    float3 border=m.border.rgb*m.border.a+base.rgb*(1-m.border.a);
    return float4(fill*inner+border*(coverage-inner),coverage);
}
struct Shadow {float4 source,color,viewport;};
struct ShadowVertex {float4 position [[position]];float2 uv;};
vertex ShadowVertex shadow_vertex(uint index [[vertex_id]],constant Shadow& s [[buffer(0)]]) {
    const float2 corners[]={float2(0,0),float2(1,0),float2(0,1),float2(0,1),float2(1,0),float2(1,1)};
    float2 uv=corners[index],xy=s.source.xy+uv*s.source.zw;
    return {float4(xy.x/s.viewport.x*2-1,1-xy.y/s.viewport.y*2,0,1),uv};
}
fragment float4 shadow_fragment(ShadowVertex v [[stage_in]],texture2d<float> mask [[texture(0)]],constant Shadow& s [[buffer(0)]]) {
    constexpr sampler filter(coord::normalized,address::clamp_to_zero,filter::linear);
    float alpha=mask.sample(filter,v.uv).r*s.color.a;
    return float4(s.color.rgb*alpha,alpha);
}
)metal";
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
std::runtime_error metal_error(NSError* error) {
    const auto* reason = error.localizedDescription.UTF8String;
    return std::runtime_error(reason ? reason : "Metal pipeline failed");
}
}

struct PlatformCanvas::Impl final {
    NSView* view = nil;
    CAMetalLayer* layer = nil;
    id<MTLDevice> device = nil;
    id<MTLCommandQueue> queue = nil;
    id<MTLLibrary> program=nil;
    id<MTLRenderPipelineState> pipeline = nil;
    id<MTLRenderPipelineState> text_pipeline = nil;
    id<MTLRenderPipelineState> material_pipeline = nil;
    id<MTLRenderPipelineState> shadow_mask_pipeline=nil,shadow_pipeline=nil;
    id<MTLTexture> texture = nil;
    id<MTLCommandBuffer> submitted = nil;
    struct MaterialCache final {
        id<MTLTexture> source=nil,blurred=nil;MPSImageGaussianBlur* kernel=nil;
        Rect region;float sigma=0;BackdropSource source_kind=BackdropSource::content;
        bool valid=false,copy_needed=false,blur_needed=false;
    };
    struct ShadowCache final {
        id<MTLTexture> mask=nil,blurred=nil;MPSImageGaussianBlur* kernel=nil;
        std::array<float,4> shape{};double scale=0;float padding=0;
        bool valid=false,dirty=false;
    };
    struct Layer final { id<MTLTexture> foreground=nil; ForegroundKey key;PixelClip region{};MaterialCache material;ShadowCache shadow; };
    std::vector<Layer> layers;
    std::optional<CanvasFrame> previous;
    double previous_scale=0;
    GraphicsInfo statistics;

    ~Impl() {
        [submitted waitUntilCompleted];
        [view removeFromSuperview];
    }
    void initialize(NSView* parent) {
        device = MTLCreateSystemDefaultDevice();
        require(device != nil, "No native Metal device is available");
        queue = [device newCommandQueue];
        require(queue != nil, "Cannot create a Metal command queue");
        NSError* error = nil;
        program = [device newLibraryWithSource:shader options:nil error:&error];
        auto library=program;
        if (!library) throw metal_error(error);
        auto descriptor = [MTLRenderPipelineDescriptor new];
        descriptor.vertexFunction = [library newFunctionWithName:@"vertex_main"];
        descriptor.fragmentFunction = [library newFunctionWithName:@"fragment_main"];
        auto color = descriptor.colorAttachments[0];
        color.pixelFormat = MTLPixelFormatBGRA8Unorm_sRGB;
        color.blendingEnabled = YES;
        color.sourceRGBBlendFactor = color.sourceAlphaBlendFactor = MTLBlendFactorOne;
        color.destinationRGBBlendFactor = color.destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
        pipeline = [device newRenderPipelineStateWithDescriptor:descriptor error:&error];
        if (!pipeline) throw metal_error(error);
        descriptor.vertexFunction=[library newFunctionWithName:@"text_vertex"];
        descriptor.fragmentFunction=[library newFunctionWithName:@"text_fragment"];
        text_pipeline=[device newRenderPipelineStateWithDescriptor:descriptor error:&error];
        if (!text_pipeline) throw metal_error(error);
        descriptor.fragmentFunction=[library newFunctionWithName:@"material_fragment"];
        material_pipeline=[device newRenderPipelineStateWithDescriptor:descriptor error:&error];
        if(!material_pipeline)throw metal_error(error);
        layer = [CAMetalLayer layer];
        layer.device = device;
        layer.pixelFormat = MTLPixelFormatBGRA8Unorm_sRGB;
        layer.framebufferOnly = NO; // GPU blit destination; no CPU access to drawables.
        layer.maximumDrawableCount = 2;
        layer.displaySyncEnabled = YES;
        layer.allowsNextDrawableTimeout = YES;
        layer.opaque = YES;
        CGColorSpaceRef space = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
        layer.colorspace = space;
        CGColorSpaceRelease(space);
        view = [[NSView alloc] initWithFrame:NSZeroRect];
        view.wantsLayer = YES;
        view.layer = layer;
        [parent addSubview:view];
        statistics.api = GraphicsApi::metal;
        statistics.adapter = device.name.UTF8String;
        statistics.hardware = true; // MTLCreateSystemDefaultDevice returns a Metal GPU.
        statistics.srgb_attachment = true;
        statistics.backdrop_supported=true;
        statistics.shadow_supported=true;
        statistics.content_transform_supported=true;
    }
    void initialize_shadow_pipelines() {
        if(shadow_pipeline)return;
        NSError* error=nil;
        auto descriptor=[MTLRenderPipelineDescriptor new];
        descriptor.vertexFunction=[program newFunctionWithName:@"vertex_main"];
        descriptor.fragmentFunction=[program newFunctionWithName:@"fragment_main"];
        descriptor.colorAttachments[0].pixelFormat=MTLPixelFormatR8Unorm;
        shadow_mask_pipeline=[device newRenderPipelineStateWithDescriptor:descriptor error:&error];
        if(!shadow_mask_pipeline)throw metal_error(error);
        descriptor.vertexFunction=[program newFunctionWithName:@"shadow_vertex"];
        descriptor.fragmentFunction=[program newFunctionWithName:@"shadow_fragment"];
        auto color=descriptor.colorAttachments[0];color.pixelFormat=MTLPixelFormatBGRA8Unorm_sRGB;
        color.blendingEnabled=YES;color.sourceRGBBlendFactor=color.sourceAlphaBlendFactor=MTLBlendFactorOne;
        color.destinationRGBBlendFactor=color.destinationAlphaBlendFactor=MTLBlendFactorOneMinusSourceAlpha;
        shadow_pipeline=[device newRenderPipelineStateWithDescriptor:descriptor error:&error];
        if(!shadow_pipeline)throw metal_error(error);
    }
    std::pair<std::uint32_t,std::uint32_t> shadow_extent(const BoxShadow& shadow,double scale) {
        const auto padding=std::ceil(shadow.blur_radius*scale);
        const auto width=std::ceil(shadow.bounds.width*scale+2*padding),height=std::ceil(shadow.bounds.height*scale+2*padding);
        require(width>=1&&height>=1&&width<=8192&&height<=8192,"Shadow working texture dimensions must be 1..8192 pixels");
        return {std::uint32_t(width),std::uint32_t(height)};
    }
    void prepare_materials(const CanvasFrame& frame,double scale,std::uint32_t width,std::uint32_t height,const std::vector<CanvasLayerView>& views) {
        const bool base_changed=!previous||previous_scale!=scale||previous->viewport!=frame.viewport||previous->background!=frame.background||previous->panels!=frame.panels||previous->texts!=frame.texts||previous->images!=frame.images||previous->images_before_text!=frame.images_before_text;
        std::uint64_t bytes=0;std::vector<Rect> regions(views.size());
        for(std::size_t i=0;i<views.size();++i)if(views[i].backdrop){const auto& m=*views[i].backdrop;auto region=views[i];region.content_transform={};const auto p=m.blur_radius;region.clip=Rect{m.bounds.x-p,m.bounds.y-p,m.bounds.width+2*p,m.bounds.height+2*p};auto r=clip_pixels(region,scale,width,height);regions[i]={float(r.x),float(r.y),float(r.width),float(r.height)};bytes+=std::uint64_t(r.width)*r.height*(m.blur_radius>0?12:4);}
        std::uint64_t shadow_bytes=0;
        for(const auto& layer:views)if(layer.shadow){const auto [w,h]=shadow_extent(*layer.shadow,scale);shadow_bytes+=std::uint64_t(w)*h*(layer.shadow->blur_radius>0?3:1);}
        require(bytes+shadow_bytes<=64ull*1024*1024,"GPU shadow/backdrop working textures exceed 64 MiB");
        for(std::size_t i=0;i<views.size();++i){auto& cached=layers[i].material;if(!views[i].backdrop||regions[i].width<=0||regions[i].height<=0){cached={};continue;}const auto& m=*views[i].backdrop;const auto region=regions[i];const auto sigma=m.blur_radius>0?float(std::max(.01,double(m.blur_radius)*scale/3)):0.f;
            const bool geometry_changed=cached.region!=region;cached.copy_needed=!cached.valid||base_changed||geometry_changed||cached.source_kind!=m.source||m.source==BackdropSource::previous_layers;cached.blur_needed=sigma>0&&(cached.copy_needed||cached.sigma!=sigma||!cached.blurred);
            if(!cached.source||cached.source.width!=region.width||cached.source.height!=region.height){auto d=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm_sRGB width:NSUInteger(region.width) height:NSUInteger(region.height) mipmapped:NO];d.storageMode=MTLStorageModePrivate;d.usage=MTLTextureUsageShaderRead;cached.source=[device newTextureWithDescriptor:d];require(cached.source!=nil,"Cannot allocate GPU backdrop crop");cached.copy_needed=true;cached.blur_needed=sigma>0;}
            if(sigma>0){if(!cached.blurred||cached.blurred.width!=region.width||cached.blurred.height!=region.height){auto d=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA16Float width:NSUInteger(region.width) height:NSUInteger(region.height) mipmapped:NO];d.storageMode=MTLStorageModePrivate;d.usage=MTLTextureUsageShaderRead|MTLTextureUsageShaderWrite;cached.blurred=[device newTextureWithDescriptor:d];require(cached.blurred!=nil,"Cannot allocate linear GPU blur target");cached.blur_needed=true;}if(!cached.kernel||cached.sigma!=sigma){cached.kernel=[[MPSImageGaussianBlur alloc] initWithDevice:device sigma:sigma];cached.kernel.edgeMode=MPSImageEdgeModeClamp;require(cached.kernel!=nil,"Cannot create native GPU Gaussian kernel");cached.blur_needed=true;}}
            else {cached.blurred=nil;cached.kernel=nil;}
            cached.region=region;cached.sigma=sigma;cached.source_kind=m.source;if(cached.copy_needed||cached.blur_needed)cached.valid=false;
        }
        statistics.backdrop_texture_bytes=bytes;
        statistics.shadow_texture_bytes=shadow_bytes;
    }
    void encode_shadows(id<MTLCommandBuffer> command,const std::vector<CanvasLayerView>& views,double scale,std::uint64_t& masks,std::uint64_t& blurs) {
        for(std::size_t index=0;index<views.size();++index){
            auto& cached=layers[index].shadow;
            if(!views[index].shadow){cached={};continue;}
            const auto& shadow=*views[index].shadow;
            const std::array<float,4> shape{shadow.bounds.width,shadow.bounds.height,shadow.radius,shadow.blur_radius};
            cached.dirty=!cached.valid||cached.shape!=shape||cached.scale!=scale;
            if(!cached.dirty)continue;
            initialize_shadow_pipelines();
            const auto [width,height]=shadow_extent(shadow,scale);
            if(!cached.mask||cached.mask.width!=width||cached.mask.height!=height){
                auto descriptor=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatR8Unorm width:width height:height mipmapped:NO];
                descriptor.storageMode=MTLStorageModePrivate;descriptor.usage=MTLTextureUsageRenderTarget|MTLTextureUsageShaderRead;
                cached.mask=[device newTextureWithDescriptor:descriptor];require(cached.mask!=nil,"Cannot allocate shadow mask");
                cached.blurred=nil;
            }
            const auto padding=float(std::ceil(shadow.blur_radius*scale));
            const GpuPanel panel{{padding,padding,float(shadow.bounds.width*scale),float(shadow.bounds.height*scale)},{1,1,1,1},{1,1,1,1},float(shadow.radius*scale),{0,0,0}};
            auto pass=[MTLRenderPassDescriptor renderPassDescriptor];auto attachment=pass.colorAttachments[0];
            attachment.texture=cached.mask;attachment.loadAction=MTLLoadActionClear;attachment.storeAction=MTLStoreActionStore;
            attachment.clearColor=MTLClearColorMake(0,0,0,0);
            auto encoder=[command renderCommandEncoderWithDescriptor:pass];require(encoder!=nil,"Cannot create shadow mask encoder");
            [encoder setRenderPipelineState:shadow_mask_pipeline];
            [encoder setVertexBytes:&panel length:sizeof(panel) atIndex:0];const float extent[]{float(width),float(height)};
            [encoder setVertexBytes:extent length:sizeof(extent) atIndex:1];
            [encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:6 instanceCount:1];[encoder endEncoding];++masks;
            if(shadow.blur_radius>0){
                if(!cached.blurred){auto descriptor=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatR16Float width:width height:height mipmapped:NO];descriptor.storageMode=MTLStorageModePrivate;descriptor.usage=MTLTextureUsageShaderRead|MTLTextureUsageShaderWrite;cached.blurred=[device newTextureWithDescriptor:descriptor];require(cached.blurred!=nil,"Cannot allocate blurred shadow mask");}
                const auto sigma=float(std::max(.01,double(shadow.blur_radius)*scale/3));
                if(!cached.kernel||cached.shape[3]!=shape[3]||cached.scale!=scale){cached.kernel=[[MPSImageGaussianBlur alloc] initWithDevice:device sigma:sigma];cached.kernel.edgeMode=MPSImageEdgeModeZero;require(cached.kernel!=nil,"Cannot create shadow Gaussian kernel");}
                [cached.kernel encodeToCommandBuffer:command sourceTexture:cached.mask destinationTexture:cached.blurred];++blurs;
            }else{cached.blurred=nil;cached.kernel=nil;}
            cached.shape=shape;cached.scale=scale;cached.padding=padding;cached.valid=false;
        }
    }
    void encode_material_source(id<MTLCommandBuffer> command,MaterialCache& cached,std::uint64_t& copies,std::uint64_t& blurs) {
        if(cached.copy_needed){auto encoder=[command blitCommandEncoder];require(encoder!=nil,"Cannot create GPU backdrop copy encoder");[encoder copyFromTexture:texture sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(NSUInteger(cached.region.x),NSUInteger(cached.region.y),0) sourceSize:MTLSizeMake(NSUInteger(cached.region.width),NSUInteger(cached.region.height),1) toTexture:cached.source destinationSlice:0 destinationLevel:0 destinationOrigin:MTLOriginMake(0,0,0)];[encoder endEncoding];++copies;}
        if(cached.blur_needed){[cached.kernel encodeToCommandBuffer:command sourceTexture:cached.source destinationTexture:cached.blurred];++blurs;}
    }
    void draw(const CanvasFrame& frame, double scale, double height) {
        const auto [width, pixels_high] = canvas_pixels(frame, scale);
        view.frame = NSMakeRect(frame.viewport.x, height-frame.viewport.y-frame.viewport.height,
                               frame.viewport.width, frame.viewport.height);
        layer.contentsScale = scale;
        layer.drawableSize = CGSizeMake(width, pixels_high);
        bool changed = !previous || *previous != frame || previous_scale!=scale;
        if (!texture || texture.width != width || texture.height != pixels_high) {
            auto descriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm_sRGB
                width:width height:pixels_high mipmapped:NO];
            descriptor.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
            descriptor.storageMode = MTLStorageModePrivate;
            auto replacement = [device newTextureWithDescriptor:descriptor];
            require(replacement != nil, "Cannot allocate the Metal canvas attachment");
            texture = replacement;
            statistics.pixel_width = width; statistics.pixel_height = pixels_high;
            statistics.cached_texture_bytes = std::uint64_t(width)*pixels_high*4;
            changed = true;
        }
        auto command = [queue commandBuffer];
        require(command != nil, "Cannot allocate a Metal command buffer");
        std::uint64_t copies=0,blurs=0,shadow_masks=0,shadow_blurs=0;
        if (changed) {
            auto pass = [MTLRenderPassDescriptor renderPassDescriptor];
            auto color = pass.colorAttachments[0];
            color.texture = texture;
            color.loadAction = MTLLoadActionClear;
            color.storeAction = MTLStoreActionStore;
            color.clearColor = MTLClearColorMake(frame.background.red, frame.background.green,
                                                frame.background.blue, 1);
            const auto views=canvas_layers(frame); layers.resize(views.size());
            prepare_materials(frame,scale,width,pixels_high,views);
            encode_shadows(command,views,scale,shadow_masks,shadow_blurs);
            auto encoder = [command renderCommandEncoderWithDescriptor:pass];
            require(encoder != nil, "Cannot create a Metal render encoder");
            statistics.foreground_texture_bytes=0;
            for(std::size_t index=0;index<views.size();++index) {
                if(index==1){bool pending=false;for(const auto& l:layers)pending=pending||(l.material.source_kind==BackdropSource::content&&(l.material.copy_needed||l.material.blur_needed));if(pending){[encoder endEncoding];for(auto& l:layers)if(l.material.source_kind==BackdropSource::content&&(l.material.copy_needed||l.material.blur_needed))encode_material_source(command,l.material,copies,blurs);color.loadAction=MTLLoadActionLoad;encoder=[command renderCommandEncoderWithDescriptor:pass];require(encoder!=nil,"Cannot resume Metal render after content capture");}}
                const auto content=views[index]; auto& cached=layers[index];
                const auto clip=clip_pixels(content,scale,width,pixels_high);
                if(!clip.width || !clip.height) { cached.foreground=nil;cached.key={};continue; }
                [encoder setScissorRect:MTLScissorRect{clip.x,clip.y,clip.width,clip.height}];
                if(content.shadow&&cached.shadow.mask){
                    const auto& shadow=*content.shadow;const auto& mask=cached.shadow;
                    struct alignas(16) Uniform final {float source[4],color[4],viewport[4];};static_assert(sizeof(Uniform)==48);
                    Uniform u{{float(shadow.bounds.x*scale-mask.padding),float(shadow.bounds.y*scale-mask.padding),float(mask.mask.width),float(mask.mask.height)},
                        {shadow.color.red,shadow.color.green,shadow.color.blue,shadow.color.alpha},{float(width),float(pixels_high),0,0}};
                    [encoder setRenderPipelineState:shadow_pipeline];[encoder setVertexBytes:&u length:sizeof(u) atIndex:0];
                    [encoder setFragmentBytes:&u length:sizeof(u) atIndex:0];[encoder setFragmentTexture:mask.blurred?mask.blurred:mask.mask atIndex:0];
                    [encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:6];
                }
                if(content.backdrop&&cached.material.source){auto& m=cached.material;if(m.source_kind==BackdropSource::previous_layers&&(m.copy_needed||m.blur_needed)){[encoder endEncoding];encode_material_source(command,m,copies,blurs);color.loadAction=MTLLoadActionLoad;encoder=[command renderCommandEncoderWithDescriptor:pass];require(encoder!=nil,"Cannot resume Metal render after material capture");[encoder setScissorRect:MTLScissorRect{clip.x,clip.y,clip.width,clip.height}];}
                    const auto& f=*content.backdrop;struct alignas(16) Uniform final {float bounds[4],source[4],tint[4],border[4],radius,border_width,padding[2];};static_assert(sizeof(Uniform)==80);Uniform u{{float(f.bounds.x*scale),float(f.bounds.y*scale),float(f.bounds.width*scale),float(f.bounds.height*scale)},{m.region.x,m.region.y,m.region.width,m.region.height},{f.tint.red,f.tint.green,f.tint.blue,f.tint.alpha},{f.border.red,f.border.green,f.border.blue,f.border.alpha},float(f.radius*scale),float(std::min(double(f.border_width),double(std::min(f.bounds.width,f.bounds.height)/2))*scale),{0,0}};
                    [encoder setRenderPipelineState:material_pipeline];[encoder setFragmentTexture:m.blurred?m.blurred:m.source atIndex:0];[encoder setFragmentBytes:&u length:sizeof(u) atIndex:0];[encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:6];
                }
                [encoder setRenderPipelineState:pipeline];
                const auto panels=pack_panels(content.panels,content.content_transform);
                if(!panels.empty()) {
                    auto buffer=[device newBufferWithBytes:panels.data() length:panels.size()*sizeof(GpuPanel) options:MTLResourceStorageModeShared];
                    require(buffer!=nil,"Cannot allocate Metal panel instances");
                    [encoder setVertexBuffer:buffer offset:0 atIndex:0];
                    const float size[]{frame.viewport.width,frame.viewport.height}; [encoder setVertexBytes:size length:sizeof(size) atIndex:1];
                    [encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:6 instanceCount:panels.size()];
                }
                if(content.foreground()) {
                    if(!cached.foreground || !cached.key.matches(frame.viewport,scale,content)) {
                        auto raster=rasterize_foreground_region(frame.viewport,content,scale);auto& bitmap=raster.pixels;cached.region=raster.region;
                        auto descriptor=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm_sRGB width:bitmap.width height:bitmap.height mipmapped:NO];
                        descriptor.usage=MTLTextureUsageShaderRead; descriptor.storageMode=MTLStorageModeShared;
                        cached.foreground=[device newTextureWithDescriptor:descriptor]; require(cached.foreground!=nil,"Cannot allocate Metal foreground texture");
                        [cached.foreground replaceRegion:MTLRegionMake2D(0,0,bitmap.width,bitmap.height) mipmapLevel:0 withBytes:bitmap.rgba.data() bytesPerRow:std::size_t(bitmap.width)*4];
                        cached.key.assign(frame.viewport,scale,content); ++statistics.foreground_rasterizations;
                    }
                    statistics.foreground_texture_bytes+=std::uint64_t(cached.region.width)*cached.region.height*4;
                    [encoder setRenderPipelineState:text_pipeline]; [encoder setFragmentTexture:cached.foreground atIndex:0];
                    const auto& t=content.content_transform;const float transform[]{float(t.origin_x*scale),float(t.origin_y*scale),t.scale,t.opacity,float(t.offset_x*scale),float(t.offset_y*scale),float(cached.region.x),float(cached.region.y)};
                    [encoder setFragmentBytes:transform length:sizeof(transform) atIndex:0];
                    [encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:6];
                } else { cached.foreground=nil; cached.key={}; }
            }
            [encoder endEncoding];
            ++statistics.scene_renders;
        }
        auto drawable = [layer nextDrawable];
        if (drawable) {
            auto copy = [command blitCommandEncoder];
            require(copy != nil, "Cannot create a Metal blit encoder");
            [copy copyFromTexture:texture sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0,0,0)
                sourceSize:MTLSizeMake(width,pixels_high,1) toTexture:drawable.texture
                destinationSlice:0 destinationLevel:0 destinationOrigin:MTLOriginMake(0,0,0)];
            [copy endEncoding];
            [command presentDrawable:drawable];
            ++statistics.presentations;
        }
        [command commit];
        for(auto& l:layers)if(l.material.source){l.material.valid=true;l.material.copy_needed=l.material.blur_needed=false;}
        for(auto& l:layers)if(l.shadow.mask){l.shadow.valid=true;l.shadow.dirty=false;}
        statistics.backdrop_copies+=copies;statistics.backdrop_blurs+=blurs;
        statistics.shadow_masks+=shadow_masks;statistics.shadow_blurs+=shadow_blurs;
        submitted = command;
        previous = frame;previous_scale=scale;
    }
    PixelBuffer capture() {
        require(texture != nil, "Metal canvas has not rendered");
        const auto width = statistics.pixel_width, height = statistics.pixel_height;
        const auto stride = (std::size_t(width)*4+255)&~std::size_t(255);
        auto buffer = [device newBufferWithLength:stride*height options:MTLResourceStorageModeShared];
        require(buffer != nil, "Cannot allocate Metal capture storage");
        auto command = [queue commandBuffer];
        auto copy = [command blitCommandEncoder];
        require(command != nil && copy != nil, "Cannot create a Metal capture command");
        [copy copyFromTexture:texture sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0,0,0)
            sourceSize:MTLSizeMake(width,height,1) toBuffer:buffer destinationOffset:0
            destinationBytesPerRow:stride destinationBytesPerImage:stride*height];
        [copy endEncoding]; [command commit]; [command waitUntilCompleted];
        if (command.status == MTLCommandBufferStatusError) throw metal_error(command.error);
        PixelBuffer result{width,height,std::vector<std::uint8_t>(std::size_t(width)*height*4)};
        const auto* source = static_cast<const std::uint8_t*>(buffer.contents);
        for (std::size_t y=0; y<height; ++y) for (std::size_t x=0; x<width; ++x) {
            const auto* pixel = source+y*stride+x*4;
            auto* target = result.rgba.data()+(y*width+x)*4;
            target[0]=pixel[2]; target[1]=pixel[1]; target[2]=pixel[0]; target[3]=pixel[3];
        }
        return result;
    }
};
PlatformCanvas::PlatformCanvas(void* parent, void*, Acceleration) : impl_(std::make_unique<Impl>()) {
    impl_->initialize((__bridge NSView*)parent);
}
PlatformCanvas::~PlatformCanvas() = default;
void PlatformCanvas::draw(const CanvasFrame& f,double s,double h) { impl_->draw(f,s,h); }
PixelBuffer PlatformCanvas::capture() { return impl_->capture(); }
GraphicsInfo PlatformCanvas::info() const { return impl_->statistics; }
bool PlatformCanvas::native_event(const void*) { return false; }
} // namespace ii::gui::detail
