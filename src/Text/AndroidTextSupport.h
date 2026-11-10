#pragma once
#include <iiGUI/AndroidJni.h>
#include <iiGUI/AndroidLabelFont.h>
#include <iiGUI/FontNative.h>
#include <iiGUI/NativeText.h>
#include <android/bitmap.h>
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>

namespace ii::gui::detail::android_text {
struct LocalFrame final {
    AndroidJni& jni;
    explicit LocalFrame(AndroidJni& value) : jni(value) {
        if (jni.env->PushLocalFrame(32) < 0) {
            jni.check();
            throw std::runtime_error("Cannot allocate Android text references");
        }
    }
    ~LocalFrame() { jni.env->PopLocalFrame(nullptr); }
};

inline jobject object(AndroidJni& jni, jobject value, const char* message) {
    jni.check();
    if (!value) throw std::runtime_error(message);
    return value;
}
inline jobject static_field(AndroidJni& jni, jclass type, const char* name, const char* signature) {
    auto field = jni.env->GetStaticFieldID(type, name, signature);
    jni.check();
    if (!field) throw std::runtime_error("Android text field is unavailable");
    return object(jni, jni.env->GetStaticObjectField(type, field), "Android text field is null");
}
inline jint android_version(AndroidJni& jni) {
    auto version = jni.type("android/os/Build$VERSION");
    auto field = jni.env->GetStaticFieldID(version, "SDK_INT", "I");
    jni.check();
    if (!field) throw std::runtime_error("Android API level is unavailable");
    const auto result = jni.env->GetStaticIntField(version, field);
    jni.check();
    return result;
}

inline std::shared_ptr<AndroidLabelFont> font_for(std::string_view family, unsigned weight) {
    auto database = FontDatabase::application();
    if (auto loaded = database->load_bundled_fonts(); !loaded) throw std::runtime_error(loaded.error().message);
    return native_android_font(database->resolve_family(family), int(weight));
}
inline std::shared_ptr<AndroidLabelFont> font_for(const TextRun& run) {
    return font_for(run.font_family, run.font_weight);
}

inline jobject typeface(AndroidJni& jni, const AndroidLabelFont& font, jint api) {
    const auto& source = font.source();
    if (source.kind == AndroidFontSource::Kind::none || source.path.empty())
        throw std::runtime_error("Android text font has no native source");
    auto type = jni.type("android/graphics/Typeface");
    auto path = static_cast<jstring>(object(jni, jni.string(source.path), "Cannot allocate Android font path"));
    jobject assets = nullptr;
    if (source.kind == AndroidFontSource::Kind::asset) {
        auto context = jni.type("android/content/Context");
        assets = object(jni, jni.env->CallObjectMethod(jni.activity,
            jni.method(context, "getAssets", "()Landroid/content/res/AssetManager;")),
            "Android font assets are unavailable");
    }
    if (!font.collection_index()) {
        if (assets) return object(jni, jni.env->CallStaticObjectMethod(type,
            jni.static_method(type, "createFromAsset", "(Landroid/content/res/AssetManager;Ljava/lang/String;)Landroid/graphics/Typeface;"),
            assets, path), "Cannot load Android font asset");
        return object(jni, jni.env->CallStaticObjectMethod(type,
            jni.static_method(type, "createFromFile", "(Ljava/lang/String;)Landroid/graphics/Typeface;"), path),
            "Cannot load Android font file");
    }
    if (api < 26) throw std::runtime_error("Android font collection indices require API 26");
    auto builder_type = jni.type("android/graphics/Typeface$Builder");
    const auto constructor = assets
        ? jni.method(builder_type, "<init>", "(Landroid/content/res/AssetManager;Ljava/lang/String;)V")
        : jni.method(builder_type, "<init>", "(Ljava/lang/String;)V");
    auto builder = object(jni, assets ? jni.env->NewObject(builder_type, constructor, assets, path)
                                       : jni.env->NewObject(builder_type, constructor, path),
                          "Cannot create Android font builder");
    auto changed = object(jni, jni.env->CallObjectMethod(builder,
        jni.method(builder_type, "setTtcIndex", "(I)Landroid/graphics/Typeface$Builder;"),
        static_cast<jint>(font.collection_index())), "Cannot select Android font collection face");
    jni.env->DeleteLocalRef(changed);
    return object(jni, jni.env->CallObjectMethod(builder,
        jni.method(builder_type, "build", "()Landroid/graphics/Typeface;")), "Cannot build Android font face");
}

inline jint argb(Color color) {
    const auto channel = [](float value) { return std::uint32_t(std::clamp(std::lround(value * 255), 0l, 255l)); };
    const auto packed = (channel(color.alpha) << 24) | (channel(encoded_srgb(color.red)) << 16) |
                        (channel(encoded_srgb(color.green)) << 8) | channel(encoded_srgb(color.blue));
    return std::bit_cast<jint>(packed);
}

inline PixelBuffer bitmap_pixels(AndroidJni& jni,jobject bitmap,std::uint32_t width,std::uint32_t height){
    AndroidBitmapInfo info{};
    if(AndroidBitmap_getInfo(jni.env,bitmap,&info)!=ANDROID_BITMAP_RESULT_SUCCESS){
        jni.check();throw std::runtime_error("Cannot inspect Android text bitmap");
    }
    if(info.format!=ANDROID_BITMAP_FORMAT_RGBA_8888||info.width!=width||info.height!=height||
       info.stride<std::uint64_t(width)*4)throw std::runtime_error("Invalid Android text bitmap format");
    void* data=nullptr;
    if(AndroidBitmap_lockPixels(jni.env,bitmap,&data)!=ANDROID_BITMAP_RESULT_SUCCESS){
        jni.check();throw std::runtime_error("Cannot read Android text bitmap");
    }
    struct Unlock final{JNIEnv* env;jobject bitmap;~Unlock(){AndroidBitmap_unlockPixels(env,bitmap);}} unlock{jni.env,bitmap};
    if(!data)throw std::runtime_error("Android text bitmap has no pixel storage");
    PixelBuffer result{width,height,std::vector<std::uint8_t>(std::size_t(width)*height*4)};
    for(std::size_t row=0;row<height;++row)
        std::memcpy(result.rgba.data()+row*width*4,static_cast<const std::uint8_t*>(data)+row*info.stride,std::size_t(width)*4);
    straight_rgba(result,false);return result;
}

} // namespace ii::gui::detail::android_text
