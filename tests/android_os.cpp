#include "iiGUI/AndroidJni.h"
#include <iiGUI/Desktop.h>
#include <iiGUI/NativeWidgets.h>
#include <android/log.h>
namespace {
void require(bool value,const char* why){if(!value)throw std::runtime_error(why);}
}
// Opt-in physical-device fixture. Preserve the exact original ClipData object;
// no clipboard content is printed and rich/file clips are never dereferenced.
void verify_android_os(ii::gui::Host& host){
    using namespace ii;gui::detail::AndroidJni j;
    const auto focused=j.env->CallBooleanMethod(j.activity,j.method(j.type("android/app/Activity"),"hasWindowFocus","()Z"));j.check();require(focused,"Clipboard fixture requires the foreground activity");
    auto keyguard=j.env->CallObjectMethod(j.activity,j.method(j.type("android/content/Context"),"getSystemService","(Ljava/lang/String;)Ljava/lang/Object;"),j.string("keyguard"));j.check();require(keyguard,"Keyguard service unavailable");
    const auto locked=j.env->CallBooleanMethod(keyguard,j.method(j.type("android/app/KeyguardManager"),"isDeviceLocked","()Z"));j.check();require(!locked,"Clipboard fixture requires an unlocked device");
    auto manager=j.clipboard();auto type=j.type("android/content/ClipboardManager");auto original=j.env->CallObjectMethod(manager,j.method(type,"getPrimaryClip","()Landroid/content/ClipData;"));j.check();
    struct Restore final{gui::detail::AndroidJni& j;jobject manager,original;jclass type;bool done=false;
        void restore(){if(done)return;done=true;if(original)j.env->CallVoidMethod(manager,j.method(type,"setPrimaryClip","(Landroid/content/ClipData;)V"),original);else j.env->CallVoidMethod(manager,j.method(type,"clearPrimaryClip","()V"));j.check();}
        ~Restore(){try{restore();}catch(...){__android_log_write(ANDROID_LOG_ERROR,"iiSDK-LVUI","ANDROID_OS_CLIPBOARD_RESTORE_FAILED");}}
    }restore{j,manager,original,type};
    gui::ClipboardContent content;content.text="iiGUI UTF-8 한글 😀";
    auto written=gui::write_clipboard(content);require(bool(written),"Android clipboard write failed");auto read=gui::read_clipboard();require(read&&read->text==content.text,"Android clipboard UTF-16 round trip failed");
    require(!gui::open_external_url("file:///etc/passwd"),"Android external URL policy accepted a file URL");
    auto stats=host.stats();require(stats&&stats->display_synced&&stats->display_callbacks>1&&stats->display_interval>runtime::Clock::duration::zero(),"Android Choreographer did not supply display frames");
    const auto before=host.capture_canvas();require(bool(before),"Cannot capture existing Android widgets");
    auto rejected=host.mount_widgets([](gui::WidgetBuilder& builder)->core::Result<void>{auto label=builder.label({},"unsupported",{.text="한글"});if(!label)return std::unexpected(label.error());return {};});
    require(!rejected,"Unsupported Android label unexpectedly mounted");
    const auto after=host.capture_canvas();require(after&&after->rgba==before->rgba,"Failed Android remount changed the previous surface");
    restore.restore();__android_log_print(ANDROID_LOG_INFO,"iiSDK-LVUI","ANDROID_OS_OK clipboard_utf8=true clipboard_restored=true external_policy=true remount_rollback=true frames=%llu display_synced=true",static_cast<unsigned long long>(stats->display_callbacks));
}
