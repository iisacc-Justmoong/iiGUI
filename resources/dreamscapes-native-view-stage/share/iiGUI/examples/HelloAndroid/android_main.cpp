#include <iiGUI/Android.h>
#include <android/log.h>
#include <android_native_app_glue.h>

#if defined(IIGUI_ANDROID_OS_SMOKE)
void bind_android_os_fixture(android_app*) noexcept;
#endif
extern "C" void android_main(android_app* native){
#if defined(IIGUI_ANDROID_OS_SMOKE)
    bind_android_os_fixture(native);
    struct ResetFixture final {~ResetFixture(){bind_android_os_fixture(nullptr);}} reset;
#endif
    auto result=ii::gui::run_android_application(native,ii::runtime::configure_application);
    if(!result){
        auto message=result.error().message;
        if(result.error().cause)try{std::rethrow_exception(result.error().cause);}catch(const std::exception& e){message+=": ";message+=e.what();}catch(...){}
        __android_log_write(ANDROID_LOG_ERROR,"iiSDK-LVUI",message.c_str());
    }else __android_log_print(ANDROID_LOG_INFO,"iiSDK-LVUI","HELLO_ANDROID_EXIT code=%d",*result);
}
