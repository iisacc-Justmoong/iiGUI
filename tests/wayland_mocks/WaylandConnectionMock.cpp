#include <iiGUI/WaylandConnection.h>
#include <iiGUI/NativeDesktop_p.hpp>
#include <fcntl.h>
#include <unistd.h>
#include <algorithm>
#include <functional>
#include <map>
#include <mutex>
#include <stdexcept>
#include <vector>

namespace {
std::mutex wake_mutex;
std::map<int,int> wake_writers;
bool reject_open=false;
bool reject_dispatch=false;
bool output_available=true;
int wait_count=0;
std::function<void()> next_wait_action;
unsigned close_epoch=0;
std::vector<ii::gui::detail::WaylandConnection*> connections;
ii::gui::FileDialogCompletion pending_dialog;
struct MockTextState final {
    std::uint64_t claim=0,epoch=0;std::uint32_t serial=0;unsigned disabled=0;
    bool focused=false,enabled=false;
    std::vector<std::uint64_t> claims;
    std::vector<ii::gui::detail::WaylandImePublication> publications;
};
std::map<const ii::gui::detail::WaylandConnection*,MockTextState> text_states;
std::map<const ii::gui::detail::WaylandConnection*,ii::gui::detail::WaylandCallbacks> callback_states;
std::map<const ii::gui::detail::WaylandConnection*,double> display_scales;
}
void fake_wayland_set_scale(double scale){
    for(auto* connection:connections){display_scales[connection]=scale;auto& callbacks=callback_states.at(connection);
        if(callbacks.configure)callbacks.configure(connection->size(),scale);}
}
void fake_wayland_output_available(bool available){output_available=available;}
void fake_wayland_emit_ordered_input(){
    using namespace ii::gui;using namespace ii::gui::detail;
    for(auto* connection:connections){auto& callbacks=callback_states.at(connection);
        if(callbacks.input)callbacks.input({.kind=WidgetInputKind::key_down,.text="1"});
        if(callbacks.ime)callbacks.ime({.kind=WaylandImeEventKind::enter,.epoch=1});
        if(callbacks.input)callbacks.input({.kind=WidgetInputKind::key_down,.text="3"});
        if(callbacks.ime)callbacks.ime({.kind=WaylandImeEventKind::done,.epoch=1,.serial=1});
    }
}
void fake_wayland_text_enter(ii::gui::detail::WaylandConnection& connection){auto& state=text_states.at(&connection);state.focused=true;state.enabled=false;++state.epoch;}
std::uint32_t fake_wayland_text_serial(const ii::gui::detail::WaylandConnection& connection){return text_states.at(&connection).serial;}
std::uint64_t fake_wayland_text_epoch(const ii::gui::detail::WaylandConnection& connection){return text_states.at(&connection).epoch;}
unsigned fake_wayland_text_disabled(const ii::gui::detail::WaylandConnection& connection){return text_states.at(&connection).disabled;}
std::vector<ii::gui::detail::WaylandImePublication> fake_wayland_text_publications(const ii::gui::detail::WaylandConnection& connection){return text_states.at(&connection).publications;}
extern "C" int fake_wayland_eventfd(unsigned int, int) {
    int fds[2]; if(pipe(fds)) return -1;
    for(auto fd:fds){fcntl(fd,F_SETFL,fcntl(fd,F_GETFL)|O_NONBLOCK);fcntl(fd,F_SETFD,FD_CLOEXEC);}
    std::lock_guard lock(wake_mutex);wake_writers[fds[0]]=fds[1];return fds[0];
}
extern "C" ssize_t fake_wayland_read(int fd,void* data,size_t size){return ::read(fd,data,size);}
extern "C" ssize_t fake_wayland_write(int fd,const void* data,size_t size){
    int writer=fd;{std::lock_guard lock(wake_mutex);if(auto found=wake_writers.find(fd);found!=wake_writers.end())writer=found->second;}
    return ::write(writer,data,size);
}
void fake_wayland_clear_wakes(){std::lock_guard lock(wake_mutex);for(auto [reader,writer]:wake_writers){(void)reader;::close(writer);}wake_writers.clear();}
void fake_wayland_reject_next_open(){reject_open=true;}
void fake_wayland_reject_next_dispatch(){reject_dispatch=true;}
void fake_wayland_reset_waits(){wait_count=0;close_epoch=0;next_wait_action={};}
void fake_wayland_on_next_wait(std::function<void()> action){next_wait_action=std::move(action);}
void fake_wayland_request_all_close(){++close_epoch;}
void fake_wayland_complete_dialog(){auto completion=std::move(pending_dialog);if(completion)completion(ii::gui::FileDialogSelection{.canceled=true});}

namespace ii::gui::detail {
struct WaylandConnection::Impl final { HostOptions options;WaylandCallbacks callbacks;bool ready=false;unsigned close_seen=0; };
WaylandConnection::WaylandConnection():impl_(std::make_unique<Impl>()){text_states.emplace(this,MockTextState{});display_scales.emplace(this,1.); }
WaylandConnection::~WaylandConnection(){std::erase(connections,this);text_states.erase(this);callback_states.erase(this);display_scales.erase(this);}
void WaylandConnection::open(const HostOptions& options,WaylandCallbacks callbacks){
    if(std::exchange(reject_open,false))throw std::runtime_error("mock open failed");
    impl_->options=options;impl_->callbacks=std::move(callbacks);callback_states[this]=impl_->callbacks;connections.push_back(this);
}
void WaylandConnection::dispatch(){
    if(std::exchange(reject_dispatch,false))throw std::runtime_error("mock dispatch failed");
    if(!impl_->ready){
        impl_->ready=true;
        if(impl_->callbacks.activity)impl_->callbacks.activity(true);
        if(impl_->callbacks.configure)impl_->callbacks.configure(impl_->options.size,1);
    }
    if(close_epoch!=impl_->close_seen){impl_->close_seen=close_epoch;if(impl_->callbacks.close)impl_->callbacks.close();}
}
void WaylandConnection::wait(int,int){dispatch();}
void WaylandConnection::wait_many(std::span<WaylandConnection* const> connections,int,int){
    if(++wait_count>10000)throw std::runtime_error("mock host failed to stop");
    auto action=std::move(next_wait_action);if(action)action();
    for(auto* connection:connections)if(connection)connection->dispatch();
}
void WaylandConnection::title(std::string_view){}
void WaylandConnection::minimum(Size){}
void WaylandConnection::resize(Size){}
void WaylandConnection::action(WindowAction){}
void WaylandConnection::request_frame(){}
void WaylandConnection::cancel_frame(){}
wl_display* WaylandConnection::display()const{return nullptr;}
wl_surface* WaylandConnection::surface()const{return nullptr;}
bool WaylandConnection::configured()const{return impl_->ready;}
Size WaylandConnection::size()const{return impl_->options.size;}
double WaylandConnection::scale()const{return display_scales.at(this);}
std::optional<DisplayPixels> WaylandConnection::display_pixels()const{return output_available?std::optional(DisplayPixels{1920,1080}):std::nullopt;}
core::Result<ClipboardContent> WaylandConnection::clipboard(){return ClipboardContent{};}
core::Result<void> WaylandConnection::clipboard(const ClipboardContent&){return {};}
std::uint64_t WaylandConnection::claim_text_input(){auto& state=text_states.at(this);const auto next=++state.claim;state.claims.push_back(next);if(state.focused&&state.enabled){state.enabled=false;++state.disabled;++state.serial;}if(state.focused)++state.epoch;return next;}
void WaylandConnection::release_text_input(std::uint64_t claim){auto& state=text_states.at(this);auto found=std::find(state.claims.begin(),state.claims.end(),claim);if(found==state.claims.end())return;const bool active=found+1==state.claims.end();if(active&&state.focused&&state.enabled){state.enabled=false;++state.disabled;++state.serial;}state.claims.erase(found);if(active&&state.focused)++state.epoch;}
std::optional<std::uint64_t> WaylandConnection::text_input_focus(std::uint64_t claim)const{const auto& state=text_states.at(this);if(state.claims.empty()||state.claims.back()!=claim||!state.focused)return {};return state.epoch;}
std::optional<std::uint32_t> WaylandConnection::text_input_publish(std::uint64_t claim,std::uint64_t epoch,const WaylandImePublication& value){
    auto& state=text_states.at(this);if(state.claims.empty()||state.claims.back()!=claim||!state.focused||state.epoch!=epoch)return {};
    state.publications.push_back(value);state.enabled=true;return ++state.serial;
}
std::optional<std::uint32_t> WaylandConnection::text_input_disable(std::uint64_t claim,std::uint64_t epoch){
    auto& state=text_states.at(this);if(state.claims.empty()||state.claims.back()!=claim||!state.focused||state.epoch!=epoch||!state.enabled)return {};
    state.enabled=false;++state.disabled;return ++state.serial;
}
WaylandConnection* wayland_connection()noexcept{return nullptr;}
core::Result<std::shared_ptr<NativeDialog>> portal_file_dialog(std::string,FileDialogOptions,UiExecutor,FileDialogCompletion completion){
    pending_dialog=std::move(completion);return std::make_shared<NativeDialog>();
}
}
