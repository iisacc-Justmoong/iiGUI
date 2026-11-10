#import <Foundation/Foundation.h>
#import <Metal/Metal.h>

#include "../src/Graphics/MetalImagePool.h"
#include <iiGUI/Image.h>
#include <iostream>
#include <stdexcept>

using namespace ii::gui;
using namespace ii::gui::detail;
namespace {
void check(bool condition, const char *message) { if (!condition) throw std::runtime_error(message); }
std::shared_ptr<const RasterImage> image(std::uint32_t width, std::uint32_t height, std::uint8_t red) {
    std::vector<std::uint8_t> bytes(std::size_t(width) * height * 4);
    for (std::size_t i=0; i<bytes.size(); i+=4) { bytes[i]=red; bytes[i+3]=255; }
    auto result=rgba_image(width,height,std::move(bytes));
    check(bool(result),"cannot create test image"); return *result;
}
MetalImagePool::Wanted wanted(std::initializer_list<std::shared_ptr<const RasterImage>> images) {
    MetalImagePool::Wanted result;
    for (const auto &value:images) result.emplace(value.get(),value);
    return result;
}
}
int main() {
    @autoreleasepool {
        try {
            id<MTLDevice> device=MTLCreateSystemDefaultDevice();check(device!=nil,"Metal device unavailable");
            id<MTLCommandQueue> queue=[device newCommandQueue];check(queue!=nil,"Metal queue unavailable");
            MetalImagePool pool;
            auto a=image(2,2,10),b=image(2,2,20),c=image(2,2,30),d=image(2,2,40),e=image(3,2,50);
            pool.prepare(device,wanted({a}),nil);
            auto first=pool.texture(a.get());check(first!=nil&&pool.allocations()==1&&pool.uploads()==1,"initial texture missing");
            pool.prepare(device,wanted({a}),nil);
            check(pool.texture(a.get())==first&&pool.allocations()==1&&pool.uploads()==1,"unchanged image was uploaded again");
            auto busy=[queue commandBuffer];
            pool.prepare(device,wanted({b}),busy);
            check(pool.texture(b.get())!=first&&pool.allocations()==2&&pool.reuses()==0,"unfinished frame reused its texture");
            auto completed=[queue commandBuffer];[completed commit];[completed waitUntilCompleted];
            check(completed.status==MTLCommandBufferStatusCompleted,"test command failed");
            auto retired=pool.texture(b.get());
            pool.prepare(device,wanted({c}),completed);
            check(pool.texture(c.get())==retired&&pool.allocations()==2&&pool.reuses()==1&&pool.uploads()==3,
                  "completed frame texture was not reused");
            std::uint8_t pixels[16]{};
            [pool.texture(c.get()) getBytes:pixels bytesPerRow:8 fromRegion:MTLRegionMake2D(0,0,2,2) mipmapLevel:0];
            check(pixels[0]==30&&pixels[3]==255,"reused texture did not receive the replacement pixels");
            pool.prepare(device,wanted({c,d}),completed);
            check(pool.texture(c.get())==retired&&pool.texture(d.get())!=retired&&pool.allocations()==3,
                  "live textures were aliased");
            pool.prepare(device,wanted({e}),completed);
            check(pool.texture(e.get())!=nil&&pool.allocations()==4&&pool.reuses()==1,
                  "incompatible texture size was reused");
            std::cout<<"METAL_IMAGE_POOL_OK\n";return 0;
        } catch(const std::exception &error) { std::cerr<<error.what()<<'\n';return 1; }
    }
}
