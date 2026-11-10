#pragma once
#include <iiGUI/AndroidJni.h>
#include <bit>
#include <cstdint>
#include <memory>
#include <optional>

namespace ii::gui::detail {
// One instance belongs to one native event. Its JNI local frame outlives both
// character lookups and the optional dead-key combination for that event.
class AndroidKeyboardMap final {
    std::unique_ptr<AndroidJni> jni_;
    jclass type_=nullptr;
    jobject map_=nullptr;
    std::optional<std::int32_t> device_;
    void prepare(){
        if(!jni_)jni_=std::make_unique<AndroidJni>();
        if(!type_)type_=jni_->type("android/view/KeyCharacterMap");
    }
public:
    std::uint32_t character(std::int32_t device,std::int32_t key,std::int32_t meta) noexcept {
        try{
            prepare();
            if(!device_||*device_!=device){
                auto candidate=jni_->env->CallStaticObjectMethod(type_,
                    jni_->static_method(type_, "load", "(I)Landroid/view/KeyCharacterMap;"),jint(device));
                jni_->check();if(!candidate)return 0;
                map_=candidate;device_=device;
            }
            const auto result=jni_->env->CallIntMethod(map_,jni_->method(type_, "get", "(II)I"),jint(key),jint(meta));
            jni_->check();return std::bit_cast<std::uint32_t>(result);
        }catch(...){return 0;}
    }
    std::uint32_t combine(std::uint32_t accent,std::uint32_t character) noexcept {
        try{
            prepare();
            const auto result=jni_->env->CallStaticIntMethod(type_,
                jni_->static_method(type_, "getDeadChar", "(II)I"),jint(accent),jint(character));
            jni_->check();return std::bit_cast<std::uint32_t>(result);
        }catch(...){return 0;}
    }
};
} // namespace ii::gui::detail
