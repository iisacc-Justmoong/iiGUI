#import <AppKit/AppKit.h>
#include "../src/Graphics/NativeCanvas.h"
#include "../src/Graphics/Image.h"
#include <iostream>
#include <stdexcept>

namespace {
using namespace ii::gui;
using ii::gui::detail::NativeCanvas;
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(ii::core::Result<T> result){if(!result)throw std::runtime_error(result.error().message);return std::move(*result);}
void same(const PixelBuffer& left,const PixelBuffer& right){
    check(left.width==right.width&&left.height==right.height,"Metal damage changed dimensions");
    if(left.rgba!=right.rgba){
        for(std::size_t i=0;i<left.rgba.size();++i)if(left.rgba[i]!=right.rgba[i]){
            std::cerr<<"First different byte "<<i<<": "<<unsigned(left.rgba[i])<<" vs "<<unsigned(right.rgba[i])<<'\n';break;
        }
        throw std::runtime_error("Partial Metal composition differed from a fresh full frame");
    }
}
}
int main(){
    @autoreleasepool {
        try{
            [NSApplication sharedApplication];
            NSView* first=[[NSView alloc] initWithFrame:NSMakeRect(0,0,200,100)];
            NSView* second=[[NSView alloc] initWithFrame:NSMakeRect(0,0,200,100)];
            NativeCanvas retained((__bridge void*)first,nullptr,Acceleration::prefer_hardware,Renderer::native,{});
            NativeCanvas reference((__bridge void*)second,nullptr,Acceleration::prefer_hardware,Renderer::native,{});
            CanvasFrame original{.viewport={0,0,200,100},.background={.1f,.1f,.1f,1}};
            original.panels={{{10,10,45,45},{1,0,0,1},{1,0,0,1}},
                             {{130,25,30,30},{0,0,1,.7f},{0,0,1,.7f}}};
            auto bitmap=take(rgba_image(2,2,{255,255,0,255,0,255,0,128,255,0,255,255,0,0,0,0}));
            CanvasLayer overlay;
            overlay.images.push_back({.bounds={115,15,30,30},.image=bitmap});
            overlay.texts.push_back({.bounds={112,50,70,25},.text="Paint",.color={1,1,1,1},.font_size=14});
            original.overlays.push_back(overlay);
            retained.draw(original,2,100);
            auto updated=original;
            updated.panels[1].bounds={145,30,30,30};
            updated.overlays[0].images[0].opacity=.5f;
            retained.draw(updated,2,100);
            reference.draw(updated,2,100);
            same(retained.capture(),reference.capture());
            const auto partial=retained.info();
            check(partial.partial_scene_renders==1&&partial.partial_scene_pixels>0&&
                  partial.partial_scene_pixels<std::uint64_t(partial.pixel_width)*partial.pixel_height,
                  "Metal did not restrict recomposition to the changed region");
            auto verify=[&](const CanvasFrame& candidate){
                auto force=candidate;force.background={.31f,.31f,.31f,1};
                reference.draw(force,2,100);reference.draw(candidate,2,100);
                retained.draw(candidate,2,100);same(retained.capture(),reference.capture());
            };
            auto edge=updated;
            edge.panels.push_back({{0,0,12,12},{.7f,.2f,.8f,.5f},{.7f,.2f,.8f,.5f}});
            verify(edge);
            auto clipped=edge;
            clipped.overlays[0].clip=ii::gui::Rect{120,20,45,48};
            clipped.overlays[0].content_transform={.origin_x=130,.origin_y=25,.scale=.8f,.offset_x=4};
            verify(clipped);
            auto removed=clipped;
            removed.overlays[0].images.clear();removed.panels.pop_back();
            verify(removed);
            auto inserted=removed;
            inserted.overlays.insert(inserted.overlays.begin(),CanvasLayer{.panels={{{70,35,20,15},{0,1,1,1},{0,1,1,1}}}});
            verify(inserted);
            auto after_insert=inserted;
            after_insert.overlays[1].texts[0].text="Changed";
            verify(after_insert);
            check(retained.info().partial_scene_renders>=6,"Local updates did not use partial Metal composition");
            auto scattered=after_insert;
            scattered.panels[0].top={.3f,.8f,.1f,1};
            scattered.panels[1].top={.9f,.2f,.4f,.7f};
            const auto before_scattered=retained.info().partial_scene_renders;
            verify(scattered);
            check(retained.info().partial_scene_renders==before_scattered+2,
                  "Distant changes did not retain separate Metal damage regions");
            const auto before_background=retained.info().partial_scene_renders;
            auto resized=scattered;resized.background={.2f,.2f,.2f,1};
            retained.draw(resized,2,100);reference.draw(resized,2,100);
            same(retained.capture(),reference.capture());
            check(retained.info().partial_scene_renders==before_background,
                  "Changed background was incorrectly treated as local damage");
            std::cout<<"METAL_DAMAGE_OK\n";
        }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
    }
}
