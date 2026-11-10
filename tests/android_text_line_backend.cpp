#include <iiGUI/AndroidJni.h>
#include <iiGUI/AndroidLabelFont.h>
#include <iiGUI/FontNative.h>
#include <iiGUI/TextLine.h>
#include <android/bitmap.h>
#include <algorithm>
#include <cstdarg>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string_view>

using namespace ii;
using namespace ii::gui;
using namespace ii::gui::detail;
FakeState& fake_state(){static FakeState state;return state;}
JNIEnv& fake_env(){static JNIEnv value;return value;}
JavaVM& fake_vm(){static JavaVM value;return value;}
namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(core::Result<T> value){if(!value){if(value.error().cause)std::rethrow_exception(value.error().cause);throw std::runtime_error(value.error().message);}return std::move(*value);}
const FakeRow& row(jobject object,int i){return object->rows.at(std::size_t(i));}
}
std::shared_ptr<AndroidLabelFont> ii::gui::detail::native_android_font(std::string_view,int){return std::make_shared<AndroidLabelFont>();}
jint JNIEnv::PushLocalFrame(jint){++fake_state().pushes;return 0;}
jobject JNIEnv::PopLocalFrame(jobject){++fake_state().pops;return nullptr;}
void JNIEnv::DeleteLocalRef(jobject){}
jint JNIEnv::GetJavaVM(JavaVM** result){*result=&fake_vm();return JNI_OK;}
jobject JNIEnv::NewGlobalRef(jobject value){++fake_state().global_refs;return value;}
void JNIEnv::DeleteGlobalRef(jobject){--fake_state().global_refs;}
jint JavaVM::GetEnv(void** result,jint){*result=&fake_env();return JNI_OK;}
jint JavaVM::AttachCurrentThread(JNIEnv** result,void*){*result=&fake_env();return JNI_OK;}
jint JavaVM::DetachCurrentThread(){return JNI_OK;}
jfieldID JNIEnv::GetStaticFieldID(jclass,const char* name,const char*){return fake_state().field(name);}
jfieldID JNIEnv::GetFieldID(jclass,const char* name,const char*){return fake_state().field(name);}
jint JNIEnv::GetStaticIntField(jclass,jfieldID value){return value->name=="SDK_INT"?fake_state().api:0x21;}
jobject JNIEnv::GetStaticObjectField(jclass,jfieldID value){return fake_state().make(value->name);}
jint JNIEnv::GetIntField(jobject value,jfieldID f){return jint(f->name=="left"?value->rects.at(0).left:value->rects.at(0).right);}

jobject JNIEnv::NewObject(jclass type,jmethodID method,...){
    auto& s=fake_state();auto result=s.make(type->type);va_list args;va_start(args,method);
    if(type->type=="android/text/SpannableString")result->text=va_arg(args,jobject)->text;
    else if(type->type=="android/text/style/TabStopSpan$Standard")result->number=va_arg(args,int);
    else if(type->type=="android/graphics/Region"&&method->signature=="(IIII)V"){
        FakeRect r;r.left=va_arg(args,int);r.top=va_arg(args,int);r.right=va_arg(args,int);r.bottom=va_arg(args,int);result->rects.push_back(r);
    }else if(type->type=="android/graphics/RegionIterator")result->source=va_arg(args,jobject);
    va_end(args);return result;
}
jobject JNIEnv::CallStaticObjectMethod(jclass,jmethodID method,...){
    auto& s=fake_state();auto name=std::string_view(method->name);va_list args;va_start(args,method);jobject result=nullptr;
    if(name=="createFromAsset"){
        (void)va_arg(args,jobject);auto path=va_arg(args,jstring);s.font_path=path->text;s.font_kind="asset";result=s.make("face");
    }else if(name=="createFromFile"){
        auto path=va_arg(args,jstring);s.font_path=path->text;s.font_kind="file";result=s.make("face");
    }else if(name=="obtain"){
        auto text=va_arg(args,jobject);const auto first=va_arg(args,int),last=va_arg(args,int);
        auto paint=va_arg(args,jobject);const auto width=va_arg(args,int);
        check(first==0&&last==int(text->text.size()),"Android editor lost a UTF-16 range");
        s.shaped_text=text->text;result=s.make("builder");result->source=text;result->paint=paint;result->width=width;
    }else if(name=="createBitmap"){
        s.width=va_arg(args,int);s.height=va_arg(args,int);(void)va_arg(args,jobject);s.stride=s.width*4+8;
        s.pixels.assign(std::size_t(s.stride)*s.height,0);
        if(!s.pixels.empty()){s.pixels[0]=10;s.pixels[1]=20;s.pixels[2]=30;s.pixels[3]=128;}
        result=s.make("bitmap");
    }else throw std::runtime_error("Unexpected static Android editor method");
    va_end(args);return result;
}
jfloat JNIEnv::CallStaticFloatMethod(jclass,jmethodID method,...){
    check(method->name=="getDesiredWidth","Unexpected static Android editor float method");
    va_list args;va_start(args,method);auto text=va_arg(args,jobject);(void)va_arg(args,jobject);va_end(args);
    return text->text==u"abc אבג xyz"||text->text==u"a\tb"?110.f:float(text->text.size()*10);
}
jobject JNIEnv::CallObjectMethod(jobject target,jmethodID method,...){
    auto& s=fake_state();auto name=std::string_view(method->name);va_list args;va_start(args,method);jobject result=target;
    if(name=="getAssets")result=s.make("assets");
    else if(name=="setTypeface")result=nullptr;
    else if(name=="setMaxLines")target->maximum=va_arg(args,int);
    else if(name=="setUseLineSpacingFromFallbacks")(void)va_arg(args,int);
    else if(name=="setEllipsize"){(void)va_arg(args,jobject);target->ellipsize=true;}
    else if(name=="setEllipsizedWidth")target->ellipsized_width=va_arg(args,int);
    else if(name=="build"){
        result=s.make("layout");result->source=target->source;result->paint=target->paint;result->width=target->width;
        const auto length=int(result->source->text.size());
        if(result->source->text==u"abc אבג xyz"&&result->width<=35){
            result->rows={{0,4,0,20,15,0,0,35},{4,7,20,42,36,0,5,30},{7,11,42,62,57,0,0,35}};
        }else if(result->source->text==u"wide"&&result->width==1){
            result->rows={{0,1,0,20,15,0,0,10},{1,2,20,40,35,0,0,10},
                          {2,3,40,60,55,0,0,10},{3,4,60,80,75,0,0,10}};
            if(target->ellipsize&&target->ellipsized_width<10)result->rows.back().ellipsis=1;
        }else result->rows={{0,length,0,20,15,0,0,float(length*10)}};
        if(int(result->rows.size())>target->maximum){result->rows.resize(std::size_t(target->maximum));result->rows.back().ellipsis=1;result->rows.back().last=length;}
    }else throw std::runtime_error("Unexpected Android editor object method");
    va_end(args);return result;
}
void JNIEnv::CallVoidMethod(jobject target,jmethodID method,...){
    auto& s=fake_state();auto name=std::string_view(method->name);va_list args;va_start(args,method);
    if(name=="setColor")target->color=va_arg(args,int);
    else if(name=="setSpan"){
        auto span=va_arg(args,jobject);(void)va_arg(args,int);(void)va_arg(args,int);(void)va_arg(args,int);s.tabs.push_back(span->number);
    }else if(name=="getSelectionPath"){
        const auto first=va_arg(args,int),last=va_arg(args,int);auto path=va_arg(args,jobject);path->rects.clear();
        if(target->rows.size()>1&&first>=4&&last<=7){
            if(first==6&&last==7)path->rects.push_back({5,20,15,42});
            else path->rects.push_back({5,20,35,42});
        }else if(first==2&&last==5){path->rects={{20,0,30,20},{50,0,60,20}};}
        else path->rects={{double(first*10),0,double(last*10),20}};
    }else if(name=="setScale")target->scale=float(va_arg(args,double));
    else if(name=="transform"){
        auto matrix=va_arg(args,jobject);for(auto& r:target->rects){r.left*=matrix->scale;r.right*=matrix->scale;r.top*=matrix->scale;r.bottom*=matrix->scale;}
    }else if(name=="translate"){
        s.translations.push_back(float(va_arg(args,double)));s.translations.push_back(float(va_arg(args,double)));
    }else if(name=="draw"){
        ++s.draws;if(s.fail_draw)s.pending=true;
    }
    va_end(args);
}
jint JNIEnv::CallIntMethod(jobject target,jmethodID method,...){
    auto name=std::string_view(method->name);va_list args;va_start(args,method);jint result=0;
    if(name=="getLineCount")result=jint(target->rows.size());
    else if(name=="getOffsetForHorizontal"){
        const auto index=va_arg(args,int);const auto x=va_arg(args,double);
        if(index==1)result=x>30?4:x>20?5:x>10?6:7;
        else result=std::clamp(int(std::lround(x/10)),row(target,index).first,row(target,index).last);
    }else {
        const auto i=va_arg(args,int);const auto& r=row(target,i);
        if(name=="getLineStart")result=r.first;else if(name=="getLineEnd")result=r.last;
        else if(name=="getLineTop")result=r.top;else if(name=="getLineBottom")result=r.bottom;
        else if(name=="getLineBaseline")result=r.baseline;else if(name=="getEllipsisCount")result=r.ellipsis;
        else if(name=="getParagraphDirection")result=(target->rows.size()>1&&i==1)?-1:1;
        else throw std::runtime_error("Unexpected Android editor integer method");
    }
    va_end(args);return result;
}
jfloat JNIEnv::CallFloatMethod(jobject target,jmethodID method,...){
    auto name=std::string_view(method->name);va_list args;va_start(args,method);const auto i=va_arg(args,int);va_end(args);
    // Android's RTL lineLeft excludes trailing whitespace.
    if(name=="getLineLeft")return row(target,i).left+((target->rows.size()>1&&i==1)?10.f:0.f);
    if(name=="getLineRight")return row(target,i).left+row(target,i).width;
    if(name=="getLineWidth")return row(target,i).width;
    if(name=="getPrimaryHorizontal"){
        if(target->rows.size()>1){if(i==4)return 35;if(i==5)return 25;if(i==6)return 15;if(i==7)return 0;if(i>7)return float((i-7)*10);}
        return float(i*10);
    }
    throw std::runtime_error("Unexpected Android editor float method");
}
jboolean JNIEnv::CallBooleanMethod(jobject target,jmethodID method,...){
    auto& s=fake_state();auto name=std::string_view(method->name);va_list args;va_start(args,method);jboolean result=JNI_TRUE;
    if(name=="isRtlCharAt"){auto pos=va_arg(args,int);result=pos>=4&&pos<7?JNI_TRUE:JNI_FALSE;}
    else if(name=="clipRect"){for(int i=0;i<4;++i)s.clips.push_back(float(va_arg(args,double)));}
    else if(name=="setPath"){
        auto path=va_arg(args,jobject),clip=va_arg(args,jobject);target->rects.clear();const auto c=clip->rects.at(0);
        for(const auto& p:path->rects){FakeRect r{std::max(p.left,c.left),std::max(p.top,c.top),std::min(p.right,c.right),std::min(p.bottom,c.bottom)};
            if(r.right>r.left&&r.bottom>r.top)target->rects.push_back(r);}
        result=target->rects.empty()?JNI_FALSE:JNI_TRUE;
    }else if(name=="next"){
        auto rect=va_arg(args,jobject);if(std::size_t(target->index)>=target->source->rects.size())result=JNI_FALSE;
        else {rect->rects={target->source->rects[std::size_t(target->index++)]};result=JNI_TRUE;}
    }else throw std::runtime_error("Unexpected Android editor boolean method");
    va_end(args);return result;
}
int AndroidBitmap_getInfo(JNIEnv*,jobject,AndroidBitmapInfo* info){auto& s=fake_state();*info={std::uint32_t(s.width),std::uint32_t(s.height),std::uint32_t(s.stride),ANDROID_BITMAP_FORMAT_RGBA_8888,0};return ANDROID_BITMAP_RESULT_SUCCESS;}
int AndroidBitmap_lockPixels(JNIEnv*,jobject,void** value){*value=fake_state().null_pixels?nullptr:fake_state().pixels.data();return ANDROID_BITMAP_RESULT_SUCCESS;}
int AndroidBitmap_unlockPixels(JNIEnv*,jobject){++fake_state().unlocks;return ANDROID_BITMAP_RESULT_SUCCESS;}

int main(){
    try{
        auto& s=fake_state();
        {
            auto source=take(TextIndex::create("abc אבג xyz"));auto line=take(TextLine::create(source,{.font_size=13,.line_height=30}));
            check(s.shaped_text==u"abc אבג xyz"&&s.font_kind=="asset"&&s.font_path==u"fonts/Pretendard-Regular.ttf","Android editor lost Unicode or the selected font");
            auto m=take(line.metrics());check(m.width==110&&m.height==30&&m.baseline==20,"Android editor metrics differ from the native layout");
            auto selected=take(line.selection(2,5));check(selected.size()==2&&selected[0].x==20&&selected[1].x==50,"Android editor collapsed bidirectional selection");
            auto limited=line.wrap(35,2);check(!limited&&limited.error().code==core::ErrorCode::full,"Android editor ignored its line budget");
            auto fragments=take(line.wrap(35,4));check(fragments.size()==3&&fragments[1].first==4&&fragments[1].last==7,"Android wrapped paragraph lost its ranges");
            auto& rtl=fragments[1].line;check(take(rtl.caret(0))==30&&take(rtl.caret(3))==0,"Android wrapped RTL caret used the next line");
            auto spans=take(rtl.selection(0,3));check(spans.size()==1&&spans[0].x==0&&spans[0].width==30,"Android wrapped selection lost its visual line origin");
            check(take(rtl.hit_test(20))==1,"Android shaped hit test lost its local position");
            auto bitmap=take(rtl.render(7,12,{1,0,0,1},2));
            check(bitmap.width==24&&bitmap.height==60&&bitmap.rgba[0]==20&&bitmap.rgba[1]==40&&bitmap.rgba[2]==60&&bitmap.rgba[3]==128,
                  "Android editor bitmap lost viewport size or straight alpha");
            check(s.draws==1&&s.clips.size()==8&&s.translations.size()==2&&s.translations[0]==-12&&s.translations[1]==-16,
                  "Android editor did not clip and position the retained visual row");
            s.null_pixels=true;auto bad=rtl.render(0,12,{1,1,1,1});check(!bad&&s.unlocks==2,"Android editor failed to unlock a null bitmap");s.null_pixels=false;
            line=TextLine{};check(take(rtl.caret(0))==30,"Wrapped Android line did not retain its parent layout");
        }
        check(s.global_refs==0,"Android editor leaked Java global references");
        {
            auto source=take(TextIndex::create("A한😀Z"));auto line=take(TextLine::create(source));
            check(s.shaped_text==u"A한😀Z"&&!line.caret(3),"Android editor split or corrupted a UTF-16 surrogate pair");
        }
        {
            auto source=take(TextIndex::create("a\tb"));auto line=take(TextLine::create(source,{.tab_stop_distance=100}));
            check(!s.tabs.empty()&&s.tabs.front()==100&&std::ranges::is_sorted(s.tabs),"Android explicit tab stops were omitted");
        }
        {
            auto source=take(TextIndex::create("wide"));auto line=take(TextLine::create(source));
            auto fragments=take(line.wrap(1,8));
            check(fragments.size()==4&&fragments.back().last==4,
                  "Android ellipsized a glyph wider than the wrapping viewport");
        }
        {
            auto source=take(TextIndex::create(std::string(1000,'\t')));
            auto denied=TextLine::create(source,{.tab_stop_distance=1});
            check(!denied&&denied.error().code==core::ErrorCode::full,
                  "Android accepted a paragraph beyond the tab-stop budget");
        }
        check(s.global_refs==0,"Android editor leaked font or source references");
        std::cout<<"Android text editor bridge: Unicode, bidi, wrap, viewport and lifetime passed with a fake OS\n";
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
