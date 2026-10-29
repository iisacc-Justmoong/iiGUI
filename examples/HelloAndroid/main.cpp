#include "HelloWorld.generated.h"
#include <iiGUI/Application.h>
#include <android/log.h>
#include <chrono>
#include <stdexcept>
#if defined(IIGUI_ANDROID_OS_SMOKE)
void verify_android_os(ii::gui::Host&);
#endif
#if defined(IIGUI_ANDROID_RENDER_SMOKE)
void verify_android_retained_labels(ii::gui::Host&,const ii::gui::WidgetMount&);
#endif

namespace {
using View=lvui_generated::HelloWorld;
template<class T>T take(ii::core::Result<T> result){if(!result){if(result.error().cause)std::rethrow_exception(result.error().cause);throw std::runtime_error(result.error().message);}return std::move(*result);}
struct State final {
    std::shared_ptr<View> view=std::make_shared<View>();
    std::weak_ptr<ii::gui::Host> host;
    ii::gui::Widget status;
    int ticks=0;
};
}
ii::core::Result<void> ii::runtime::configure_application(Application& application){
    auto state=std::make_shared<State>();
    auto options=View::options();
#if defined(IIGUI_ANDROID_OS_SMOKE)
    options.frame_loop=&application.frame_loop();
#endif
    auto host=gui::attach_application(application,options,{.opened=[state,&application]{
        auto mount=state->view->widgets();state->status=take(mount.find("status"));
        auto greeting=take(take(mount.find("greeting")).snapshot());
        auto host=state->host.lock();if(!host)throw std::runtime_error("Native host expired");
        const auto gpu=take(host->graphics_info());
        if(greeting.native_class!="iiGUI.Label"||greeting.renderer!="iiGUI"||gpu.api!=gui::GraphicsApi::vulkan)
            throw std::runtime_error("LVUI was not mounted as self-rendered native C++ widgets");
#if defined(IIGUI_ANDROID_RENDER_SMOKE)
        verify_android_retained_labels(*host,mount);
#endif
        __android_log_print(ANDROID_LOG_INFO,"iiSDK-LVUI","HELLO_ANDROID_READY widget=%s renderer=Vulkan adapter=%s runtime=iiSDKCore qt=false aot=true",greeting.native_class.c_str(),gpu.adapter.c_str());
        auto scheduled=application.event_loop().sender().every(std::chrono::seconds(1),[state]{
            auto result=state->status.set_text("Native C++23 - "+std::to_string(++state->ticks)+" s");
            if(!result)throw std::runtime_error(result.error().message);
#if defined(IIGUI_ANDROID_OS_SMOKE)
            if(state->ticks==1){auto host=state->host.lock();if(!host)throw std::runtime_error("Android host expired");verify_android_os(*host);}
#endif
            if(state->ticks==2)__android_log_write(ANDROID_LOG_INFO,"iiSDK-LVUI","HELLO_ANDROID_TIMER_OK ticks=2");
        });
        if(!scheduled)throw std::runtime_error("Core timer was rejected");
    }});
    if(!host)return std::unexpected(host.error());state->host=*host;
    return application.attach(gui::view_attachment("hello-world",*host,state->view));
}
