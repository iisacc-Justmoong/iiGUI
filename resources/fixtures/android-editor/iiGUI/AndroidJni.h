#pragma once
#include <iiSDKCore/Core/Text/Text.h>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
using jint=std::int32_t;using jfloat=float;using jboolean=unsigned char;
inline constexpr jint JNI_OK=0,JNI_EDETACHED=-2,JNI_VERSION_1_6=0x00010006;
inline constexpr jboolean JNI_TRUE=1,JNI_FALSE=0;
struct FakeRect {double left=0,top=0,right=0,bottom=0;};
struct FakeRow {int first=0,last=0,top=0,bottom=0,baseline=0,ellipsis=0;float left=0,width=0;};
struct FakeObject {
    std::string type;std::u16string text;int number=0,width=0,maximum=65537,color=0,index=0,ellipsized_width=0;
    bool ellipsize=false;
    float scale=1;FakeObject* source=nullptr;FakeObject* paint=nullptr;
    std::vector<FakeRow> rows;std::vector<FakeRect> rects;
};
struct FakeMethod {std::string name,signature;};
struct FakeField {std::string name;};
using jobject=FakeObject*;using jclass=FakeObject*;using jstring=FakeObject*;
using jmethodID=FakeMethod*;using jfieldID=FakeField*;
struct JavaVM;struct JNIEnv;
struct FakeState {
    int api=28,width=0,height=0,stride=0,global_refs=0,unlocks=0,draws=0,pushes=0,pops=0;
    bool pending=false,null_pixels=false,fail_draw=false;
    std::u16string shaped_text,font_path;std::string font_kind;
    std::vector<int> tabs;std::vector<float> translations,clips;std::vector<std::uint8_t> pixels;
    std::vector<std::unique_ptr<FakeObject>> objects;
    std::vector<std::unique_ptr<FakeMethod>> methods;
    std::vector<std::unique_ptr<FakeField>> fields;
    jobject make(std::string type={},std::u16string text={}){
        auto value=std::make_unique<FakeObject>();value->type=std::move(type);value->text=std::move(text);
        objects.push_back(std::move(value));return objects.back().get();
    }
    jmethodID method(const char* name,const char* signature){methods.push_back(std::make_unique<FakeMethod>(FakeMethod{name,signature}));return methods.back().get();}
    jfieldID field(const char* name){fields.push_back(std::make_unique<FakeField>(FakeField{name}));return fields.back().get();}
};
FakeState& fake_state();JNIEnv& fake_env();JavaVM& fake_vm();
struct JNIEnv {
    jint PushLocalFrame(jint);jobject PopLocalFrame(jobject);void DeleteLocalRef(jobject);
    jint GetJavaVM(JavaVM**);jobject NewGlobalRef(jobject);void DeleteGlobalRef(jobject);
    jfieldID GetStaticFieldID(jclass,const char*,const char*);jfieldID GetFieldID(jclass,const char*,const char*);
    jint GetStaticIntField(jclass,jfieldID);jobject GetStaticObjectField(jclass,jfieldID);jint GetIntField(jobject,jfieldID);
    jobject CallObjectMethod(jobject,jmethodID,...);jobject CallStaticObjectMethod(jclass,jmethodID,...);
    jfloat CallStaticFloatMethod(jclass,jmethodID,...);jobject NewObject(jclass,jmethodID,...);
    void CallVoidMethod(jobject,jmethodID,...);jint CallIntMethod(jobject,jmethodID,...);
    jfloat CallFloatMethod(jobject,jmethodID,...);jboolean CallBooleanMethod(jobject,jmethodID,...);
};
struct JavaVM {jint GetEnv(void**,jint);jint AttachCurrentThread(JNIEnv**,void*);jint DetachCurrentThread();};
namespace ii::gui::detail {
class AndroidJni final {
public:
    JNIEnv* env=&fake_env();jobject activity=nullptr;
    AndroidJni():activity(fake_state().make("activity")){}
    void check(){if(fake_state().pending){fake_state().pending=false;throw std::runtime_error("Simulated Android exception");}}
    jclass type(const char* name){return fake_state().make(name);}
    jmethodID method(jclass,const char* name,const char* signature){return fake_state().method(name,signature);}
    jmethodID static_method(jclass,const char* name,const char* signature){return fake_state().method(name,signature);}
    jstring string(std::string_view value){auto text=ii::core::to_utf16(value);if(!text)throw std::invalid_argument("Invalid UTF-8");return fake_state().make("string",std::move(*text));}
};
}
