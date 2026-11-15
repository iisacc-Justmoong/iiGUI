#pragma once
#include <iiSDKCore/Core/Text/Text.h>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

using jint = std::int32_t;
using jfloat = float;
using jboolean = unsigned char;
inline constexpr jboolean JNI_TRUE = 1, JNI_FALSE = 0;
struct FakeObject { std::string type; std::u16string text; };
using jobject = FakeObject*;
using jclass = FakeObject*;
using jstring = FakeObject*;
using jmethodID = const char*;
using jfieldID = const char*;

struct FakeState {
    int api = 28, width = 0, height = 0, stride = 0, max_lines = 0, collection_index = 0;
    int selected_collection_index = -1;
    int pushes = 0, pops = 0, unlocks = 0, draws = 0;
    bool fallback = false, underline = false, ellipsis = false, null_pixels = false;
    bool pending_exception = false, fail_draw = false;
    std::u16string shaped_text, font_path;
    std::string font_kind;
    std::vector<float> translations, clips;
    std::vector<std::uint8_t> pixels;
    std::vector<std::unique_ptr<FakeObject>> objects;
    jobject make(std::string type = {}, std::u16string text = {}) {
        objects.push_back(std::make_unique<FakeObject>(FakeObject{std::move(type), std::move(text)}));
        return objects.back().get();
    }
};
FakeState& fake_state();

struct JNIEnv {
    jint PushLocalFrame(jint);
    jobject PopLocalFrame(jobject);
    void DeleteLocalRef(jobject);
    jfieldID GetStaticFieldID(jclass, const char*, const char*);
    jint GetStaticIntField(jclass, jfieldID);
    jobject GetStaticObjectField(jclass, jfieldID);
    jobject CallObjectMethod(jobject, jmethodID, ...);
    jobject CallStaticObjectMethod(jclass, jmethodID, ...);
    jobject NewObject(jclass, jmethodID, ...);
    void CallVoidMethod(jobject, jmethodID, ...);
    jint CallIntMethod(jobject, jmethodID, ...);
    jfloat CallFloatMethod(jobject, jmethodID, ...);
    jboolean CallBooleanMethod(jobject, jmethodID, ...);
    jint GetStringLength(jstring);
};
namespace ii::gui::detail {
class AndroidJni final {
    JNIEnv storage_;
public:
    JNIEnv* env = &storage_;
    jobject activity = nullptr;
    AndroidJni() : activity(fake_state().make("activity")) {}
    void check() {
        if (fake_state().pending_exception) {
            fake_state().pending_exception = false;
            throw std::runtime_error("Simulated Android exception");
        }
    }
    jclass type(const char* name) { return fake_state().make(name); }
    jmethodID method(jclass, const char* name, const char*) { return name; }
    jmethodID static_method(jclass, const char* name, const char*) { return name; }
    jstring string(std::string_view value) {
        auto text = ii::core::to_utf16(value);
        if (!text) throw std::invalid_argument("OS text must be UTF-8");
        return fake_state().make("string", std::move(*text));
    }
};
}
