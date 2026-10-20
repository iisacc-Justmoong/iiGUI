#include "RendererChain.h"
#include <iostream>
#include <stdexcept>

using namespace ii::gui;
namespace {
void check(bool value,const char* message) { if (!value) throw std::runtime_error(message); }
struct Native {
    static inline int created=0,drawn=0,destroyed=0;
    static inline bool fail_init=false,fail_draw=false;
    Native(void*,void*,Acceleration) { ++created; if (fail_init) throw std::runtime_error("native unavailable"); }
    ~Native() { ++destroyed; }
    void draw(const CanvasFrame&,double,double) { ++drawn; if (fail_draw) throw std::runtime_error("device lost"); }
    PixelBuffer capture() { return {}; }
    GraphicsInfo info() const { return {.api=GraphicsApi::metal,.hardware=true}; }
    bool native_event(const void*) { return true; }
};
struct Vulkan {
    static inline int created=0,drawn=0;
    static inline bool fail_init=false,software=false;
    Vulkan(void*,void*,Acceleration acceleration) {
        ++created;
        if (fail_init || (software && acceleration==Acceleration::require_hardware)) throw std::runtime_error("Vulkan unavailable");
    }
    void draw(const CanvasFrame&,double,double) { ++drawn; }
    PixelBuffer capture() { return {}; }
    GraphicsInfo info() const { return {.api=GraphicsApi::vulkan,.hardware=!software}; }
    bool native_event(const void*) { return false; }
};
using Chain=ii::gui::detail::RendererChain<Native,Vulkan>;
void reset() { Native::created=Native::drawn=Native::destroyed=Vulkan::created=Vulkan::drawn=0;
    Native::fail_init=Native::fail_draw=Vulkan::fail_init=Vulkan::software=false; }
template<class F> std::string failure(F&& f) {
    try { f(); } catch (const std::exception& e) { return e.what(); }
    throw std::runtime_error("Expected failure");
}
}
int main() {
    try {
        CanvasFrame frame{.viewport={0,0,100,100}};
        reset(); { Chain chain(nullptr,nullptr,Acceleration::prefer_hardware,Renderer::automatic);
            chain.draw(frame,1,100); check(Native::drawn==1 && Vulkan::created==0 && !chain.info().fallback,"Native first"); }
        reset(); Native::fail_init=true;
        { Chain chain(nullptr,nullptr,Acceleration::prefer_hardware,Renderer::automatic);
            chain.draw(frame,1,100); check(Vulkan::drawn==1 && chain.info().fallback &&
                chain.info().fallback_reason.find("native unavailable")!=std::string::npos,"Initialization fallback records reason"); }
        reset(); Native::fail_draw=true;
        { Chain chain(nullptr,nullptr,Acceleration::prefer_hardware,Renderer::automatic);
            chain.draw(frame,1,100); chain.draw(frame,1,100);
            check(Native::drawn==1 && Native::destroyed==1 && Vulkan::drawn==2 && chain.info().fallback,"Draw failure switches once and preserves rendering"); }
        reset(); { Chain chain(nullptr,nullptr,Acceleration::prefer_hardware,Renderer::vulkan);
            check(Native::created==0 && Vulkan::created==1 && !chain.info().fallback,"Explicit Vulkan"); }
        reset(); Native::fail_init=true;
        (void)failure([] { Chain c(nullptr,nullptr,Acceleration::prefer_hardware,Renderer::native); });
        check(Vulkan::created==0,"Explicit native never silently changes API");
        reset(); Native::fail_init=Vulkan::fail_init=true;
        auto both=failure([] { Chain c(nullptr,nullptr,Acceleration::prefer_hardware,Renderer::automatic); });
        check(both.find("native unavailable")!=both.npos && both.find("Vulkan unavailable")!=both.npos,"Both errors retained");
        reset(); Native::fail_init=Vulkan::software=true;
        (void)failure([] { Chain c(nullptr,nullptr,Acceleration::require_hardware,Renderer::automatic); });
        reset(); { ii::gui::detail::RendererChain<Native,Vulkan,false> c(nullptr,nullptr,Acceleration::prefer_hardware,Renderer::automatic);
            check(Native::created==0 && c.info().api==GraphicsApi::vulkan && !c.info().fallback,"Vulkan platform default"); }
        std::cout<<"Renderer policy passed: native first, Vulkan fallback, explicit API, strict hardware, no repeated native retry\n";
    } catch (const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
