#pragma once
#include <iiGUI/AndroidSurface.h>
#include <iiSDKCore/Core/Text/Text.h>
#include <stdexcept>
namespace ii::gui::detail {
// Attach only this native thread. JNI local references and exceptions never
// escape the boundary, and standard UTF-8 is converted through Core UTF-16.
class AndroidJni final {
    JavaVM* vm_;bool attached_=false;
public:
    JNIEnv* env=nullptr;jobject activity=nullptr;
    AndroidJni(){auto* app=android_application();if(!app)throw std::logic_error("Android OS access requires its native application thread");vm_=app->activity->vm;activity=app->activity->clazz;
        if(vm_->GetEnv(reinterpret_cast<void**>(&env),JNI_VERSION_1_6)!=JNI_OK){if(vm_->AttachCurrentThread(&env,nullptr)!=JNI_OK)throw std::runtime_error("Cannot attach native OS bridge");attached_=true;}
        if(env->PushLocalFrame(64)<0){if(env->ExceptionCheck())env->ExceptionClear();if(attached_)vm_->DetachCurrentThread();throw std::runtime_error("Cannot allocate JNI reference frame");}}
    ~AndroidJni(){if(env->ExceptionCheck())env->ExceptionClear();env->PopLocalFrame(nullptr);if(attached_)vm_->DetachCurrentThread();}
    void check(){if(env->ExceptionCheck()){env->ExceptionClear();throw std::runtime_error("Android OS denied the request or the service is unavailable");}}
    jclass type(const char* name){auto result=env->FindClass(name);check();if(!result)throw std::runtime_error("Android class unavailable");return result;}
    jmethodID method(jclass type,const char* name,const char* signature){auto value=env->GetMethodID(type,name,signature);check();if(!value)throw std::runtime_error("Android method unavailable");return value;}
    jmethodID static_method(jclass type,const char* name,const char* signature){auto value=env->GetStaticMethodID(type,name,signature);check();if(!value)throw std::runtime_error("Android static method unavailable");return value;}
    jstring string(std::string_view value){auto text=core::to_utf16(value);if(!text)throw std::invalid_argument("OS text must be UTF-8");auto result=env->NewString(reinterpret_cast<const jchar*>(text->data()),jsize(text->size()));check();return result;}
    std::string string(jstring value){if(!value)return {};auto length=env->GetStringLength(value);if(length<0||length>1024*1024)throw std::length_error("Android text exceeds clipboard budget");auto* chars=env->GetStringChars(value,nullptr);check();if(!chars)throw std::runtime_error("Cannot read Android text");auto text=core::to_utf8(std::u16string_view(reinterpret_cast<const char16_t*>(chars),length));env->ReleaseStringChars(value,chars);if(!text)throw std::invalid_argument("Android text is not Unicode scalar text");return std::move(*text);}
    jobject clipboard(){auto context=type("android/content/Context");auto service=env->CallObjectMethod(activity,method(context,"getSystemService","(Ljava/lang/String;)Ljava/lang/Object;"),string("clipboard"));check();if(!service)throw std::runtime_error("Clipboard service unavailable");return service;}
};
}
