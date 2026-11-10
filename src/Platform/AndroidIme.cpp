#include "AndroidIme.h"
#include "AndroidImeDispatch.h"
#include "AndroidImeProtocol.h"
#include "AndroidImePublication.h"
#include "AndroidJni.h"
#include <iiSDKCore/Core/Text/Text.h>
#include <android/log.h>
#include <array>
#include <atomic>
#include <limits>
#include <mutex>
#include <unordered_map>

namespace ii::gui::detail {
struct AndroidImeBridgeState final {
    AndroidImeHost host;
    AndroidImeProtocol protocol;
    AndroidImeDispatcher dispatch;
    jlong token=0;
    jclass java_class=nullptr;
    jmethodID update=nullptr,detach=nullptr;
    bool closed=false,updating=false,dirty=false,region_request=false;
    std::optional<AndroidImePublication> published;
    std::optional<TextInputRevision> published_revision;
    AndroidImeBridgeState(AndroidImeHost value,std::function<bool(runtime::Task)> post)
        :host(std::move(value)),dispatch(std::move(post)){}
};
namespace {
std::mutex registry_mutex;
std::unordered_map<jlong,std::weak_ptr<AndroidImeBridgeState>> registry;
std::atomic<std::uint64_t> next_token{1};

std::uint64_t allocate_token(){
    auto token=next_token.load(std::memory_order_relaxed);
    for(;;){
        if(token==0)throw std::runtime_error("Android IME token exhausted");
        const auto next=token==std::uint64_t((std::numeric_limits<jlong>::max)())?0:token+1;
        if(next_token.compare_exchange_weak(token,next,std::memory_order_relaxed))return token;
    }
}

std::shared_ptr<AndroidImeBridgeState> lookup(jlong token){
    if(token<=0)return {};
    std::lock_guard lock(registry_mutex);
    const auto it=registry.find(token);return it==registry.end()?nullptr:it->second.lock();
}

jstring JNICALL read_text(JNIEnv* env,jclass,jlong token,jlong generation,jint query,jint count) noexcept {
    try{
        auto state=lookup(token);if(!state||generation<=0||query<0||query>2||count<0)return nullptr;
        auto result=state->dispatch.call<std::optional<std::u16string>>([state,generation,query,count]{
            return state->protocol.query_text(state->host,std::uint64_t(generation),AndroidImeQuery(query),count);
        });
        if(!result||!(*result))return nullptr;
        const auto& text=**result;
        return env->NewString(reinterpret_cast<const jchar*>(text.data()),jsize(text.size()));
    }catch(...){if(env->ExceptionCheck())env->ExceptionClear();return nullptr;}
}

jboolean JNICALL command(JNIEnv* env,jclass,jlong token,jlong generation,jint operation,jstring input,jint first,jint second) noexcept {
    try{
        auto state=lookup(token);if(!state||generation<=0||operation<1||operation>7)return JNI_FALSE;
        std::string text;
        if(operation==2||operation==3){
            if(!input)return JNI_FALSE;
            const auto size=env->GetStringLength(input);if(size<0||size>65536)return JNI_FALSE;
            const auto* chars=env->GetStringChars(input,nullptr);if(!chars){if(env->ExceptionCheck())env->ExceptionClear();return JNI_FALSE;}
            const auto converted=core::to_utf8(std::u16string_view(reinterpret_cast<const char16_t*>(chars),std::size_t(size)));
            env->ReleaseStringChars(input,chars);
            if(!converted||converted->size()>65536)return JNI_FALSE;text=*converted;
        }
        auto result=state->dispatch.call<void>([state,generation,operation,text=std::move(text),first,second]() mutable ->core::Result<void>{
            const auto gen=std::uint64_t(generation);
            switch(operation){
                case 1:return state->protocol.select(state->host,gen,first,second);
                case 2:return state->protocol.composition(state->host,gen,std::move(text),first);
                case 3:return state->protocol.commit(state->host,gen,std::move(text),first);
                case 4:return state->protocol.finish(state->host,gen);
                case 5:return state->protocol.erase_surrounding(state->host,gen,first,second);
                case 6:return state->protocol.erase_surrounding_codepoints(state->host,gen,first,second);
                case 7:{
                    struct RegionGuard final {bool& flag;bool prior;explicit RegionGuard(bool& value):flag(value),prior(value){flag=true;}~RegionGuard(){flag=prior;}} guard{state->region_request};
                    return state->protocol.composing_region(state->host,gen,first,second);
                }
                default:return core::fail(core::ErrorCode::unsupported,"Unknown Android text command");
            }
        });
        return result?JNI_TRUE:JNI_FALSE;
    }catch(...){if(env->ExceptionCheck())env->ExceptionClear();return JNI_FALSE;}
}

void register_bridge(const std::shared_ptr<AndroidImeBridgeState>& state){
    AndroidJni jni;
    auto context=jni.type("android/content/Context");
    auto loader=jni.env->CallObjectMethod(jni.activity,jni.method(context,"getClassLoader","()Ljava/lang/ClassLoader;"));jni.check();
    if(!loader)throw std::runtime_error("Android application class loader is unavailable");
    auto loader_type=jni.type("java/lang/ClassLoader");
    auto name=jni.string("com.iisacc.iigui.AndroidImeBridge");
    auto local=static_cast<jclass>(jni.env->CallObjectMethod(loader,jni.method(loader_type,"loadClass","(Ljava/lang/String;)Ljava/lang/Class;"),name));jni.check();
    if(!local)throw std::runtime_error("Android IME bridge class is unavailable");
    JNINativeMethod methods[]={
        {const_cast<char*>("nativeRead"),const_cast<char*>("(JJII)Ljava/lang/String;"),reinterpret_cast<void*>(&read_text)},
        {const_cast<char*>("nativeCommand"),const_cast<char*>("(JJILjava/lang/String;II)Z"),reinterpret_cast<void*>(&command)}
    };
    if(jni.env->RegisterNatives(local,methods,2)!=JNI_OK){jni.check();throw std::runtime_error("Android IME native methods could not be registered");}
    auto attach=jni.static_method(local,"attach","(Landroid/app/Activity;J)V");
    state->update=jni.static_method(local,"update","(Landroid/app/Activity;JJIIIIIIZ)V");
    state->detach=jni.static_method(local,"detach","(Landroid/app/Activity;J)V");
    state->java_class=static_cast<jclass>(jni.env->NewGlobalRef(local));jni.check();
    if(!state->java_class)throw std::runtime_error("Android IME bridge reference is unavailable");
    const auto token=allocate_token();
    state->token=jlong(token);
    {std::lock_guard lock(registry_mutex);registry.emplace(state->token,state);}
    jni.env->CallStaticVoidMethod(state->java_class,attach,jni.activity,state->token);jni.check();
}

AndroidImePublication publication(const core::Result<AndroidImeState>& current){
    if(!current)return {};
    const auto& v=*current;
    constexpr auto max=std::size_t((std::numeric_limits<jint>::max)());
    if(v.length>max)return {};
    const auto begin=v.selection.location,end=begin+v.selection.length;
    const auto anchor=v.selection_reversed?end:begin,caret=v.selection_reversed?begin:end;
    const jlong marked_start=v.marked?jlong(v.marked->location):-1;
    const jlong marked_end=v.marked?jlong(v.marked->location+v.marked->length):-1;
    const int flags=(v.read_only?1:0)|(v.multiline?2:0)|(v.password?4:0);
    return {std::int64_t(v.generation),std::int64_t(v.length),std::int64_t(anchor),std::int64_t(caret),marked_start,marked_end,flags};
}
}

std::unique_ptr<AndroidIme> AndroidIme::create(AndroidImeHost host,std::function<bool(runtime::Task)> post){
    auto state=std::make_shared<AndroidImeBridgeState>(std::move(host),std::move(post));
    auto result=std::unique_ptr<AndroidIme>(new AndroidIme(state));
    try{register_bridge(state);}catch(...){
        result->close();
        __android_log_write(ANDROID_LOG_WARN,"iiGUI","Android IME bridge unavailable; include AndroidImeBridge.java in the application");
        return {};
    }
    return result;
}
AndroidIme::~AndroidIme(){close();}
void AndroidIme::changed() noexcept {
    auto state=state_;if(!state||state->closed||!state->java_class)return;
    state->dirty=true;if(state->updating)return;state->updating=true;
    try{
        // A provider may synchronously notify while its snapshot is collected.
        // Bound the retry; a later surface flush retries a continuously changing view.
        for(int attempt=0;state->dirty&&attempt<4&&!state->closed;++attempt){
            state->dirty=false;auto current=state->protocol.state(state->host);if(state->closed)break;
            auto values=publication(current);const auto revision=current?current->revision:std::nullopt;
            if(!android_ime_publish_required(state->published,values,state->published_revision,revision,state->region_request)){
                state->published_revision=revision;continue;
            }
            const auto notify=android_ime_notify_selection(state->published,values,state->region_request);
            AndroidJni jni;
            jni.env->CallStaticVoidMethod(state->java_class,state->update,jni.activity,state->token,jlong(values[0]),
                jint(values[1]),jint(values[2]),jint(values[3]),jint(values[4]),jint(values[5]),jint(values[6]),jboolean(notify));
            jni.check();state->published=values;state->published_revision=revision;
        }
    }catch(...){__android_log_write(ANDROID_LOG_WARN,"iiGUI","Android IME state could not be published");}
    state->updating=false;
}
void AndroidIme::close() noexcept {
    auto state=std::exchange(state_,{});if(!state||state->closed)return;
    state->closed=true;state->dispatch.close();
    if(state->token){std::lock_guard lock(registry_mutex);registry.erase(state->token);}
    if(state->java_class){
        try{AndroidJni jni;if(state->detach&&state->token){jni.env->CallStaticVoidMethod(state->java_class,state->detach,jni.activity,state->token);if(jni.env->ExceptionCheck())jni.env->ExceptionClear();}
            jni.env->DeleteGlobalRef(state->java_class);
        }catch(...){}
        state->java_class=nullptr;
    }
}
} // namespace ii::gui::detail
