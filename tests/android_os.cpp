#include <iiGUI/Desktop.h>
#include <iiGUI/NativeWidgets.h>
#include <iiSDKCore/Core/Text/Text.h>
#include <android/log.h>
#include <android_native_app_glue.h>
#include <stdexcept>
#include <string_view>
namespace {
void require(bool value,const char* why){if(!value)throw std::runtime_error(why);}
thread_local android_app* fixture_app=nullptr;
// The installed example has only public SDK headers. Its optional device
// fixture borrows NativeActivity directly instead of private iiGUI symbols.
class FixtureJni final {
    JavaVM* vm_=nullptr;bool attached_=false;
public:
    JNIEnv* env=nullptr;jobject activity=nullptr;
    explicit FixtureJni(android_app* app){
        if(!app||!app->activity||!app->activity->vm||!app->activity->clazz)throw std::runtime_error("Android fixture activity is unavailable");
        vm_=app->activity->vm;activity=app->activity->clazz;
        const auto status=vm_->GetEnv(reinterpret_cast<void**>(&env),JNI_VERSION_1_6);
        if(status==JNI_EDETACHED){
            if(vm_->AttachCurrentThread(&env,nullptr)!=JNI_OK)throw std::runtime_error("Cannot attach Android fixture thread");
            attached_=true;
        }else if(status!=JNI_OK)throw std::runtime_error("Android fixture JNI version is unavailable");
        if(env->PushLocalFrame(32)<0){
            if(env->ExceptionCheck())env->ExceptionClear();if(attached_)vm_->DetachCurrentThread();
            throw std::runtime_error("Cannot allocate Android fixture references");
        }
    }
    ~FixtureJni(){if(env->ExceptionCheck())env->ExceptionClear();env->PopLocalFrame(nullptr);if(attached_)vm_->DetachCurrentThread();}
    void check(){if(env->ExceptionCheck()){env->ExceptionClear();throw std::runtime_error("Android fixture service failed");}}
    jclass type(const char* name){auto value=env->FindClass(name);check();if(!value)throw std::runtime_error("Android fixture class unavailable");return value;}
    jmethodID method(jclass owner,const char* name,const char* signature){auto value=env->GetMethodID(owner,name,signature);check();if(!value)throw std::runtime_error("Android fixture method unavailable");return value;}
    jstring string(std::string_view input){auto text=ii::core::to_utf16(input);if(!text)throw std::invalid_argument("Android fixture requires UTF-8");
        auto result=env->NewString(reinterpret_cast<const jchar*>(text->data()),jsize(text->size()));check();if(!result)throw std::runtime_error("Android fixture string unavailable");return result;}
    jobject clipboard(){auto context=type("android/content/Context");auto value=env->CallObjectMethod(activity,
        method(context,"getSystemService","(Ljava/lang/String;)Ljava/lang/Object;"),string("clipboard"));check();
        if(!value)throw std::runtime_error("Android clipboard service unavailable");return value;}
};
}
void bind_android_os_fixture(android_app* app)noexcept{fixture_app=app;}
// Opt-in physical-device fixture. Preserve the exact original ClipData object;
// no clipboard content is printed and rich/file clips are never dereferenced.
void verify_android_os(ii::gui::Host& host){
    using namespace ii;FixtureJni j(fixture_app);
    const auto focused=j.env->CallBooleanMethod(j.activity,j.method(j.type("android/app/Activity"),"hasWindowFocus","()Z"));j.check();require(focused,"Clipboard fixture requires the foreground activity");
    auto keyguard=j.env->CallObjectMethod(j.activity,j.method(j.type("android/content/Context"),"getSystemService","(Ljava/lang/String;)Ljava/lang/Object;"),j.string("keyguard"));j.check();require(keyguard,"Keyguard service unavailable");
    const auto locked=j.env->CallBooleanMethod(keyguard,j.method(j.type("android/app/KeyguardManager"),"isDeviceLocked","()Z"));j.check();require(!locked,"Clipboard fixture requires an unlocked device");
    auto manager=j.clipboard();auto type=j.type("android/content/ClipboardManager");auto original=j.env->CallObjectMethod(manager,j.method(type,"getPrimaryClip","()Landroid/content/ClipData;"));j.check();
    if(!original){
        auto version=j.type("android/os/Build$VERSION");auto sdk=j.env->GetStaticFieldID(version,"SDK_INT","I");j.check();
        require(sdk,"Android API level unavailable");const auto api=j.env->GetStaticIntField(version,sdk);j.check();
        require(api>=28,"An empty Android clipboard cannot be restored on this API level");
    }
    struct Restore final{FixtureJni& j;jobject manager,original;jclass type;bool done=false;
        void restore(){if(done)return;done=true;if(original)j.env->CallVoidMethod(manager,j.method(type,"setPrimaryClip","(Landroid/content/ClipData;)V"),original);else j.env->CallVoidMethod(manager,j.method(type,"clearPrimaryClip","()V"));j.check();}
        ~Restore(){try{restore();}catch(...){__android_log_write(ANDROID_LOG_ERROR,"iiSDK-LVUI","ANDROID_OS_CLIPBOARD_RESTORE_FAILED");}}
    }restore{j,manager,original,type};
    gui::ClipboardContent content;content.text="iiGUI UTF-8 한글 😀";
    auto written=gui::write_clipboard(content);require(bool(written),"Android clipboard write failed");auto read=gui::read_clipboard();require(read&&read->text==content.text,"Android clipboard UTF-16 round trip failed");
    require(!gui::open_external_url("file:///etc/passwd"),"Android external URL policy accepted a file URL");
    auto stats=host.stats();require(stats&&stats->display_synced&&stats->display_callbacks>1&&stats->display_interval>runtime::Clock::duration::zero(),"Android Choreographer did not supply display frames");
    const auto before=host.capture_canvas();require(bool(before),"Cannot capture existing Android widgets");
    bool candidate_built=false;
    auto rejected=host.mount_widgets([&](gui::WidgetBuilder& builder)->core::Result<void>{
        auto label=builder.label({},"candidate",{.text="한글 · مرحبا · 😀"});
        if(!label)return std::unexpected(label.error());candidate_built=true;
        return core::fail(core::ErrorCode::failed,"Intentional Android remount rollback probe");
    });
    require(candidate_built&&!rejected&&rejected.error().message=="Intentional Android remount rollback probe",
            "Android remount did not reject the deliberately failed multilingual candidate");
    const auto after=host.capture_canvas();require(after&&after->rgba==before->rgba,"Failed Android remount changed the previous surface");
    restore.restore();__android_log_print(ANDROID_LOG_INFO,"iiSDK-LVUI","ANDROID_OS_OK clipboard_utf8=true clipboard_restored=true external_policy=true remount_rollback=true frames=%llu display_synced=true",static_cast<unsigned long long>(stats->display_callbacks));
}
