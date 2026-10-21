#import <AppKit/AppKit.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#include "NativeCanvas.h"
#include "NativeText.h"
#include <optional>

namespace ii::gui::detail {
namespace {
// The same instanced geometry, signed distance and premultiplied linear color
// equations are used by the D3D11 and GLES3 backends.
NSString* const shader = @R"metal(
#include <metal_stdlib>
using namespace metal;
struct Panel { float4 bounds, top, bottom; float radius, pad0, pad1, pad2; };
struct Varying { float4 position [[position]]; float2 local; float2 size;
                 float4 top, bottom; float radius; };
vertex Varying vertex_main(uint vertex_index [[vertex_id]], uint instance [[instance_id]],
                          const device Panel* panels [[buffer(0)]], constant float2& size [[buffer(1)]]) {
    const float2 corners[] = {float2(0,0),float2(1,0),float2(0,1),float2(0,1),float2(1,0),float2(1,1)};
    Panel p = panels[instance]; float2 uv = corners[vertex_index];
    float2 xy = p.bounds.xy + uv*p.bounds.zw;
    return {float4(xy.x/size.x*2-1, 1-xy.y/size.y*2, 0, 1), uv*p.bounds.zw,
            p.bounds.zw, p.top, p.bottom, p.radius};
}
fragment float4 fragment_main(Varying v [[stage_in]]) {
    float2 q = abs(v.local-v.size*0.5)-(v.size*0.5-v.radius);
    float distance = length(max(q,float2(0)))+min(max(q.x,q.y),0.0)-v.radius;
    float coverage = clamp(0.5-distance/max(fwidth(distance),0.0001),0.0,1.0);
    float4 color = mix(v.top,v.bottom,clamp(v.local.y/v.size.y,0.0,1.0));
    color.a *= coverage; return float4(color.rgb*color.a,color.a);
}
vertex float4 text_vertex(uint index [[vertex_id]]) {
    const float2 corners[]={float2(-1,1),float2(1,1),float2(-1,-1),float2(-1,-1),float2(1,1),float2(1,-1)};
    return float4(corners[index],0,1);
}
fragment float4 text_fragment(float4 position [[position]], texture2d<float> glyphs [[texture(0)]]) {
    float4 color=glyphs.read(uint2(position.xy));
    return float4(color.rgb*color.a,color.a);
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
    id<MTLRenderPipelineState> pipeline = nil;
    id<MTLRenderPipelineState> text_pipeline = nil;
    id<MTLTexture> texture = nil;
    id<MTLCommandBuffer> submitted = nil;
    std::optional<CanvasFrame> previous;
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
        auto library = [device newLibraryWithSource:shader options:nil error:&error];
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
    }
    void draw(const CanvasFrame& frame, double scale, double height) {
        const auto [width, pixels_high] = canvas_pixels(frame, scale);
        view.frame = NSMakeRect(frame.viewport.x, height-frame.viewport.y-frame.viewport.height,
                               frame.viewport.width, frame.viewport.height);
        layer.contentsScale = scale;
        layer.drawableSize = CGSizeMake(width, pixels_high);
        bool changed = !previous || *previous != frame;
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
        if (changed) {
            auto pass = [MTLRenderPassDescriptor renderPassDescriptor];
            auto color = pass.colorAttachments[0];
            color.texture = texture;
            color.loadAction = MTLLoadActionClear;
            color.storeAction = MTLStoreActionStore;
            color.clearColor = MTLClearColorMake(frame.background.red, frame.background.green,
                                                frame.background.blue, 1);
            auto encoder = [command renderCommandEncoderWithDescriptor:pass];
            require(encoder != nil, "Cannot create a Metal render encoder");
            [encoder setRenderPipelineState:pipeline];
            const auto panels = pack_panels(frame);
            if (!panels.empty()) {
                auto buffer = [device newBufferWithBytes:panels.data() length:panels.size()*sizeof(GpuPanel)
                    options:MTLResourceStorageModeShared];
                require(buffer != nil, "Cannot allocate Metal panel instances");
                [encoder setVertexBuffer:buffer offset:0 atIndex:0];
                const float size[]{frame.viewport.width, frame.viewport.height};
                [encoder setVertexBytes:size length:sizeof(size) atIndex:1];
                [encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:6 instanceCount:panels.size()];
            }
            if (!frame.texts.empty()) {
                const auto bitmap=rasterize_text(frame,scale);
                auto descriptor=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm_sRGB
                    width:width height:pixels_high mipmapped:NO];
                descriptor.usage=MTLTextureUsageShaderRead; descriptor.storageMode=MTLStorageModeShared;
                auto glyphs=[device newTextureWithDescriptor:descriptor];
                require(glyphs!=nil,"Cannot allocate Metal text texture");
                [glyphs replaceRegion:MTLRegionMake2D(0,0,width,pixels_high) mipmapLevel:0
                    withBytes:bitmap.rgba.data() bytesPerRow:std::size_t(width)*4];
                [encoder setRenderPipelineState:text_pipeline];
                [encoder setFragmentTexture:glyphs atIndex:0];
                [encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:6];
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
        submitted = command;
        previous = frame;
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
