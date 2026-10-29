#include <iiGUI/iiGUI.h>
#include <LVUI/NativeDocument.h>
#include "ViewSource.h"
#include <chrono>
#include <cmath>
#include <iostream>
#include <stdexcept>

using namespace std::chrono_literals;
template<class T> void require(const T& result,const char* message) {
    if (!result) throw std::runtime_error(message);
}
void require(const ii::core::Result<void>& result,const char*) {
    if (result) return;
    if (result.error().cause) std::rethrow_exception(result.error().cause);
    throw std::runtime_error(result.error().message);
}
int main(int argc,char** argv) {
    try {
        bool self_test=false,capture=false;
        auto renderer=ii::gui::Renderer::automatic;
        for (int i=1;i<argc;++i) {
            const std::string_view arg=argv[i];
            if (arg=="--self-test") self_test=true;
            else if (arg=="--capture") capture=true;
            else if (arg=="--vulkan") renderer=ii::gui::Renderer::vulkan;
            else if (arg=="--native") renderer=ii::gui::Renderer::native;
            else throw std::runtime_error("Usage: HelloLVUI [--self-test|--capture] [--vulkan|--native]");
        }
        if (self_test && capture) throw std::runtime_error("Choose either --self-test or --capture");
        auto document=lvui::parse(view_source);
        if (!document) throw std::runtime_error("LVUI "+std::to_string(document.error().line)+":"+
            std::to_string(document.error().column)+" "+document.error().message);
        ii::gui::Host host({.title="LVUI · iiGUI",.size={800,540},.minimum_size={640,500}});
        auto ui=host.executor(); bool opened=false,resized=false,glyphs=false,closed=false;
        const auto paint=[&] { require(host.set_view(*document,renderer),"LVUI layout or device admission failed"); };
        auto result=host.run({.opened=[&] {
            opened=true; require(host.set_text(""),"Cannot clear host label"); paint();
            if (self_test || capture) {
                require(ui.after(5s,[] { throw std::runtime_error("LVUI example watchdog"); }),"Cannot arm watchdog");
                if (self_test) require(host.set_content_size({720,540}),"Cannot resize LVUI example");
                require(ui.after(80ms,[&] {
                    auto pixels=host.capture_canvas(); auto info=host.graphics_info();
                    require(pixels && info && info->scene_renders>0,"LVUI GPU capture failed");
                    if (renderer==ii::gui::Renderer::vulkan)
                        require(info->api==ii::gui::GraphicsApi::vulkan && info->presentations>0,"Vulkan was requested but did not present");
                    std::size_t bright=0;
                    for (std::size_t i=0;i<pixels->rgba.size();i+=4)
                        if (pixels->rgba[i]>210 && pixels->rgba[i+1]>210 && pixels->rgba[i+2]>210) ++bright;
                    glyphs=bright>100;
                    require(glyphs,"LVUI text did not reach GPU pixels");
                    if (capture) {
                        // ASCII PPM also works with Windows' text-mode stdout.
                        std::cout<<"P3\n"<<pixels->width<<' '<<pixels->height<<"\n255\n";
                        for (std::size_t i=0;i<pixels->rgba.size();i+=4)
                            std::cout<<unsigned(pixels->rgba[i])<<' '<<unsigned(pixels->rgba[i+1])<<' '
                                <<unsigned(pixels->rgba[i+2])<<'\n';
                    } else std::cout<<"LVUI_GPU api="<<int(info->api)<<" fallback="<<info->fallback<<" adapter="<<info->adapter<<" hardware="<<info->hardware
                        <<" glyph_pixels="<<bright<<'\n';
                    require(host.request_close(),"Cannot close LVUI example");
                }),"Cannot schedule LVUI capture");
            }
        },.resized=[&](ii::gui::Size size) {
            if (opened) paint();
            resized=resized || std::abs(size.width-720)<1;
        },.closed=[&] { closed=true; }});
        require(result,"Native host failed");
        if (self_test) {
            require(opened && resized && glyphs && closed,"LVUI self-test milestones missing");
            std::cout<<"HELLO_LVUI_OK resize=true glyphs=true closed=true\n";
        }
    } catch (const std::exception& error) { std::cerr<<error.what()<<'\n'; return 1; }
}
