#include "AndroidTextSupport.h"
#include <iiGUI/AndroidTextLayout.h>
#include <iiGUI/NativeText.h>
#include <iiGUI/TextLineNative.h>
#include <algorithm>
#include <cmath>
#include <mutex>

namespace ii::gui::detail {
namespace {
std::mutex font_mutex;

using namespace android_text;

struct Layout final {
    jobject value;
    std::vector<AndroidTextLine> lines;
    AndroidTextMetrics metrics;
};

class TextApi final {
    AndroidJni& jni_;
    JNIEnv* env_;
    jint api_;
    jclass paint_, text_paint_, layout_, builder_;
    jmethodID set_size_, set_color_, set_typeface_, set_antialias_, set_subpixel_, set_underline_;
    jmethodID obtain_, build_, line_count_, line_left_, line_right_, line_direction_, line_width_, line_top_, line_bottom_, height_;
public:
    explicit TextApi(AndroidJni& jni) : jni_(jni), env_(jni.env), api_(android_version(jni)),
        paint_(jni.type("android/graphics/Paint")), text_paint_(jni.type("android/text/TextPaint")),
        layout_(jni.type("android/text/Layout")), builder_(jni.type("android/text/StaticLayout$Builder")),
        set_size_(jni.method(paint_, "setTextSize", "(F)V")), set_color_(jni.method(paint_, "setColor", "(I)V")),
        set_typeface_(jni.method(paint_, "setTypeface", "(Landroid/graphics/Typeface;)Landroid/graphics/Typeface;")),
        set_antialias_(jni.method(paint_, "setAntiAlias", "(Z)V")), set_subpixel_(jni.method(paint_, "setSubpixelText", "(Z)V")),
        set_underline_(jni.method(paint_, "setUnderlineText", "(Z)V")),
        obtain_(jni.static_method(builder_, "obtain", "(Ljava/lang/CharSequence;IILandroid/text/TextPaint;I)Landroid/text/StaticLayout$Builder;")),
        build_(jni.method(builder_, "build", "()Landroid/text/StaticLayout;")),
        line_count_(jni.method(layout_, "getLineCount", "()I")), line_left_(jni.method(layout_, "getLineLeft", "(I)F")),
        line_right_(jni.method(layout_, "getLineRight", "(I)F")), line_direction_(jni.method(layout_, "getParagraphDirection", "(I)I")),
        line_width_(jni.method(layout_, "getLineWidth", "(I)F")), line_top_(jni.method(layout_, "getLineTop", "(I)I")),
        line_bottom_(jni.method(layout_, "getLineBottom", "(I)I")), height_(jni.method(layout_, "getHeight", "()I")) {}

    Layout make(const TextRun& run) {
        auto font = font_for(run);
        auto face = typeface(jni_, *font, api_);
        auto paint = object(jni_, env_->NewObject(text_paint_, jni_.method(text_paint_, "<init>", "()V")),
                            "Cannot create Android text paint");
        env_->CallVoidMethod(paint, set_size_, run.font_size); jni_.check();
        env_->CallVoidMethod(paint, set_color_, argb(run.color)); jni_.check();
        env_->CallVoidMethod(paint, set_antialias_, JNI_TRUE); jni_.check();
        env_->CallVoidMethod(paint, set_subpixel_, JNI_TRUE); jni_.check();
        env_->CallVoidMethod(paint, set_underline_, run.underline ? JNI_TRUE : JNI_FALSE); jni_.check();
        auto old_face = env_->CallObjectMethod(paint, set_typeface_, face); jni_.check();
        if (old_face) env_->DeleteLocalRef(old_face);
        auto text = static_cast<jstring>(object(jni_, jni_.string(run.text), "Cannot allocate Android text"));
        const auto length = env_->GetStringLength(text); jni_.check();
        const auto width = static_cast<jint>(std::clamp(std::ceil(double(run.bounds.width)), 1.0, 8192.0));
        auto builder = object(jni_, env_->CallStaticObjectMethod(builder_, obtain_, text, 0, length, paint, width),
                              "Cannot create Android text layout");
        if (api_ >= 28) {
            auto result = object(jni_, env_->CallObjectMethod(builder,
                jni_.method(builder_, "setUseLineSpacingFromFallbacks", "(Z)Landroid/text/StaticLayout$Builder;"), JNI_TRUE),
                "Cannot enable Android fallback font metrics");
            env_->DeleteLocalRef(result);
        }
        if (run.elide_right) {
            auto truncation = jni_.type("android/text/TextUtils$TruncateAt");
            auto end = static_field(jni_, truncation, "END", "Landroid/text/TextUtils$TruncateAt;");
            auto ellipsize = object(jni_, env_->CallObjectMethod(builder,
                jni_.method(builder_, "setEllipsize", "(Landroid/text/TextUtils$TruncateAt;)Landroid/text/StaticLayout$Builder;"), end),
                "Cannot configure Android text ellipsis");
            env_->DeleteLocalRef(ellipsize);
            auto maximum = object(jni_, env_->CallObjectMethod(builder,
                jni_.method(builder_, "setMaxLines", "(I)Landroid/text/StaticLayout$Builder;"), 1),
                "Cannot limit Android text lines");
            env_->DeleteLocalRef(maximum);
        }
        auto value = object(jni_, env_->CallObjectMethod(builder, build_), "Cannot build Android text layout");
        const auto count = env_->CallIntMethod(value, line_count_); jni_.check();
        const auto height = env_->CallIntMethod(value, height_); jni_.check();
        if (count <= 0 || count > 65537 || height < 0) throw std::runtime_error("Android text layout exceeds its line budget");
        std::vector<AndroidTextLine> lines;
        lines.reserve(static_cast<std::size_t>(count));
        for (jint i = 0; i < count; ++i) {
            auto left = env_->CallFloatMethod(value, line_left_, i); jni_.check();
            const auto advance = env_->CallFloatMethod(value, line_width_, i); jni_.check();
            const auto direction = env_->CallIntMethod(value, line_direction_, i); jni_.check();
            if(direction<0){const auto right=env_->CallFloatMethod(value,line_right_,i);jni_.check();left=right-advance;}
            const auto top = env_->CallIntMethod(value, line_top_, i); jni_.check();
            const auto bottom = env_->CallIntMethod(value, line_bottom_, i); jni_.check();
            lines.push_back({left, advance, float(top), float(bottom)});
        }
        auto metrics = android_text_metrics(lines, float(height), run.line_height);
        return {value, std::move(lines), metrics};
    }
    jclass layout_type() const noexcept { return layout_; }
};

class BitmapCanvas final {
    AndroidJni& jni_;
    JNIEnv* env_;
    jobject bitmap_, canvas_;
    jclass canvas_type_;
    jmethodID save_, restore_, clip_, translate_, draw_;
    std::uint32_t width_, height_;
public:
    BitmapCanvas(AndroidJni& jni, TextApi& text, std::uint32_t width, std::uint32_t height, double scale)
        : jni_(jni), env_(jni.env), width_(width), height_(height) {
        auto bitmap_type = jni.type("android/graphics/Bitmap");
        auto config_type = jni.type("android/graphics/Bitmap$Config");
        auto config = static_field(jni, config_type, "ARGB_8888", "Landroid/graphics/Bitmap$Config;");
        bitmap_ = object(jni, env_->CallStaticObjectMethod(bitmap_type,
            jni.static_method(bitmap_type, "createBitmap", "(IILandroid/graphics/Bitmap$Config;)Landroid/graphics/Bitmap;"),
            static_cast<jint>(width), static_cast<jint>(height), config), "Cannot create Android text bitmap");
        env_->CallVoidMethod(bitmap_, jni.method(bitmap_type, "eraseColor", "(I)V"), 0); jni.check();
        canvas_type_ = jni.type("android/graphics/Canvas");
        canvas_ = object(jni, env_->NewObject(canvas_type_, jni.method(canvas_type_, "<init>", "(Landroid/graphics/Bitmap;)V"), bitmap_),
                         "Cannot create Android text canvas");
        save_ = jni.method(canvas_type_, "save", "()I");
        restore_ = jni.method(canvas_type_, "restoreToCount", "(I)V");
        clip_ = jni.method(canvas_type_, "clipRect", "(FFFF)Z");
        translate_ = jni.method(canvas_type_, "translate", "(FF)V");
        draw_ = jni.method(text.layout_type(), "draw", "(Landroid/graphics/Canvas;)V");
        env_->CallVoidMethod(canvas_, jni.method(canvas_type_, "scale", "(FF)V"), float(scale), float(scale)); jni.check();
    }
    jint save() { auto value = env_->CallIntMethod(canvas_, save_); jni_.check(); return value; }
    void restore(jint value) { env_->CallVoidMethod(canvas_, restore_, value); jni_.check(); }
    bool clip(float left, float top, float right, float bottom) {
        auto value = env_->CallBooleanMethod(canvas_, clip_, left, top, right, bottom); jni_.check(); return value == JNI_TRUE;
    }
    void draw(const TextRun& run, const Layout& layout) {
        const auto outer = save();
        const auto& r = run.bounds;
        bool visible = clip(r.x, r.y, r.x + r.width, r.y + r.height);
        if (visible && run.clip) {
            const auto& c = *run.clip;
            visible = clip(c.x, c.y, c.x + c.width, c.y + c.height);
        }
        if (visible) for (std::size_t i = 0; i < layout.lines.size(); ++i) {
            const auto place = android_text_placement(r, run.alignment, layout.lines[i], i, run.line_height);
            if (place.clip_bottom <= r.y || place.clip_top >= r.y + r.height) continue;
            const auto line = save();
            if (clip(r.x, place.clip_top, r.x + r.width, place.clip_bottom)) {
                env_->CallVoidMethod(canvas_, translate_, place.x, place.y); jni_.check();
                env_->CallVoidMethod(layout.value, draw_, canvas_); jni_.check();
            }
            restore(line);
        }
        restore(outer);
    }
    PixelBuffer pixels() {
        return bitmap_pixels(jni_,bitmap_,width_,height_);
    }
};
}

float measure_text_width(const TextRun& run) {
    std::lock_guard lock(font_mutex);
    AndroidJni jni; TextApi api(jni); auto copy = run; copy.bounds.width = 8192;
    return api.make(copy).metrics.width;
}
float measure_text(const TextRun& run) {
    std::lock_guard lock(font_mutex);
    AndroidJni jni; TextApi api(jni);
    return std::min(8192.f, std::max(run.font_size, api.make(run).metrics.height));
}
PixelBuffer rasterize_text(const CanvasFrame& frame, double scale) {
    std::lock_guard lock(font_mutex);
    const auto [width, height] = canvas_pixels(frame, scale);
    AndroidJni jni; TextApi api(jni); BitmapCanvas canvas(jni, api, width, height, scale);
    for (const auto& run : frame.texts) {
        LocalFrame references(jni);
        auto layout = api.make(run);
        canvas.draw(run, layout);
    }
    return canvas.pixels();
}
} // namespace ii::gui::detail
