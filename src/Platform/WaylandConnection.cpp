#include <iiGUI/WaylandConnection.h>
#include <iiGUI/WaylandPolicy.h>
#include <iiGUI/DesktopCommon.h>
#include <iiGUI/PipeWrite.h>
#include "xdg-shell-client-protocol.h"
#include <wayland-client.h>
#include <wayland-cursor.h>
#include <xkbcommon/xkbcommon.h>
#include <sys/mman.h>
#include <poll.h>
#include <unistd.h>
#include <fcntl.h>
#include <linux/input-event-codes.h>
#include <cerrno>
#include <cstring>
#include <map>
#include <set>
#include <thread>

namespace ii::gui::detail {
namespace {
thread_local WaylandConnection* active=nullptr;
WidgetKey key(xkb_keysym_t value){switch(value){case XKB_KEY_Tab:case XKB_KEY_ISO_Left_Tab:return WidgetKey::tab;case XKB_KEY_Return:case XKB_KEY_KP_Enter:return WidgetKey::enter;case XKB_KEY_Escape:return WidgetKey::escape;case XKB_KEY_space:return WidgetKey::space;case XKB_KEY_Left:return WidgetKey::left;case XKB_KEY_Right:return WidgetKey::right;case XKB_KEY_Up:return WidgetKey::up;case XKB_KEY_Down:return WidgetKey::down;case XKB_KEY_Home:return WidgetKey::home;case XKB_KEY_End:return WidgetKey::end;case XKB_KEY_BackSpace:return WidgetKey::backspace;case XKB_KEY_Delete:return WidgetKey::delete_forward;case XKB_KEY_Page_Up:return WidgetKey::page_up;case XKB_KEY_Page_Down:return WidgetKey::page_down;default:return WidgetKey::character;}}
void nonblocking(int fd){const auto flags=fcntl(fd,F_GETFL);if(flags<0||fcntl(fd,F_SETFL,flags|O_NONBLOCK)<0)throw std::runtime_error("Cannot make Wayland transfer nonblocking");}
}
struct WaylandConnection::Impl final {
    wl_display* display=nullptr;wl_registry* registry=nullptr;wl_compositor* compositor=nullptr;
    xdg_wm_base* shell=nullptr;wl_surface* surface=nullptr;xdg_surface* role=nullptr;xdg_toplevel* top=nullptr;
    wl_seat* seat=nullptr;wl_pointer* pointer=nullptr;wl_keyboard* keyboard=nullptr;
    wl_shm* shm=nullptr;wl_cursor_theme* cursor_theme=nullptr;wl_surface* cursor_surface=nullptr;
    int cursor_scale=0;std::uint32_t pointer_serial=0;
    wl_data_device_manager* manager=nullptr;wl_data_device* device=nullptr;wl_callback* frame=nullptr;
    xkb_context* xkb=nullptr;xkb_keymap* keymap=nullptr;xkb_state* keys=nullptr;
    struct Output {Impl* owner;wl_output* native;int scale=1;};
    std::map<std::uint32_t,std::unique_ptr<Output>> outputs;std::set<wl_output*> entered;
    struct Offer {wl_data_offer* native=nullptr;std::set<std::string> types;~Offer(){if(native)wl_data_offer_destroy(native);}};
    std::map<wl_data_offer*,std::unique_ptr<Offer>> offers;Offer* selected=nullptr;
    struct Source {Impl* owner;wl_data_source* native=nullptr;ClipboardContent content;~Source(){if(native)wl_data_source_destroy(native);}};
    std::vector<std::unique_ptr<Source>> sources;
    struct Send {int fd;std::shared_ptr<const std::string> bytes;std::size_t offset=0;runtime::Clock::time_point expires=runtime::Clock::now()+std::chrono::seconds(2);};
    std::vector<Send> sends;
    WaylandCallbacks callbacks;Size extent{640,420};int scale=1;std::uint32_t serial=0;
    double x=0,y=0;unsigned buttons=0;bool configured=false,fullscreen=false;std::optional<std::string> failure;
    int repeat_rate=0,repeat_delay=0;std::optional<std::uint32_t> repeat_key;runtime::Clock::time_point repeat_at;
    const std::thread::id owner=std::this_thread::get_id();
    void require()const{if(owner!=std::this_thread::get_id())throw std::logic_error("Wayland access requires its owner thread");}
    template<class F>void guard(F&& work)noexcept{try{work();}catch(const std::exception& e){failure=e.what();}catch(...){failure="Wayland callback failed";}}
    ~Impl(){
        cancel_frame();for(const auto& send:sends)::close(send.fd);sources.clear();offers.clear();
        if(device)wl_data_device_release(device);if(manager)wl_data_device_manager_destroy(manager);
        if(pointer)wl_pointer_release(pointer);if(keyboard)wl_keyboard_release(keyboard);if(seat)wl_seat_release(seat);
        if(cursor_surface)wl_surface_destroy(cursor_surface);if(cursor_theme)wl_cursor_theme_destroy(cursor_theme);if(shm)wl_shm_destroy(shm);
        if(keys)xkb_state_unref(keys);if(keymap)xkb_keymap_unref(keymap);if(xkb)xkb_context_unref(xkb);
        if(top)xdg_toplevel_destroy(top);if(role)xdg_surface_destroy(role);if(surface)wl_surface_destroy(surface);
        for(auto& [id,out]:outputs)wl_output_destroy(out->native);
        if(shell)xdg_wm_base_destroy(shell);if(compositor)wl_compositor_destroy(compositor);if(registry)wl_registry_destroy(registry);
        if(display)wl_display_disconnect(display);
    }
    void emit(WidgetInput value){if(callbacks.input)callbacks.input(std::move(value));}
    WidgetInput input(WidgetInputKind kind,double time=0){WidgetInput value{.kind=kind};value.x=x;value.y=y;value.buttons=buttons;value.timestamp=time/1000.;value.device=PointerDevice::mouse;
        if(keys){value.shift=xkb_state_mod_name_is_active(keys,XKB_MOD_NAME_SHIFT,XKB_STATE_MODS_EFFECTIVE)>0;value.control=xkb_state_mod_name_is_active(keys,XKB_MOD_NAME_CTRL,XKB_STATE_MODS_EFFECTIVE)>0;value.alt=xkb_state_mod_name_is_active(keys,XKB_MOD_NAME_ALT,XKB_STATE_MODS_EFFECTIVE)>0;value.meta=xkb_state_mod_name_is_active(keys,XKB_MOD_NAME_LOGO,XKB_STATE_MODS_EFFECTIVE)>0;}return value;}
    void key_event(std::uint32_t code,bool down,double time){
        if(!keys)return;const auto symbol=xkb_state_key_get_one_sym(keys,code+8);
        auto value=input(down?WidgetInputKind::key_down:WidgetInputKind::key_up,time);value.key=key(symbol);
        char name[64]{};const auto name_count=xkb_keysym_to_utf8(symbol,name,sizeof(name));
        if(name_count>1&&name_count<=int(sizeof(name))&&static_cast<unsigned char>(name[0])>=32)value.text.assign(name,std::size_t(name_count-1));
        std::string text;
        if(down&&!value.control&&!value.alt&&!value.meta&&value.key!=WidgetKey::space){
            const auto count=xkb_state_key_get_utf8(keys,code+8,nullptr,0);
            if(count>0&&count<=64){text.resize(std::size_t(count)+1);xkb_state_key_get_utf8(keys,code+8,text.data(),text.size());text.resize(count);if(static_cast<unsigned char>(text.front())<32)text.clear();}
        }
        emit(std::move(value));if(!text.empty())emit({.kind=WidgetInputKind::text,.text=std::move(text)});
    }
    void cursor(){
        if(!pointer||!pointer_serial||!compositor||!shm)return;
        if(!cursor_theme||cursor_scale!=scale){if(cursor_theme)wl_cursor_theme_destroy(cursor_theme);cursor_theme=wl_cursor_theme_load(nullptr,24*scale,shm);cursor_scale=scale;}
        if(!cursor_theme)return;auto* cursor=wl_cursor_theme_get_cursor(cursor_theme,"left_ptr");if(!cursor||!cursor->image_count)return;
        auto* image=cursor->images[0];auto* buffer=wl_cursor_image_get_buffer(image);if(!buffer)return;
        if(!cursor_surface)cursor_surface=wl_compositor_create_surface(compositor);
        wl_surface_set_buffer_scale(cursor_surface,scale);wl_pointer_set_cursor(pointer,pointer_serial,cursor_surface,int(image->hotspot_x/scale),int(image->hotspot_y/scale));
        wl_surface_attach(cursor_surface,buffer,0,0);wl_surface_damage(cursor_surface,0,0,INT32_MAX,INT32_MAX);wl_surface_commit(cursor_surface);
    }
    void update_scale(){int next=1;for(const auto& [id,out]:outputs)if(entered.contains(out->native))next=std::max(next,out->scale);if(next==scale)return;scale=next;if(surface)wl_surface_set_buffer_scale(surface,scale);cursor();if(configured&&callbacks.configure)callbacks.configure(extent,scale);}
    void cancel_frame(){if(frame){wl_callback_destroy(frame);frame=nullptr;}}
    void request_frame(){if(frame||!surface||!configured)return;frame=wl_surface_frame(surface);static const wl_callback_listener listener{+[](void* raw,wl_callback* callback,std::uint32_t){auto& self=*static_cast<Impl*>(raw);wl_callback_destroy(callback);self.frame=nullptr;self.guard([&]{if(self.callbacks.frame)self.callbacks.frame();});}};wl_callback_add_listener(frame,&listener,this);wl_surface_commit(surface);}
    void bind_seat(){if(!seat)return;
        static const wl_seat_listener listener{.capabilities=+[](void* raw,wl_seat* seat,std::uint32_t capabilities){auto& self=*static_cast<Impl*>(raw);self.guard([&]{
            if((capabilities&WL_SEAT_CAPABILITY_POINTER)&&!self.pointer){self.pointer=wl_seat_get_pointer(seat);wl_pointer_add_listener(self.pointer,&pointer_listener,&self);}
            if(!(capabilities&WL_SEAT_CAPABILITY_POINTER)&&self.pointer){wl_pointer_release(self.pointer);self.pointer=nullptr;self.buttons=0;}
            if((capabilities&WL_SEAT_CAPABILITY_KEYBOARD)&&!self.keyboard){self.keyboard=wl_seat_get_keyboard(seat);wl_keyboard_add_listener(self.keyboard,&keyboard_listener,&self);}
            if(!(capabilities&WL_SEAT_CAPABILITY_KEYBOARD)&&self.keyboard){wl_keyboard_release(self.keyboard);self.keyboard=nullptr;self.repeat_key.reset();self.emit(self.input(WidgetInputKind::focus_lost));}
        });},.name=+[](void*,wl_seat*,const char*){}};
        wl_seat_add_listener(seat,&listener,this);
    }
    static const wl_pointer_listener pointer_listener;
    static const wl_keyboard_listener keyboard_listener;
    static const wl_data_device_listener device_listener;
    void bind_data(){if(!device&&manager&&seat){device=wl_data_device_manager_get_data_device(manager,seat);wl_data_device_add_listener(device,&device_listener,this);}}
    void open(const HostOptions& options,WaylandCallbacks value){
        require();if(options.title.size()>8192)throw std::invalid_argument("Wayland title exceeds protocol budget");callbacks=std::move(value);extent=options.size;display=wl_display_connect(nullptr);if(!display)throw std::runtime_error("Cannot connect to WAYLAND_DISPLAY");
        xkb=xkb_context_new(XKB_CONTEXT_NO_FLAGS);if(!xkb)throw std::runtime_error("Cannot create xkbcommon context");registry=wl_display_get_registry(display);
        static const wl_registry_listener globals{+[](void* raw,wl_registry* registry,std::uint32_t id,const char* interface,std::uint32_t version){auto& self=*static_cast<Impl*>(raw);self.guard([&]{
            if(!std::strcmp(interface,wl_compositor_interface.name)&&version>=3)self.compositor=static_cast<wl_compositor*>(wl_registry_bind(registry,id,&wl_compositor_interface,std::min(version,4u)));
            else if(!std::strcmp(interface,xdg_wm_base_interface.name)){self.shell=static_cast<xdg_wm_base*>(wl_registry_bind(registry,id,&xdg_wm_base_interface,std::min(version,3u)));static const xdg_wm_base_listener ping{+[](void*,xdg_wm_base* shell,std::uint32_t serial){xdg_wm_base_pong(shell,serial);}};xdg_wm_base_add_listener(self.shell,&ping,&self);}
            else if(!std::strcmp(interface,wl_seat_interface.name)&&!self.seat&&version>=5){self.seat=static_cast<wl_seat*>(wl_registry_bind(registry,id,&wl_seat_interface,std::min(version,7u)));self.bind_seat();self.bind_data();}
            else if(!std::strcmp(interface,wl_data_device_manager_interface.name)&&version>=3){self.manager=static_cast<wl_data_device_manager*>(wl_registry_bind(registry,id,&wl_data_device_manager_interface,3));self.bind_data();}
            else if(!std::strcmp(interface,wl_shm_interface.name)){self.shm=static_cast<wl_shm*>(wl_registry_bind(registry,id,&wl_shm_interface,1));static const wl_shm_listener listener{+[](void*,wl_shm*,std::uint32_t){}};wl_shm_add_listener(self.shm,&listener,&self);}
            else if(!std::strcmp(interface,wl_output_interface.name)&&version>=2){auto output=std::make_unique<Output>(Output{&self,static_cast<wl_output*>(wl_registry_bind(registry,id,&wl_output_interface,2))});static const wl_output_listener listener{.geometry=+[](void*,wl_output*,int,int,int,int,int,const char*,const char*,int){},.mode=+[](void*,wl_output*,std::uint32_t,int,int,int){},.done=+[](void* raw,wl_output*){auto& out=*static_cast<Output*>(raw);out.owner->guard([&]{out.owner->update_scale();});},.scale=+[](void* raw,wl_output*,int scale){auto& out=*static_cast<Output*>(raw);out.owner->guard([&]{out.scale=wayland_buffer_scale(scale);});}};wl_output_add_listener(output->native,&listener,output.get());self.outputs.emplace(id,std::move(output));}
        });},+[](void* raw,wl_registry*,std::uint32_t id){auto& self=*static_cast<Impl*>(raw);if(auto found=self.outputs.find(id);found!=self.outputs.end()){self.entered.erase(found->second->native);wl_output_destroy(found->second->native);self.outputs.erase(found);self.guard([&]{self.update_scale();});}}};
        wl_registry_add_listener(registry,&globals,this);
        if(wl_display_roundtrip(display)<0)throw std::runtime_error("Wayland registry negotiation failed");
        if(!compositor||!shell)throw std::runtime_error("Wayland compositor requires wl_compositor and xdg_wm_base");
        surface=wl_compositor_create_surface(compositor);static const wl_surface_listener surface_listener{.enter=+[](void* raw,wl_surface*,wl_output* output){auto& self=*static_cast<Impl*>(raw);self.entered.insert(output);self.guard([&]{self.update_scale();});},.leave=+[](void* raw,wl_surface*,wl_output* output){auto& self=*static_cast<Impl*>(raw);self.entered.erase(output);self.guard([&]{self.update_scale();});}};
        wl_surface_add_listener(surface,&surface_listener,this);role=xdg_wm_base_get_xdg_surface(shell,surface);top=xdg_surface_get_toplevel(role);
        static const xdg_surface_listener configure{+[](void* raw,xdg_surface* role,std::uint32_t serial){auto& self=*static_cast<Impl*>(raw);xdg_surface_ack_configure(role,serial);self.configured=true;self.guard([&]{if(self.callbacks.configure)self.callbacks.configure(self.extent,self.scale);});}};xdg_surface_add_listener(role,&configure,this);
        static const xdg_toplevel_listener top_listener{.configure=+[](void* raw,xdg_toplevel*,int width,int height,wl_array* states){auto& self=*static_cast<Impl*>(raw);self.guard([&]{self.extent=wayland_configure_size(self.extent,width,height);self.fullscreen=false;if(states->size%sizeof(std::uint32_t)||states->size>4096)throw std::runtime_error("Invalid xdg toplevel state list");const auto* values=static_cast<const std::uint32_t*>(states->data);for(std::size_t i=0;i<states->size/sizeof(std::uint32_t);++i)if(values[i]==XDG_TOPLEVEL_STATE_FULLSCREEN)self.fullscreen=true;});},.close=+[](void* raw,xdg_toplevel*){auto& self=*static_cast<Impl*>(raw);self.guard([&]{if(self.callbacks.close)self.callbacks.close();});},.configure_bounds=+[](void*,xdg_toplevel*,int,int){},.wm_capabilities=+[](void*,xdg_toplevel*,wl_array*){}};
        xdg_toplevel_add_listener(top,&top_listener,this);xdg_toplevel_set_title(top,options.title.c_str());xdg_toplevel_set_app_id(top,"com.iisacc.iiGUI");xdg_toplevel_set_min_size(top,int(options.minimum_size.width),int(options.minimum_size.height));wl_surface_commit(surface);dispatch();
    }
    void transfer(){for(auto it=sends.begin();it!=sends.end();){bool finished=runtime::Clock::now()>=it->expires;
        if(!finished){const auto count=pipe_write(it->fd,it->bytes->data()+it->offset,it->bytes->size()-it->offset);if(count>0)it->offset+=std::size_t(count);else if(count<0&&errno!=EINTR&&errno!=EAGAIN)finished=true;finished=finished||it->offset==it->bytes->size();}
        if(finished){::close(it->fd);it=sends.erase(it);}else ++it;}}
    void dispatch(){require();if(wl_display_dispatch_pending(display)<0)throw std::runtime_error("Wayland dispatch failed");transfer();if(repeat_key&&runtime::Clock::now()>=repeat_at){key_event(*repeat_key,true,0);repeat_at=runtime::Clock::now()+std::chrono::milliseconds(std::max(1,1000/repeat_rate));}if(failure)throw std::runtime_error(*failure);}
    void wait(int wake,int timeout){
        dispatch();while(wl_display_prepare_read(display)!=0)dispatch();
        const bool blocked=wl_display_flush(display)<0&&errno==EAGAIN;
        if(!blocked&&wl_display_get_error(display)){wl_display_cancel_read(display);throw std::runtime_error("Wayland connection failed");}
        if(repeat_key){const auto repeat=deadline_timeout(repeat_at);timeout=timeout<0?repeat:std::min(timeout,repeat);}
        if(!sends.empty())timeout=timeout<0?20:std::min(timeout,20);
        std::vector<pollfd> fds{{wl_display_get_fd(display),short(POLLIN|(blocked?POLLOUT:0)),0}};if(wake>=0)fds.push_back({wake,POLLIN,0});
        for(const auto& send:sends)fds.push_back({send.fd,POLLOUT,0});
        const auto ready=::poll(fds.data(),fds.size(),timeout);
        if(ready<0){wl_display_cancel_read(display);if(errno==EINTR)return;throw std::runtime_error("Wayland poll failed");}
        if(fds[0].revents&POLLIN){if(wl_display_read_events(display)<0)throw std::runtime_error("Wayland read failed");}else wl_display_cancel_read(display);
        if(fds[0].revents&(POLLERR|POLLHUP|POLLNVAL))throw std::runtime_error("Wayland compositor disconnected");dispatch();
    }
};
const wl_pointer_listener WaylandConnection::Impl::pointer_listener{
    .enter=+[](void* raw,wl_pointer*,std::uint32_t serial,wl_surface*,wl_fixed_t x,wl_fixed_t y){auto& self=*static_cast<Impl*>(raw);self.serial=serial;self.pointer_serial=serial;self.x=wl_fixed_to_double(x);self.y=wl_fixed_to_double(y);self.guard([&]{self.cursor();});},
    .leave=+[](void* raw,wl_pointer*,std::uint32_t,wl_surface*){auto& self=*static_cast<Impl*>(raw);self.pointer_serial=0;self.guard([&]{self.emit(self.input(WidgetInputKind::pointer_leave));});},
    .motion=+[](void* raw,wl_pointer*,std::uint32_t time,wl_fixed_t x,wl_fixed_t y){auto& self=*static_cast<Impl*>(raw);self.x=wl_fixed_to_double(x);self.y=wl_fixed_to_double(y);self.guard([&]{self.emit(self.input(WidgetInputKind::pointer_move,time));});},
    .button=+[](void* raw,wl_pointer*,std::uint32_t serial,std::uint32_t time,std::uint32_t button,std::uint32_t state){auto& self=*static_cast<Impl*>(raw);self.serial=serial;const auto mask=button==BTN_LEFT?1u:button==BTN_RIGHT?2u:button==BTN_MIDDLE?4u:0u;if(state)self.buttons|=mask;else self.buttons&=~mask;self.guard([&]{self.emit(self.input(state?WidgetInputKind::pointer_down:WidgetInputKind::pointer_up,time));});},
    .axis=+[](void* raw,wl_pointer*,std::uint32_t time,std::uint32_t axis,wl_fixed_t value){auto& self=*static_cast<Impl*>(raw);self.guard([&]{auto input=self.input(WidgetInputKind::scroll,time);if(axis==WL_POINTER_AXIS_VERTICAL_SCROLL)input.scroll_y=-wl_fixed_to_double(value);else input.scroll_x=-wl_fixed_to_double(value);self.emit(std::move(input));});},
    .frame=+[](void*,wl_pointer*){},.axis_source=+[](void*,wl_pointer*,std::uint32_t){},.axis_stop=+[](void*,wl_pointer*,std::uint32_t,std::uint32_t){},.axis_discrete=+[](void*,wl_pointer*,std::uint32_t,std::int32_t){}
};
const wl_keyboard_listener WaylandConnection::Impl::keyboard_listener{
    .keymap=+[](void* raw,wl_keyboard*,std::uint32_t format,int fd,std::uint32_t size){auto& self=*static_cast<Impl*>(raw);self.guard([&]{struct Close{int fd;~Close(){::close(fd);}}close{fd};if(format!=WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1||!size||size>4*1024*1024)throw std::runtime_error("Invalid Wayland XKB keymap");auto* data=static_cast<char*>(mmap(nullptr,size,PROT_READ,MAP_PRIVATE,fd,0));if(data==MAP_FAILED)throw std::runtime_error("Cannot map XKB keymap");struct Unmap{char* p;std::size_t n;~Unmap(){munmap(p,n);}}unmap{data,size};if(data[size-1]!='\0')throw std::runtime_error("XKB keymap lacks NUL terminator");auto* map=xkb_keymap_new_from_string(self.xkb,data,XKB_KEYMAP_FORMAT_TEXT_V1,XKB_KEYMAP_COMPILE_NO_FLAGS);if(!map)throw std::runtime_error("Cannot compile XKB keymap");auto* state=xkb_state_new(map);if(!state){xkb_keymap_unref(map);throw std::runtime_error("Cannot create XKB state");}if(self.keys)xkb_state_unref(self.keys);if(self.keymap)xkb_keymap_unref(self.keymap);self.keymap=map;self.keys=state;});},
    .enter=+[](void* raw,wl_keyboard*,std::uint32_t serial,wl_surface*,wl_array*){static_cast<Impl*>(raw)->serial=serial;},
    .leave=+[](void* raw,wl_keyboard*,std::uint32_t,wl_surface*){auto& self=*static_cast<Impl*>(raw);self.repeat_key.reset();self.guard([&]{self.emit(self.input(WidgetInputKind::focus_lost));});},
    .key=+[](void* raw,wl_keyboard*,std::uint32_t serial,std::uint32_t time,std::uint32_t code,std::uint32_t state){auto& self=*static_cast<Impl*>(raw);self.serial=serial;self.guard([&]{self.key_event(code,state==WL_KEYBOARD_KEY_STATE_PRESSED,time);if(state&&self.repeat_rate>0&&self.keymap&&xkb_keymap_key_repeats(self.keymap,code+8)){self.repeat_key=code;self.repeat_at=runtime::Clock::now()+std::chrono::milliseconds(self.repeat_delay);}else if(!state&&self.repeat_key==code)self.repeat_key.reset();});},
    .modifiers=+[](void* raw,wl_keyboard*,std::uint32_t serial,std::uint32_t depressed,std::uint32_t latched,std::uint32_t locked,std::uint32_t group){auto& self=*static_cast<Impl*>(raw);self.serial=serial;if(self.keys)xkb_state_update_mask(self.keys,depressed,latched,locked,0,0,group);},
    .repeat_info=+[](void* raw,wl_keyboard*,int rate,int delay){auto& self=*static_cast<Impl*>(raw);self.repeat_rate=std::clamp(rate,0,100);self.repeat_delay=std::clamp(delay,0,10000);if(!self.repeat_rate)self.repeat_key.reset();}
};
const wl_data_device_listener WaylandConnection::Impl::device_listener{
    .data_offer=+[](void* raw,wl_data_device*,wl_data_offer* native){auto& self=*static_cast<Impl*>(raw);self.guard([&]{if(self.offers.size()>=32)throw std::runtime_error("Too many Wayland offers");auto offer=std::make_unique<Offer>();offer->native=native;static const wl_data_offer_listener listener{.offer=+[](void* raw,wl_data_offer*,const char* mime){auto& offer=*static_cast<Offer*>(raw);if(offer.types.size()<128&&mime&&std::strlen(mime)<128)offer.types.insert(mime);},.source_actions=+[](void*,wl_data_offer*,std::uint32_t){},.action=+[](void*,wl_data_offer*,std::uint32_t){}};wl_data_offer_add_listener(native,&listener,offer.get());self.offers.emplace(native,std::move(offer));});},
    .enter=+[](void*,wl_data_device*,std::uint32_t,wl_surface*,wl_fixed_t,wl_fixed_t,wl_data_offer* offer){if(offer)wl_data_offer_accept(offer,0,nullptr);},
    .leave=+[](void*,wl_data_device*){},.motion=+[](void*,wl_data_device*,std::uint32_t,wl_fixed_t,wl_fixed_t){},.drop=+[](void*,wl_data_device*){},
    .selection=+[](void* raw,wl_data_device*,wl_data_offer* offer){auto& self=*static_cast<Impl*>(raw);self.selected=nullptr;if(auto it=self.offers.find(offer);it!=self.offers.end())self.selected=it->second.get();std::erase_if(self.offers,[&](const auto& pair){return pair.first!=offer;});}
};
WaylandConnection::WaylandConnection():impl_(std::make_unique<Impl>()){}
WaylandConnection::~WaylandConnection(){if(active==this)active=nullptr;}
WaylandConnection* wayland_connection()noexcept{return active;}
void WaylandConnection::open(const HostOptions& options,WaylandCallbacks callbacks){if(active)throw std::logic_error("Wayland host already active on this thread");impl_->open(options,std::move(callbacks));active=this;}
void WaylandConnection::wait(int fd,int timeout){impl_->wait(fd,timeout);}
void WaylandConnection::dispatch(){impl_->dispatch();}
void WaylandConnection::title(std::string_view value){impl_->require();std::string text(value);xdg_toplevel_set_title(impl_->top,text.c_str());}
void WaylandConnection::minimum(Size size){impl_->require();xdg_toplevel_set_min_size(impl_->top,int(std::ceil(size.width)),int(std::ceil(size.height)));}
void WaylandConnection::resize(Size size){impl_->require();impl_->extent=size;xdg_surface_set_window_geometry(impl_->role,0,0,int(size.width),int(size.height));if(impl_->callbacks.configure)impl_->callbacks.configure(size,impl_->scale);}
void WaylandConnection::action(WindowAction action){impl_->require();switch(action){case WindowAction::minimize:xdg_toplevel_set_minimized(impl_->top);break;case WindowAction::maximize:xdg_toplevel_set_maximized(impl_->top);break;case WindowAction::restore:xdg_toplevel_unset_maximized(impl_->top);xdg_toplevel_unset_fullscreen(impl_->top);break;case WindowAction::toggle_fullscreen:if(impl_->fullscreen)xdg_toplevel_unset_fullscreen(impl_->top);else xdg_toplevel_set_fullscreen(impl_->top,nullptr);break;case WindowAction::raise:throw std::invalid_argument("Wayland activation requires a user activation token");default:throw std::invalid_argument("Unknown window action");}wl_display_flush(impl_->display);}
void WaylandConnection::request_frame(){impl_->request_frame();}
void WaylandConnection::cancel_frame(){impl_->cancel_frame();}
wl_display* WaylandConnection::display()const{return impl_->display;}
wl_surface* WaylandConnection::surface()const{return impl_->surface;}
bool WaylandConnection::configured()const{return impl_->configured;}
Size WaylandConnection::size()const{return impl_->extent;}
double WaylandConnection::scale()const{return impl_->scale;}
core::Result<ClipboardContent> WaylandConnection::clipboard(){
    impl_->require();if(!impl_->selected)return ClipboardContent{};auto* offer=impl_->selected;
    const char* mime=offer->types.contains("text/plain;charset=utf-8")?"text/plain;charset=utf-8":offer->types.contains("text/plain")?"text/plain":nullptr;
    if(!mime)return core::fail(core::ErrorCode::unsupported,"Clipboard offer has no UTF-8 text representation");
    int pipes[2];if(pipe2(pipes,O_CLOEXEC|O_NONBLOCK)<0)return core::fail(core::ErrorCode::failed,"Cannot create clipboard transfer pipe");
    struct Close{int fd;~Close(){::close(fd);}}close{pipes[0]};wl_data_offer_receive(offer->native,mime,pipes[1]);::close(pipes[1]);
    std::string bytes;const auto limit=runtime::Clock::now()+std::chrono::seconds(2);char buffer[8192];
    while(runtime::Clock::now()<limit){const auto count=::read(pipes[0],buffer,sizeof(buffer));if(count>0){if(bytes.size()+std::size_t(count)>1024*1024)return core::fail(core::ErrorCode::out_of_range,"Clipboard exceeds 1 MiB");bytes.append(buffer,std::size_t(count));}else if(count==0){ClipboardContent result;result.text=std::move(bytes);if(!valid_clipboard(result))return core::fail(core::ErrorCode::invalid_argument,"Clipboard is not valid UTF-8");return result;}else if(errno!=EINTR&&errno!=EAGAIN)return core::fail(core::ErrorCode::failed,"Clipboard pipe failed");else impl_->wait(pipes[0],deadline_timeout(limit));}
    return core::fail(core::ErrorCode::failed,"Wayland clipboard owner timed out");
}
core::Result<void> WaylandConnection::clipboard(const ClipboardContent& value){
    impl_->require();if(!valid_clipboard(value))return core::fail(core::ErrorCode::invalid_argument,"Invalid clipboard data");
    if(!value.files.empty()||!value.image.empty())return core::fail(core::ErrorCode::unsupported,"Wayland clipboard currently supports UTF-8 text and URLs");
    if(!impl_->device||!impl_->serial)return core::fail(core::ErrorCode::busy,"Wayland clipboard requires a seat input serial");
    if(impl_->sources.size()>=16)return core::fail(core::ErrorCode::busy,"Too many active clipboard sources");
    auto source=std::make_unique<Impl::Source>();source->owner=impl_.get();source->content=value;source->native=wl_data_device_manager_create_data_source(impl_->manager);
    static const wl_data_source_listener listener{.target=+[](void*,wl_data_source*,const char*){},.send=+[](void* raw,wl_data_source*,const char* mime,int fd){auto& source=*static_cast<Impl::Source*>(raw);source.owner->guard([&]{if(!mime||(std::strcmp(mime,"text/plain;charset=utf-8")&&std::strcmp(mime,"text/plain"))||source.owner->sends.size()>=16){::close(fd);return;}struct Close{int fd;~Close(){if(fd>=0)::close(fd);}}close{fd};nonblocking(fd);source.owner->sends.push_back({fd,std::make_shared<const std::string>(source.content.text.empty()?source.content.url:source.content.text)});close.fd=-1;});},.cancelled=+[](void* raw,wl_data_source*){auto& source=*static_cast<Impl::Source*>(raw);auto* owner=source.owner;std::erase_if(owner->sources,[&](const auto& p){return p.get()==&source;});},.dnd_drop_performed=+[](void*,wl_data_source*){},.dnd_finished=+[](void*,wl_data_source*){},.action=+[](void*,wl_data_source*,std::uint32_t){}};
    wl_data_source_add_listener(source->native,&listener,source.get());wl_data_source_offer(source->native,"text/plain;charset=utf-8");wl_data_source_offer(source->native,"text/plain");wl_data_device_set_selection(impl_->device,source->native,impl_->serial);impl_->sources.push_back(std::move(source));wl_display_flush(impl_->display);return {};
}
}
