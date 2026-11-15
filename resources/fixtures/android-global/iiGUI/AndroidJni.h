#pragma once
#include <stdexcept>
using jint=int;
using jobject=void*;
inline constexpr jint JNI_OK=0,JNI_EDETACHED=-2,JNI_VERSION_1_6=0x00010006;
struct JavaVM;
struct JNIEnv {
    jint GetJavaVM(JavaVM**);
    jobject NewGlobalRef(jobject);
    void DeleteGlobalRef(jobject);
};
struct JavaVM {
    jint GetEnv(void**,jint);
    jint AttachCurrentThread(JNIEnv**,void*);
    jint DetachCurrentThread();
};
namespace ii::gui::detail {
struct AndroidJni {JNIEnv* env;void check();};
}
