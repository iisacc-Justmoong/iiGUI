#include <iiGUI/AndroidJni.h>
#include <iiGUI/AndroidLabelFont.h>
#include <iiGUI/FontNative.h>
#include <iiGUI/NativeText.h>
#include <android/bitmap.h>
#include <cstdarg>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string_view>

using namespace ii::gui;
using namespace ii::gui::detail;

FakeState& fake_state() { static FakeState value; return value; }
namespace {
void require(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
bool equal(float a, float b) { return std::abs(a - b) < .001f; }
void reset() { fake_state() = FakeState{}; }
}

std::shared_ptr<AndroidLabelFont> ii::gui::detail::native_android_font(std::string_view, int) {
    auto result = std::make_shared<AndroidLabelFont>();
    result->index = static_cast<unsigned>(fake_state().collection_index);
    if (fake_state().font_kind == "file") result->data.kind = AndroidFontSource::Kind::file;
    return result;
}

jint JNIEnv::PushLocalFrame(jint) { ++fake_state().pushes; return 0; }
jobject JNIEnv::PopLocalFrame(jobject) { ++fake_state().pops; return nullptr; }
void JNIEnv::DeleteLocalRef(jobject) {}
jfieldID JNIEnv::GetStaticFieldID(jclass, const char* name, const char*) { return name; }
jint JNIEnv::GetStaticIntField(jclass, jfieldID) { return fake_state().api; }
jobject JNIEnv::GetStaticObjectField(jclass, jfieldID field) { return fake_state().make(field); }
jint JNIEnv::GetStringLength(jstring text) { return static_cast<jint>(text->text.size()); }

jobject JNIEnv::NewObject(jclass type, jmethodID, ...) { return fake_state().make(type->type); }
jobject JNIEnv::CallObjectMethod(jobject target, jmethodID method, ...) {
    auto& s = fake_state();
    const std::string_view name(method);
    va_list args; va_start(args, method);
    jobject result = target;
    if (name == "getAssets") result = s.make("assets");
    else if (name == "setTypeface") result = nullptr;
    else if (name == "setUseLineSpacingFromFallbacks") s.fallback = va_arg(args, int);
    else if (name == "setEllipsize") { (void)va_arg(args, jobject); s.ellipsis = true; }
    else if (name == "setMaxLines") s.max_lines = va_arg(args, int);
    else if (name == "setTtcIndex") s.selected_collection_index = va_arg(args, int);
    else if (name == "build") result = s.make("built:" + target->type);
    else throw std::runtime_error("Unexpected Android object method");
    va_end(args);
    return result;
}
jobject JNIEnv::CallStaticObjectMethod(jclass, jmethodID method, ...) {
    auto& s = fake_state();
    const std::string_view name(method);
    va_list args; va_start(args, method);
    jobject result = nullptr;
    if (name == "createFromAsset") {
        (void)va_arg(args, jobject); auto path = va_arg(args, jstring);
        s.font_kind = "asset"; s.font_path = path->text; result = s.make("typeface");
    } else if (name == "createFromFile") {
        auto path = va_arg(args, jstring);
        s.font_kind = "file"; s.font_path = path->text; result = s.make("typeface");
    } else if (name == "obtain") {
        auto text = va_arg(args, jstring);
        const auto first = va_arg(args, int), last = va_arg(args, int);
        (void)va_arg(args, jobject); (void)va_arg(args, int);
        require(first == 0 && last == static_cast<int>(text->text.size()), "UTF-16 length was truncated");
        s.shaped_text = text->text; result = s.make("layout_builder");
    } else if (name == "createBitmap") {
        s.width = va_arg(args, int); s.height = va_arg(args, int); (void)va_arg(args, jobject);
        s.stride = s.width * 4 + 8;
        s.pixels.assign(static_cast<std::size_t>(s.stride) * s.height, 0);
        if (!s.pixels.empty()) { s.pixels[0] = 10; s.pixels[1] = 20; s.pixels[2] = 30; s.pixels[3] = 128; }
        if (s.height > 1) { s.pixels[s.stride] = 5; s.pixels[s.stride + 3] = 10; }
        result = s.make("bitmap");
    } else throw std::runtime_error("Unexpected Android static method");
    va_end(args);
    return result;
}
void JNIEnv::CallVoidMethod(jobject, jmethodID method, ...) {
    auto& s = fake_state();
    const std::string_view name(method);
    va_list args; va_start(args, method);
    if (name == "setUnderlineText") s.underline = va_arg(args, int);
    else if (name == "translate") {
        s.translations.push_back(static_cast<float>(va_arg(args, double)));
        s.translations.push_back(static_cast<float>(va_arg(args, double)));
    } else if (name == "draw") {
        ++s.draws;
        if (s.fail_draw) s.pending_exception = true;
    }
    va_end(args);
}
jint JNIEnv::CallIntMethod(jobject, jmethodID method, ...) {
    auto& s = fake_state();
    const std::string_view name(method);
    va_list args; va_start(args, method);
    jint result = 0;
    if (name == "getLineCount") result = s.max_lines == 1 ? 1 : 2;
    else if (name == "getHeight") result = s.max_lines == 1 ? 20 : 44;
    else if (name == "getLineTop") result = va_arg(args, int) == 0 ? 0 : 20;
    else if (name == "getLineBottom") result = va_arg(args, int) == 0 ? 20 : 44;
    else if (name == "getParagraphDirection") result = va_arg(args, int) == 0 ? -1 : 1;
    else if (name == "save") result = 1;
    else throw std::runtime_error("Unexpected Android integer method");
    va_end(args);
    return result;
}
jfloat JNIEnv::CallFloatMethod(jobject, jmethodID method, ...) {
    const std::string_view name(method);
    va_list args; va_start(args, method);
    const auto index = va_arg(args, int);
    va_end(args);
    // RTL getLineLeft excludes trailing whitespace; getLineWidth includes it.
    if (name == "getLineLeft") return index == 0 ? 90.f : 5.f;
    if (name == "getLineRight") return index == 0 ? 100.f : 40.f;
    if (name == "getLineWidth") return index == 0 ? 20.f : 35.f;
    throw std::runtime_error("Unexpected Android float method");
}
jboolean JNIEnv::CallBooleanMethod(jobject, jmethodID method, ...) {
    require(std::string_view(method) == "clipRect", "Unexpected Android boolean method");
    va_list args; va_start(args, method);
    for (int i = 0; i < 4; ++i) fake_state().clips.push_back(static_cast<float>(va_arg(args, double)));
    va_end(args);
    return JNI_TRUE;
}
int AndroidBitmap_getInfo(JNIEnv*, jobject, AndroidBitmapInfo* info) {
    const auto& s = fake_state();
    *info = {static_cast<std::uint32_t>(s.width), static_cast<std::uint32_t>(s.height),
             static_cast<std::uint32_t>(s.stride), ANDROID_BITMAP_FORMAT_RGBA_8888, 0};
    return ANDROID_BITMAP_RESULT_SUCCESS;
}
int AndroidBitmap_lockPixels(JNIEnv*, jobject, void** pixels) {
    *pixels = fake_state().null_pixels ? nullptr : fake_state().pixels.data();
    return ANDROID_BITMAP_RESULT_SUCCESS;
}
int AndroidBitmap_unlockPixels(JNIEnv*, jobject) { ++fake_state().unlocks; return ANDROID_BITMAP_RESULT_SUCCESS; }

int main() {
    try {
        TextRun run;
        run.text = "한글 — مرحبا 🙂";
        run.bounds = {10, 20, 100, 70};
        run.font_size = 16;
        run.line_height = 30;
        run.clip = Rect{15, 25, 80, 40};
        run.underline = true;
        require(equal(measure_text_width(run), 35) && equal(measure_text(run), 60),
                "Measured shaped text did not preserve native line geometry");
        require(fake_state().shaped_text == u"한글 — مرحبا 🙂", "Unicode text did not reach the OS intact");
        require(fake_state().font_kind == "asset" && fake_state().font_path == u"fonts/Pretendard-Regular.ttf",
                "The selected font asset was not passed to the OS");
        require(fake_state().fallback && fake_state().underline, "Android fallback metrics or underline were omitted");

        reset();
        CanvasFrame frame; frame.viewport = {0, 0, 128, 96}; frame.texts.push_back(run);
        auto image = rasterize_text(frame, 1);
        auto& s = fake_state();
        require(image.width == 128 && image.height == 96 && image.rgba[0] == 20 && image.rgba[1] == 40 &&
                image.rgba[2] == 60 && image.rgba[3] == 128 && image.rgba[128 * 4] == 128 &&
                image.rgba[128 * 4 + 3] == 10, "Bitmap stride or premultiplied color conversion failed");
        require(s.draws == 2 && s.translations.size() == 4 && equal(s.translations[0], -70) &&
                equal(s.translations[1], 25) && equal(s.translations[2], 5) && equal(s.translations[3], 33),
                "Shaped physical alignment or fixed line height was lost");
        require(s.clips.size() == 16 && equal(s.clips[0], 10) && equal(s.clips[4], 15) &&
                equal(s.clips[8], 10) && equal(s.clips[9], 20) && equal(s.clips[13], 50),
                "Text bounds, additional clip, or line clips were omitted");
        require(s.unlocks == 1 && s.pushes == 1 && s.pops == 1, "Native resources were not released");

        reset(); run.elide_right = true;
        (void)measure_text(run);
        require(fake_state().ellipsis && fake_state().max_lines == 1, "One-line ellipsis was not requested");
        reset(); fake_state().api = 24; (void)measure_text(run);
        require(!fake_state().fallback && fake_state().ellipsis, "API 24 requested an unavailable fallback API");
        reset(); fake_state().font_kind = "file"; (void)measure_text(run);
        require(fake_state().font_kind == "file" && !fake_state().font_path.empty(), "System font file was ignored");
        reset(); fake_state().api = 24; fake_state().collection_index = 1;
        bool rejected = false; try { (void)measure_text(run); } catch (const std::runtime_error&) { rejected = true; }
        require(rejected, "API 24 silently selected the wrong font collection face");
        reset(); fake_state().collection_index = 1; (void)measure_text(run);
        require(fake_state().selected_collection_index == 1, "Font collection face was not selected");

        reset(); fake_state().null_pixels = true;
        rejected = false; try { (void)rasterize_text(frame, 1); } catch (const std::runtime_error&) { rejected = true; }
        require(rejected && fake_state().unlocks == 1, "A null locked bitmap was not unlocked");
        reset(); fake_state().fail_draw = true;
        rejected = false; try { (void)rasterize_text(frame, 1); } catch (const std::runtime_error&) { rejected = true; }
        require(rejected && fake_state().pushes == fake_state().pops, "Java failure leaked local references");
        reset(); run.text = "\xff";
        rejected = false; try { (void)measure_text(run); } catch (const std::invalid_argument&) { rejected = true; }
        require(rejected, "Invalid UTF-8 was passed to the OS");
        std::cout << "Android text bridge: Unicode, layout, clipping, bitmap and failures passed with a fake OS\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
