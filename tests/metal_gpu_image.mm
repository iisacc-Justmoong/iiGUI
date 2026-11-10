#import <Foundation/Foundation.h>
#import <Metal/Metal.h>

#include "../src/Graphics/GpuImage.h"
#include "../src/Text/NativeText.h"
#include <iiGUI/Image.h>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <vector>

using namespace ii::gui;
using namespace ii::gui::detail;
namespace {
void check(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
float decode(float v){return v<=.04045f?v/12.92f:std::pow((v+.055f)/1.055f,2.4f);}
float encode(float v){return v<=.0031308f?v*12.92f:1.055f*std::pow(v,1.f/2.4f)-.055f;}
NSString* shader_source(){
    std::ifstream input(std::string(IIGUI_SOURCE_DIRECTORY)+"/src/Graphics/CanvasMetal.mm");
    std::string source((std::istreambuf_iterator<char>(input)),{});
    const std::string begin="NSString* const shader = @R\"metal(",end=")metal\";";
    auto first=source.find(begin);check(first!=source.npos,"Metal shader source was not found");first+=begin.size();
    auto last=source.find(end,first);check(last!=source.npos,"Metal shader source is incomplete");
    return [[NSString alloc] initWithBytes:source.data()+first length:last-first encoding:NSUTF8StringEncoding];
}
}
int main(){
    @autoreleasepool {
      try {
        id<MTLDevice> device=MTLCreateSystemDefaultDevice();check(device!=nil,"Metal device is unavailable");
        NSError* error=nil;id<MTLLibrary> library=[device newLibraryWithSource:shader_source() options:nil error:&error];
        if(!library)throw std::runtime_error(error.localizedDescription.UTF8String);
        auto descriptor=[MTLRenderPipelineDescriptor new];
        descriptor.vertexFunction=[library newFunctionWithName:@"image_vertex"];
        descriptor.fragmentFunction=[library newFunctionWithName:@"image_fragment"];
        auto attachment=descriptor.colorAttachments[0];attachment.pixelFormat=MTLPixelFormatRGBA8Unorm_sRGB;
        attachment.blendingEnabled=YES;attachment.sourceRGBBlendFactor=attachment.sourceAlphaBlendFactor=MTLBlendFactorOne;
        attachment.destinationRGBBlendFactor=attachment.destinationAlphaBlendFactor=MTLBlendFactorOneMinusSourceAlpha;
        auto pipeline=[device newRenderPipelineStateWithDescriptor:descriptor error:&error];
        if(!pipeline)throw std::runtime_error(error.localizedDescription.UTF8String);
        auto queue=[device newCommandQueue];check(queue!=nil,"Cannot create Metal test queue");
        std::vector<std::uint8_t> pixels{255,0,0,255, 0,255,0,0, 0,0,255,128, 255,255,255,255};
        auto image=rgba_image(2,2,pixels);check(bool(image),"Cannot create image test input");
        auto source_desc=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm_sRGB width:2 height:2 mipmapped:NO];
        source_desc.usage=MTLTextureUsageShaderRead;source_desc.storageMode=MTLStorageModeShared;
        auto source=[device newTextureWithDescriptor:source_desc];check(source!=nil,"Cannot create Metal image input");
        [source replaceRegion:MTLRegionMake2D(0,0,2,2) mipmapLevel:0 withBytes:pixels.data() bytesPerRow:8];
        for(int variant=0;variant<6;++variant){
            CanvasFrame frame{.viewport={0,0,16,16}};
            ImageRun run{.bounds={2,2,12,12},.image=*image};
            if(variant==1){run.tint=Color{.2f,.8f,.3f,.6f};run.opacity=.75f;}
            if(variant==2){run.radius=4;run.border_width=1.5f;run.border={0,1,0,.8f};}
            if(variant==3){run.source={.25f,0,.5f,1};run.gradient=ImageGradient{.height=12,.middle_position=.3f};}
            if(variant==4){run.bounds={2.75f,2.75f,10.5f,10.5f};}
            if(variant==5){run.bounds={2.75f,2.75f,10.5f,10.5f};run.radius=3.5f;run.border_width=6;run.border={0,1,0,.8f};}
            frame.images.push_back(run);
            const auto expected=rasterize_foreground(frame.viewport,canvas_layers(frame).front(),1);
            auto output_desc=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm_sRGB width:16 height:16 mipmapped:NO];
            output_desc.usage=MTLTextureUsageRenderTarget;output_desc.storageMode=MTLStorageModeShared;
            auto output=[device newTextureWithDescriptor:output_desc];check(output!=nil,"Cannot create Metal image output");
            auto pass=[MTLRenderPassDescriptor renderPassDescriptor];pass.colorAttachments[0].texture=output;
            pass.colorAttachments[0].loadAction=MTLLoadActionClear;pass.colorAttachments[0].storeAction=MTLStoreActionStore;
            pass.colorAttachments[0].clearColor=MTLClearColorMake(0,0,0,0);
            auto command=[queue commandBuffer];auto encoder=[command renderCommandEncoderWithDescriptor:pass];
            check(command!=nil&&encoder!=nil,"Cannot encode Metal image test");
            const auto uniform=gpu_image_uniform(run,{},1,frame.viewport);
            [encoder setRenderPipelineState:pipeline];[encoder setVertexBytes:&uniform length:sizeof(uniform) atIndex:0];
            [encoder setFragmentTexture:source atIndex:0];[encoder setFragmentBytes:&uniform length:sizeof(uniform) atIndex:0];
            [encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:6];[encoder endEncoding];
            [command commit];[command waitUntilCompleted];
            if(command.status==MTLCommandBufferStatusError)throw std::runtime_error(command.error.localizedDescription.UTF8String);
            std::vector<std::uint8_t> actual(16*16*4);[output getBytes:actual.data() bytesPerRow:64 fromRegion:MTLRegionMake2D(0,0,16,16) mipmapLevel:0];
            for(std::size_t i=0;i<actual.size();i+=4){
                const float alpha=expected.rgba[i+3]/255.f;
                if(std::abs(int(actual[i+3])-int(expected.rgba[i+3]))>5)throw std::runtime_error("Metal image alpha differs from reference in case "+std::to_string(variant)+" at pixel "+std::to_string(i/4));
                for(unsigned c=0;c<3;++c){
                    const auto premultiplied=int(std::lround(encode(decode(expected.rgba[i+c]/255.f)*alpha)*255));
                    if(std::abs(int(actual[i+c])-premultiplied)>7)throw std::runtime_error("Metal image color differs from reference in case "+std::to_string(variant)+" at pixel "+std::to_string(i/4));
                }
            }
        }
        std::cout << "METAL_GPU_IMAGE_OK\n";
        return 0;
      }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
    }
}
