#include "AndroidGlobalRef.h"
#include "AndroidTextLineModel.h"
#include "AndroidTextSupport.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

namespace ii::gui::detail {
namespace {
using namespace android_text;
constexpr int maximum_android_line_width=8*1024*1024;
constexpr std::size_t maximum_android_tab_stops=8192;
constexpr std::size_t maximum_android_selection_spans=65536;
constexpr float selection_precision=64;

jfieldID field(AndroidJni& jni,jclass type,const char* name,const char* signature){
    auto value=jni.env->GetFieldID(type,name,signature);jni.check();
    if(!value)throw std::runtime_error("Android text field is unavailable");return value;
}
jint static_integer(AndroidJni& jni,jclass type,const char* name,const char* signature){
    auto value=jni.env->GetStaticFieldID(type,name,signature);jni.check();
    if(!value)throw std::runtime_error("Android text constant is unavailable");
    auto result=jni.env->GetStaticIntField(type,value);jni.check();return result;
}
int layout_width(double value){
    if(!std::isfinite(value)||value<0||value>maximum_android_line_width)
        throw std::length_error("Android paragraph exceeds its native coordinate budget");
    return std::max(1,int(std::ceil(value)));
}

class Shaper;
class Layout final:public AndroidParagraphLayout {
    std::shared_ptr<const Shaper> shaper_;
    AndroidGlobalRef value_;
    int width_;
    std::vector<AndroidParagraphLine> lines_;
public:
    Layout(std::shared_ptr<const Shaper>,AndroidGlobalRef,int,std::vector<AndroidParagraphLine>);
    const std::vector<AndroidParagraphLine>& lines()const noexcept override{return lines_;}
    double primary(std::size_t)const override;
    std::size_t hit(std::size_t,double)const override;
    bool rtl(std::size_t)const override;
    std::vector<TextLineSpan> selection(std::size_t,std::size_t,std::size_t)const override;
    TextBitmap render(std::size_t,double,double,double,Color,double)const override;
};

class Shaper final:public AndroidParagraphShaper,public std::enable_shared_from_this<Shaper> {
    TextIndex display_;
    TextLineStyle style_;
    jint api_=0,length_=0;
    int unwrapped_width_=1;
    double last_tab_stop_=0;
    AndroidGlobalRef text_,face_;

    jobject paint(AndroidJni& jni,Color color)const{
        auto paint_type=jni.type("android/graphics/Paint"),text_paint=jni.type("android/text/TextPaint");
        auto value=object(jni,jni.env->NewObject(text_paint,jni.method(text_paint, "<init>", "()V")),"Cannot create Android editor paint");
        jni.env->CallVoidMethod(value,jni.method(paint_type, "setTextSize", "(F)V"),float(style_.font_size));jni.check();
        jni.env->CallVoidMethod(value,jni.method(paint_type, "setColor", "(I)V"),argb(color));jni.check();
        jni.env->CallVoidMethod(value,jni.method(paint_type, "setAntiAlias", "(Z)V"),JNI_TRUE);jni.check();
        jni.env->CallVoidMethod(value,jni.method(paint_type, "setSubpixelText", "(Z)V"),JNI_TRUE);jni.check();
        auto previous=jni.env->CallObjectMethod(value,jni.method(paint_type, "setTypeface", "(Landroid/graphics/Typeface;)Landroid/graphics/Typeface;"),face_.get());jni.check();
        if(previous)jni.env->DeleteLocalRef(previous);return value;
    }
    jobject build(AndroidJni& jni,int width,std::size_t maximum_lines,Color color)const{
        auto paint_object=paint(jni,color);auto builder_type=jni.type("android/text/StaticLayout$Builder");
        auto builder=object(jni,jni.env->CallStaticObjectMethod(builder_type,
            jni.static_method(builder_type, "obtain", "(Ljava/lang/CharSequence;IILandroid/text/TextPaint;I)Landroid/text/StaticLayout$Builder;"),
            text_.get(),jint(0),length_,paint_object,jint(width)),"Cannot create Android editor layout");
        // StaticLayout may ignore maxLines without ellipsizing. Request one
        // extra line and reject any ellipsis instead of exposing partial text.
        // A wide ellipsis boundary retains glyphs wider than the viewport;
        // the independent layout width continues to control wrapping.
        const auto limit=static_cast<jint>(std::min<std::size_t>(maximum_lines+1,65537));
        auto max=object(jni,jni.env->CallObjectMethod(builder,
            jni.method(builder_type, "setMaxLines", "(I)Landroid/text/StaticLayout$Builder;"),limit),"Cannot bound Android editor lines");
        jni.env->DeleteLocalRef(max);
        auto truncate_type=jni.type("android/text/TextUtils$TruncateAt");
        auto truncate=static_field(jni,truncate_type, "END", "Landroid/text/TextUtils$TruncateAt;");
        auto ellipsize=object(jni,jni.env->CallObjectMethod(builder,
            jni.method(builder_type, "setEllipsize", "(Landroid/text/TextUtils$TruncateAt;)Landroid/text/StaticLayout$Builder;"),truncate),
            "Cannot bound Android editor layout");jni.env->DeleteLocalRef(ellipsize);
        auto ellipsis_width=object(jni,jni.env->CallObjectMethod(builder,
            jni.method(builder_type, "setEllipsizedWidth", "(I)Landroid/text/StaticLayout$Builder;"),jint(maximum_android_line_width)),
            "Cannot set Android editor ellipsis boundary");jni.env->DeleteLocalRef(ellipsis_width);
        if(api_>=28){auto fallback=object(jni,jni.env->CallObjectMethod(builder,
            jni.method(builder_type, "setUseLineSpacingFromFallbacks", "(Z)Landroid/text/StaticLayout$Builder;"),JNI_TRUE),
            "Cannot configure Android fallback metrics");jni.env->DeleteLocalRef(fallback);}
        return object(jni,jni.env->CallObjectMethod(builder,jni.method(builder_type, "build", "()Landroid/text/StaticLayout;")),
                      "Cannot build Android editor layout");
    }
    std::vector<AndroidParagraphLine> geometry(AndroidJni& jni,jobject layout,std::size_t maximum_lines)const{
        auto type=jni.type("android/text/Layout");auto count_method=jni.method(type, "getLineCount", "()I");
        const auto count=jni.env->CallIntMethod(layout,count_method);jni.check();
        if(count<=0)throw std::runtime_error("Android returned an empty paragraph layout");
        if(std::size_t(count)>maximum_lines)throw std::length_error("Android paragraph exceeds its line budget");
        auto start=jni.method(type, "getLineStart", "(I)I"),end=jni.method(type, "getLineEnd", "(I)I");
        auto ellipsis=jni.method(type, "getEllipsisCount", "(I)I");
        auto top=jni.method(type, "getLineTop", "(I)I"),bottom=jni.method(type, "getLineBottom", "(I)I");
        auto baseline=jni.method(type, "getLineBaseline", "(I)I");
        auto left=jni.method(type, "getLineLeft", "(I)F"),right=jni.method(type, "getLineRight", "(I)F");
        auto direction=jni.method(type, "getParagraphDirection", "(I)I"),width=jni.method(type, "getLineWidth", "(I)F");
        std::vector<AndroidParagraphLine> result;result.reserve(std::size_t(count));
        for(jint i=0;i<count;++i){
            const auto omitted=jni.env->CallIntMethod(layout,ellipsis,i);jni.check();
            if(omitted!=0)throw std::length_error("Android paragraph exceeded its native line budget");
            const auto a=jni.env->CallIntMethod(layout,start,i);jni.check();const auto b=jni.env->CallIntMethod(layout,end,i);jni.check();
            const auto t=jni.env->CallIntMethod(layout,top,i);jni.check();const auto d=jni.env->CallIntMethod(layout,bottom,i);jni.check();
            const auto base=jni.env->CallIntMethod(layout,baseline,i);jni.check();auto x=jni.env->CallFloatMethod(layout,left,i);jni.check();
            const auto w=jni.env->CallFloatMethod(layout,width,i);jni.check();
            const auto dir=jni.env->CallIntMethod(layout,direction,i);jni.check();
            if(dir<0){const auto r=jni.env->CallFloatMethod(layout,right,i);jni.check();x=r-w;}
            if(a<0||b<a||b>length_)throw std::runtime_error("Android returned invalid UTF-16 line boundaries");
            result.push_back({std::size_t(a),std::size_t(b),double(x),double(w),double(t),double(d),double(base)});
        }
        if(result.back().last!=std::size_t(length_))throw std::length_error("Android paragraph was truncated by its line budget");
        return result;
    }
public:
    Shaper(TextIndex display,TextLineStyle style):display_(std::move(display)),style_(std::move(style)){
        AndroidJni jni;api_=android_version(jni);length_=static_cast<jint>(display_.utf16_size());
        auto font=font_for(style_.font_family,style_.font_weight);
        face_=AndroidGlobalRef(jni,typeface(jni,*font,api_));
        auto source=static_cast<jstring>(object(jni,jni.string(display_.text()),"Cannot allocate Android editor text"));
        auto measure_paint=paint(jni,{0,0,0,1});auto layout_type=jni.type("android/text/Layout");
        auto desired=jni.env->CallStaticFloatMethod(layout_type,
            jni.static_method(layout_type, "getDesiredWidth", "(Ljava/lang/CharSequence;Landroid/text/TextPaint;)F"),source,measure_paint);jni.check();
        if(!std::isfinite(desired)||desired<0)throw std::runtime_error("Android returned an invalid paragraph width");
        const auto tabs=std::size_t(std::count(display_.text().begin(),display_.text().end(),'\t'));
        jobject sequence=source;
        if(tabs){
            // Every tab stop is explicit; never silently fall back to Android's
            // platform-default interval once the requested list is exhausted.
            const double upper=double(desired)+double(tabs)*(style_.tab_stop_distance+20)+style_.font_size*8+64;
            const auto stops=std::ceil(upper/style_.tab_stop_distance)+1;
            if(!std::isfinite(stops)||stops>maximum_android_tab_stops)throw std::length_error("Android tab-stop budget exceeded");
            auto spannable=jni.type("android/text/SpannableString");
            sequence=object(jni,jni.env->NewObject(spannable,jni.method(spannable, "<init>", "(Ljava/lang/CharSequence;)V"),source),
                            "Cannot create Android tabbed paragraph");
            auto set_span=jni.method(spannable, "setSpan", "(Ljava/lang/Object;III)V");
            auto tab_type=jni.type("android/text/style/TabStopSpan$Standard");
            auto tab_init=jni.method(tab_type, "<init>", "(I)V");
            auto spanned=jni.type("android/text/Spanned");
            const auto flags=static_integer(jni,spanned, "SPAN_EXCLUSIVE_EXCLUSIVE", "I");
            for(std::size_t i=1;i<=std::size_t(stops);++i){
                const auto offset=std::llround(double(i)*style_.tab_stop_distance);
                if(offset<=0||offset>maximum_android_line_width)throw std::length_error("Android tab position exceeds its coordinate budget");
                auto tab=object(jni,jni.env->NewObject(tab_type,tab_init,jint(offset)),"Cannot create Android tab stop");
                jni.env->CallVoidMethod(sequence,set_span,tab,jint(0),length_,flags);jni.check();jni.env->DeleteLocalRef(tab);
                last_tab_stop_=double(offset);
            }
            desired=jni.env->CallStaticFloatMethod(layout_type,
                jni.static_method(layout_type, "getDesiredWidth", "(Ljava/lang/CharSequence;Landroid/text/TextPaint;)F"),sequence,measure_paint);jni.check();
        }
        if(!std::isfinite(desired)||desired<0)throw std::runtime_error("Android returned an invalid paragraph width");
        unwrapped_width_=layout_width(double(desired)+style_.font_size*2+32);
        text_=AndroidGlobalRef(jni,sequence);
    }
    std::shared_ptr<AndroidParagraphLayout> shape(std::optional<double> width,std::size_t maximum_lines)const override{
        if(maximum_lines==0||maximum_lines>65536)throw std::invalid_argument("Invalid Android paragraph line budget");
        AndroidJni jni;int candidate=width?layout_width(*width):unwrapped_width_;
        for(;;){
            LocalFrame frame(jni);
            auto object_layout=build(jni,candidate,width?maximum_lines:1,{0,0,0,1});
            std::vector<AndroidParagraphLine> lines;
            try{
                lines=geometry(jni,object_layout,width?maximum_lines:1);
                if(!width&&(lines.size()!=1||lines.front().first!=0))throw std::length_error("Android unwrapped paragraph requires more width");
            }catch(const std::length_error&){
                if(width||candidate>=maximum_android_line_width)throw;
                candidate=std::min(maximum_android_line_width,candidate>maximum_android_line_width/2?maximum_android_line_width:candidate*2);
                continue;
            }
            if(last_tab_stop_>0&&!width&&lines.front().width+style_.tab_stop_distance>=last_tab_stop_)
                throw std::length_error("Android tab positions exceed their admitted range");
            auto global=AndroidGlobalRef(jni,object_layout);
            return std::make_shared<Layout>(shared_from_this(),std::move(global),candidate,std::move(lines));
        }
    }
    jobject colored_layout(AndroidJni& jni,int width,std::size_t limit,Color color)const{return build(jni,width,limit,color);}
};

Layout::Layout(std::shared_ptr<const Shaper> shaper,AndroidGlobalRef value,int width,std::vector<AndroidParagraphLine> lines)
    :shaper_(std::move(shaper)),value_(std::move(value)),width_(width),lines_(std::move(lines)){}
double Layout::primary(std::size_t position)const{
    AndroidJni jni;auto type=jni.type("android/text/Layout");
    const auto value=jni.env->CallFloatMethod(value_.get(),jni.method(type, "getPrimaryHorizontal", "(I)F"),jint(position));jni.check();return value;
}
std::size_t Layout::hit(std::size_t line,double x)const{
    AndroidJni jni;auto type=jni.type("android/text/Layout");
    const auto value=jni.env->CallIntMethod(value_.get(),jni.method(type, "getOffsetForHorizontal", "(IF)I"),jint(line),float(x));jni.check();
    if(value<0)throw std::runtime_error("Android returned an invalid text hit");return std::size_t(value);
}
bool Layout::rtl(std::size_t position)const{
    AndroidJni jni;auto type=jni.type("android/text/Layout");
    const auto value=jni.env->CallBooleanMethod(value_.get(),jni.method(type, "isRtlCharAt", "(I)Z"),jint(position));jni.check();return value==JNI_TRUE;
}
std::vector<TextLineSpan> Layout::selection(std::size_t row,std::size_t first,std::size_t last)const{
    if(row>=lines_.size())throw std::out_of_range("Android selection row is invalid");
    if(first==last)return {};
    AndroidJni jni;auto layout=jni.type("android/text/Layout"),path_type=jni.type("android/graphics/Path");
    auto path=object(jni,jni.env->NewObject(path_type,jni.method(path_type, "<init>", "()V")),"Cannot create Android selection path");
    jni.env->CallVoidMethod(value_.get(),jni.method(layout, "getSelectionPath", "(IILandroid/graphics/Path;)V"),jint(first),jint(last),path);jni.check();
    auto matrix_type=jni.type("android/graphics/Matrix");
    auto matrix=object(jni,jni.env->NewObject(matrix_type,jni.method(matrix_type, "<init>", "()V")),"Cannot create Android selection transform");
    jni.env->CallVoidMethod(matrix,jni.method(matrix_type, "setScale", "(FF)V"),selection_precision,selection_precision);jni.check();
    jni.env->CallVoidMethod(path,jni.method(path_type, "transform", "(Landroid/graphics/Matrix;)V"),matrix);jni.check();
    const auto& line=lines_[row];
    const auto to_int=[](double value){if(!std::isfinite(value)||value<double(std::numeric_limits<jint>::min())||value>double(std::numeric_limits<jint>::max()))throw std::length_error("Android selection exceeds its coordinate budget");return jint(value);};
    const auto left=to_int(std::floor(line.left*selection_precision)),right=to_int(std::ceil((line.left+line.width)*selection_precision));
    const auto top=to_int(std::floor(line.top*selection_precision)),bottom=to_int(std::ceil(line.bottom*selection_precision));
    if(right<=left||bottom<=top)return {};
    auto region_type=jni.type("android/graphics/Region");
    auto clip=object(jni,jni.env->NewObject(region_type,jni.method(region_type, "<init>", "(IIII)V"),left,top,right,bottom),"Cannot create Android selection clip");
    auto region=object(jni,jni.env->NewObject(region_type,jni.method(region_type, "<init>", "()V")),"Cannot create Android selection region");
    const auto present=jni.env->CallBooleanMethod(region,jni.method(region_type, "setPath", "(Landroid/graphics/Path;Landroid/graphics/Region;)Z"),path,clip);jni.check();
    if(present!=JNI_TRUE)return {};
    auto iterator_type=jni.type("android/graphics/RegionIterator");
    auto iterator=object(jni,jni.env->NewObject(iterator_type,jni.method(iterator_type, "<init>", "(Landroid/graphics/Region;)V"),region),"Cannot enumerate Android selection");
    auto rect_type=jni.type("android/graphics/Rect");auto rect=object(jni,jni.env->NewObject(rect_type,jni.method(rect_type, "<init>", "()V")),"Cannot allocate Android selection rectangle");
    auto left_field=field(jni,rect_type, "left", "I"),right_field=field(jni,rect_type, "right", "I");
    auto next=jni.method(iterator_type, "next", "(Landroid/graphics/Rect;)Z");std::vector<TextLineSpan> result;
    for(;;){const auto has=jni.env->CallBooleanMethod(iterator,next,rect);jni.check();if(has!=JNI_TRUE)break;
        const auto a=jni.env->GetIntField(rect,left_field);jni.check();const auto b=jni.env->GetIntField(rect,right_field);jni.check();
        if(b<a)throw std::runtime_error("Android returned a reversed selection rectangle");
        if(result.size()==maximum_android_selection_spans)throw std::length_error("Android selection span budget exceeded");
        if(b>a)result.push_back({double(a)/selection_precision,double(b-a)/selection_precision});
    }return result;
}
TextBitmap Layout::render(std::size_t row,double x,double width,double height,Color color,double scale)const{
    if(row>=lines_.size())throw std::out_of_range("Android render row is invalid");
    const auto pixel_width=std::uint32_t(std::ceil(width*scale)),pixel_height=std::uint32_t(std::ceil(height*scale));
    AndroidJni jni;auto bitmap_type=jni.type("android/graphics/Bitmap"),config_type=jni.type("android/graphics/Bitmap$Config");
    auto config=static_field(jni,config_type, "ARGB_8888", "Landroid/graphics/Bitmap$Config;");
    auto bitmap=object(jni,jni.env->CallStaticObjectMethod(bitmap_type,
        jni.static_method(bitmap_type, "createBitmap", "(IILandroid/graphics/Bitmap$Config;)Landroid/graphics/Bitmap;"),
        jint(pixel_width),jint(pixel_height),config),"Cannot create Android editor bitmap");
    jni.env->CallVoidMethod(bitmap,jni.method(bitmap_type, "eraseColor", "(I)V"),jint(0));jni.check();
    const auto& line=lines_[row];
    {
        auto canvas_type=jni.type("android/graphics/Canvas");
        auto canvas=object(jni,jni.env->NewObject(canvas_type,jni.method(canvas_type, "<init>", "(Landroid/graphics/Bitmap;)V"),bitmap),"Cannot create Android editor canvas");
        jni.env->CallVoidMethod(canvas,jni.method(canvas_type, "scale", "(FF)V"),float(scale),float(scale));jni.check();
        const auto visible=jni.env->CallBooleanMethod(canvas,jni.method(canvas_type, "clipRect", "(FFFF)Z"),0.f,0.f,float(width),float(height));jni.check();
        if(visible==JNI_TRUE){
            const auto y=(height-(line.bottom-line.top))/2-line.top;
            jni.env->CallVoidMethod(canvas,jni.method(canvas_type, "translate", "(FF)V"),float(-x-line.left),float(y));jni.check();
            const auto line_visible=jni.env->CallBooleanMethod(canvas,jni.method(canvas_type, "clipRect", "(FFFF)Z"),
                float(-maximum_android_line_width),float(line.top),float(maximum_android_line_width),float(line.bottom));jni.check();
            if(line_visible!=JNI_TRUE){auto pixels=bitmap_pixels(jni,bitmap,pixel_width,pixel_height);return {pixel_width,pixel_height,std::move(pixels.rgba)};}
            auto colored=shaper_->colored_layout(jni,width_,lines_.size(),color);auto layout_type=jni.type("android/text/Layout");
            jni.env->CallVoidMethod(colored,jni.method(layout_type, "draw", "(Landroid/graphics/Canvas;)V"),canvas);jni.check();
        }
    }
    auto pixels=bitmap_pixels(jni,bitmap,pixel_width,pixel_height);return {pixel_width,pixel_height,std::move(pixels.rgba)};
}
}
std::unique_ptr<NativeTextLine> native_text_line(TextIndex display,TextLineStyle style){
    auto shaper=std::make_shared<Shaper>(display,style);return android_text_line_model(std::move(display),std::move(style),std::move(shaper));
}
} // namespace ii::gui::detail
