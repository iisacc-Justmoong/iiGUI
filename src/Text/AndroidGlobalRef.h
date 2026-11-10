#pragma once
#include <iiGUI/AndroidJni.h>
#include <stdexcept>
#include <utility>

namespace ii::gui::detail {
// JNI local frames belong to individual calls, while wrapped text fragments
// may outlive the parent. A global reference can be released on any thread.
class AndroidGlobalRef final {
    JavaVM* vm_=nullptr;
    jobject value_=nullptr;
    void reset()noexcept{
        if(!vm_||!value_)return;
        JNIEnv* env=nullptr;bool attached=false;
        const auto status=vm_->GetEnv(reinterpret_cast<void**>(&env),JNI_VERSION_1_6);
        if(status==JNI_EDETACHED){if(vm_->AttachCurrentThread(&env,nullptr)!=JNI_OK)return;attached=true;}
        else if(status!=JNI_OK)return;
        env->DeleteGlobalRef(value_);if(attached)vm_->DetachCurrentThread();value_=nullptr;vm_=nullptr;
    }
public:
    AndroidGlobalRef()=default;
    AndroidGlobalRef(AndroidJni& jni,jobject object){
        if(!object)throw std::invalid_argument("Cannot retain a null Android object");
        if(jni.env->GetJavaVM(&vm_)!=JNI_OK||!vm_)throw std::runtime_error("Android Java VM is unavailable");
        auto value=jni.env->NewGlobalRef(object);
        try{jni.check();if(!value)throw std::runtime_error("Cannot retain Android text object");}
        catch(...){if(value)jni.env->DeleteGlobalRef(value);vm_=nullptr;throw;}
        value_=value;
    }
    ~AndroidGlobalRef(){reset();}
    AndroidGlobalRef(const AndroidGlobalRef&)=delete;
    AndroidGlobalRef& operator=(const AndroidGlobalRef&)=delete;
    AndroidGlobalRef(AndroidGlobalRef&& other)noexcept:vm_(std::exchange(other.vm_,nullptr)),value_(std::exchange(other.value_,nullptr)){}
    AndroidGlobalRef& operator=(AndroidGlobalRef&& other)noexcept{
        if(this!=&other){reset();vm_=std::exchange(other.vm_,nullptr);value_=std::exchange(other.value_,nullptr);}return *this;
    }
    [[nodiscard]] jobject get()const noexcept{return value_;}
};
}
