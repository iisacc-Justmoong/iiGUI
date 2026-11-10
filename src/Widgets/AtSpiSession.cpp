#include "AtSpiSession.h"
#include <gio/gio.h>
#include <atomic>
#include <chrono>
#include <thread>
#include <utility>

namespace ii::gui::detail {
namespace {
template<class T> std::shared_ptr<T> object(T* value){return {value,[](T* item){if(item)g_object_unref(item);}};}
struct Variant final {GVariant* value=nullptr;~Variant(){if(value)g_variant_unref(value);}};
struct Error final {GError* value=nullptr;~Error(){if(value)g_error_free(value);}};
struct ThreadContext final {
    GMainContext* value;explicit ThreadContext(GMainContext* context):value(context){g_main_context_push_thread_default(value);}
    ~ThreadContext(){g_main_context_pop_thread_default(value);}
};
std::optional<AtSpiConnection> connect_native(GCancellable* cancel){
    Error error;auto session=object(g_bus_get_sync(G_BUS_TYPE_SESSION,cancel,&error.value));if(!session||error.value)return std::nullopt;
    Variant address{g_dbus_connection_call_sync(session.get(),"org.a11y.Bus","/org/a11y/bus","org.a11y.Bus","GetAddress",
        nullptr,G_VARIANT_TYPE("(s)"),G_DBUS_CALL_FLAGS_NONE,3000,cancel,&error.value)};
    if(!address.value||error.value)return std::nullopt;
    const gchar* raw=nullptr;g_variant_get(address.value,"(&s)",&raw);if(!raw||!*raw)return std::nullopt;
    auto connection=object(g_dbus_connection_new_for_address_sync(raw,
        GDBusConnectionFlags(G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT|G_DBUS_CONNECTION_FLAGS_MESSAGE_BUS_CONNECTION),
        nullptr,cancel,&error.value));
    if(!connection||error.value)return std::nullopt;
    g_dbus_connection_set_exit_on_close(connection.get(),FALSE);
    const auto* name=g_dbus_connection_get_unique_name(connection.get());if(!name)return std::nullopt;
    return AtSpiConnection{std::move(connection),name};
}
std::optional<AtSpiReference> embed_native(const AtSpiConnection& connection,GCancellable* cancel){
    if(!connection.value||connection.name.empty())return std::nullopt;
    Error error;Variant reply{g_dbus_connection_call_sync(connection.value.get(),"org.a11y.atspi.Registry",
        "/org/a11y/atspi/accessible/root","org.a11y.atspi.Socket","Embed",
        g_variant_new("((so))",connection.name.c_str(),AtSpiApplication::root_path().data()),G_VARIANT_TYPE("((so))"),
        G_DBUS_CALL_FLAGS_NONE,3000,cancel,&error.value)};
    if(!reply.value||error.value)return std::nullopt;
    const gchar* bus=nullptr;const gchar* path=nullptr;g_variant_get(reply.value,"((&s&o))",&bus,&path);
    if(!bus||!*bus||!path||!g_variant_is_object_path(path))return std::nullopt;
    return AtSpiReference{bus,path};
}
} // namespace

struct AtSpiSession::Impl final {
    struct Flags final {std::atomic_bool done=false,posted=false;};
    struct Job final {
        std::shared_ptr<GCancellable> cancel=object(g_cancellable_new());
        std::shared_ptr<Flags> flags=std::make_shared<Flags>();std::thread thread;
        ~Job(){if(cancel)g_cancellable_cancel(cancel.get());if(thread.joinable())thread.join();}
    };
    enum class Phase {idle,connecting,embedding,connected,stopped};
    const std::thread::id owner=std::this_thread::get_id();
    std::shared_ptr<AtSpiCoordinator> coordinator;Post post;Connect connect;Embed embed;
    std::weak_ptr<AtSpiSession> self;GMainContext* context=g_main_context_new();
    std::unique_ptr<AtSpiDbusBridge> bridge;std::optional<AtSpiConnection> connection;
    std::unique_ptr<Job> job;std::uint64_t epoch=0;Phase phase=Phase::idle;
    std::chrono::steady_clock::time_point retry{};
    Impl(std::shared_ptr<AtSpiCoordinator> owner,Post dispatch,Connect make_connection,Embed register_app):
        coordinator(std::move(owner)),post(std::move(dispatch)),connect(std::move(make_connection)),embed(std::move(register_app)){}
    ~Impl(){stop();g_main_context_unref(context);}
    bool on_owner()const noexcept{return owner==std::this_thread::get_id();}
    void release_bridge()noexcept{
        bridge.reset();
        for(int count=0;count<64&&g_main_context_pending(context);++count)g_main_context_iteration(context,FALSE);
    }
    void disconnect()noexcept{
        if(job)job.reset();release_bridge();connection.reset();
        if(phase!=Phase::stopped){phase=Phase::idle;retry=std::chrono::steady_clock::now()+std::chrono::seconds(2);}
    }
    void stop()noexcept{
        if(phase==Phase::stopped)return;phase=Phase::stopped;++epoch;
        coordinator->on_changed({});
        if(job)job.reset();release_bridge();connection.reset();
    }
    template<class Result,class Operation,class Complete>
    void launch(Operation operation,Complete complete){
        const auto current=++epoch;auto candidate=std::make_unique<Job>();auto cancellation=candidate->cancel;auto flags=candidate->flags;
        auto weak=self;auto dispatch=post;
        candidate->thread=std::thread([weak,dispatch=std::move(dispatch),cancellation=std::move(cancellation),flags=std::move(flags),current,operation=std::move(operation),complete]()mutable{
            std::optional<Result> result;try{result=operation(cancellation.get());}catch(...){}
            bool posted=false;try{posted=dispatch([weak,current,result=std::move(result),complete]()mutable{
                if(auto owner=weak.lock()){auto& impl=*owner->impl_;if(impl.phase!=Phase::stopped&&impl.epoch==current)complete(impl,std::move(result));}
            });}catch(...){}
            flags->posted.store(posted,std::memory_order_relaxed);flags->done.store(true,std::memory_order_release);
        });
        job=std::move(candidate);
    }
    void begin_connect(){
        phase=Phase::connecting;
        launch<AtSpiConnection>(connect,[](Impl& state,std::optional<AtSpiConnection> value){state.connection_ready(std::move(value));});
    }
    void connection_ready(std::optional<AtSpiConnection> value){
        if(job)job.reset();
        if(!value||!value->value||value->name.empty()){disconnect();return;}
        connection=std::move(value);
        auto weak=std::weak_ptr<AtSpiCoordinator>(coordinator);
        try{
            ThreadContext scope(context);
            bridge=std::make_unique<AtSpiDbusBridge>(coordinator->model(),[weak](std::string path,WidgetInputKind kind){if(auto owner=weak.lock())return owner->request(std::move(path),kind);return false;},AtSpiDbusBridge::TextCallbacks{
                .read=[weak](std::string_view path)->std::optional<AtSpiTextData>{if(auto owner=weak.lock())return owner->text(path);return std::nullopt;},
                .input=[weak](std::string path,WidgetInput input){if(auto owner=weak.lock())return owner->request_text(std::move(path),std::move(input));return false;}},
                [weak](std::string path,double value){if(auto owner=weak.lock())return owner->request_value(std::move(path),value);return false;});
            if(!bridge->attach(connection->value.get(),connection->name)){disconnect();return;}
        }catch(...){disconnect();return;}
        phase=Phase::embedding;auto value_copy=*connection;auto operation=embed;
        try{launch<AtSpiReference>([operation=std::move(operation),value_copy=std::move(value_copy)](GCancellable* cancellation){return operation(value_copy,cancellation);},
            [](Impl& state,std::optional<AtSpiReference> parent){state.embed_ready(std::move(parent));});}
        catch(...){disconnect();}
    }
    void embed_ready(std::optional<AtSpiReference> parent){
        if(job)job.reset();
        if(!parent||parent->bus.empty()||!g_variant_is_object_path(parent->path.c_str())||!bridge){disconnect();return;}
        bridge->registry_parent(std::move(parent->bus),std::move(parent->path));phase=Phase::connected;
    }
    void pump(){
        if(!on_owner()||phase==Phase::stopped)return;
        for(int count=0;count<64&&g_main_context_pending(context);++count)g_main_context_iteration(context,FALSE);
        if(connection&&g_dbus_connection_is_closed(connection->value.get())){++epoch;disconnect();}
        if(job&&job->flags->done.load(std::memory_order_acquire)&&!job->flags->posted.load(std::memory_order_relaxed)){++epoch;disconnect();}
        if(phase==Phase::idle&&std::chrono::steady_clock::now()>=retry)begin_connect();
    }
};

AtSpiSession::AtSpiSession(std::unique_ptr<Impl> value):impl_(std::move(value)){}
std::shared_ptr<AtSpiSession> AtSpiSession::create(std::shared_ptr<AtSpiCoordinator> coordinator,Post post,Connect connect,Embed embed){
    if(!coordinator||!post)return {};
    if(!connect)connect=connect_native;if(!embed)embed=embed_native;
    auto result=std::shared_ptr<AtSpiSession>(new AtSpiSession(std::make_unique<Impl>(std::move(coordinator),std::move(post),std::move(connect),std::move(embed))));
    result->impl_->self=result;result->start();return result;
}
AtSpiSession::~AtSpiSession()=default;
void AtSpiSession::start(){
    auto weak=weak_from_this();impl_->coordinator->on_changed([weak](std::string){if(auto owner=weak.lock())if(owner->impl_->bridge)owner->impl_->bridge->changed();});
    try{impl_->begin_connect();}catch(...){impl_->disconnect();}
}
void AtSpiSession::pump()noexcept{try{impl_->pump();}catch(...){impl_->disconnect();}}
void AtSpiSession::stop()noexcept{if(impl_->on_owner())impl_->stop();}
bool AtSpiSession::connected()const noexcept{return impl_->on_owner()&&impl_->phase==Impl::Phase::connected;}
} // namespace ii::gui::detail
