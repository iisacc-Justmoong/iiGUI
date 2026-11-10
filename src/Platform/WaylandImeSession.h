#pragma once

#include "WaylandImeProtocol.h"
#include <algorithm>
#include <deque>

namespace ii::gui::detail {
// The transport owns the Wayland object and its monotonically counted commit
// serial. This controller owns editor generations and never calls the client
// while a native Wayland listener is still dispatching.
class WaylandImeSession final {
    struct Ack final {std::uint32_t serial;WaylandImeOwner owner;};
    WaylandImeProtocol ime_;
    std::optional<std::uint64_t> epoch_;
    std::optional<WaylandImeOwner> owner_;
    std::optional<WaylandImePublication> published_;
    std::deque<Ack> acks_;
    std::uint32_t last_serial_=0;
    bool waiting_=false,busy_=false;
    static std::optional<WaylandImeOwner> editable(const std::optional<WidgetSnapshot>& value){
        if(!value||!value->focused||!value->text_input||!value->visible||!value->enabled||
           value->input_blocked||value->read_only||!value->text_revision||!value->text_revision->identity)return {};
        return WaylandImeOwner{value->id,value->text_revision->identity,value->text_revision->focus};
    }
    static bool same(const WaylandImePublication& a,const WaylandImePublication& b){
        return a.password==b.password&&a.multiline==b.multiline&&a.caret==b.caret&&a.surrounding==b.surrounding;
    }
    void remember(std::uint32_t serial){
        last_serial_=serial;acks_.push_back({serial,*owner_});if(acks_.size()>512)acks_.pop_front();
    }
    template<class Host> void cancel(Host& host){if(owner_)(void)ime_.cancel(host);ime_.abandon();}
    template<class Host,class Wire> void refresh_impl(Host& host,Wire& wire,bool external){
        if(!epoch_)return;
        auto before=host.focus();auto next=editable(before);
        if(!next){
            if(owner_){cancel(host);(void)wire.disable(*epoch_);}
            owner_.reset();published_.reset();waiting_=false;return;
        }
        const bool privacy_changed=owner_&&published_&&published_->password!=before->password;
        const bool changed=!owner_||*owner_!=*next||privacy_changed;
        if(changed){
            if(owner_)cancel(host);
            owner_=*next;published_.reset();waiting_=false;if(privacy_changed)acks_.clear();
            if(auto started=ime_.start(host);!started){owner_.reset();(void)wire.disable(*epoch_);return;}
            before=host.focus();if(editable(before)!=owner_)return;
        }
        if(waiting_&&!changed)return;
        auto context=ime_.surrounding(host);if(!context)return;
        auto after=host.focus();
        if(editable(after)!=owner_||!after||!before||after->text_revision!=before->text_revision||
           after->text_selection!=before->text_selection||after->marked_range!=before->marked_range)return;
        WaylandImePublication value{.enable=changed,.external=external,.password=after->password,
                                    .multiline=after->multiline,.caret=after->caret_bounds,.surrounding=std::move(*context)};
        // enable resets prior protocol state. It is also needed when a
        // support becomes unavailable or available: omission alone leaves
        // old text visible, while unsupported initial state cannot be updated.
        if(published_&&bool(published_->surrounding)!=bool(value.surrounding)){
            // An enable request resets the compositor's composition. Keep
            // the old support state until the current preedit is resolved.
            if(after->marked_range)return;
            value.enable=true;
        }
        if(!changed&&published_&&same(*published_,value))return;
        auto serial=wire.publish(*epoch_,value);if(!serial)return;
        if(value.enable)acks_.clear();
        published_=std::move(value);remember(*serial);
    }
public:
    template<class Host,class Wire> void enter(Host& host,Wire& wire,std::uint64_t epoch){
        if(busy_)return;
        if(epoch_&&*epoch_==epoch){refresh(host,wire);return;}
        if(owner_)cancel(host);epoch_=epoch;owner_.reset();published_.reset();acks_.clear();waiting_=false;
        refresh(host,wire);
    }
    template<class Host,class Wire> void refresh(Host& host,Wire& wire){
        if(busy_)return;busy_=true;struct Reset final{bool& value;~Reset(){value=false;}} reset{busy_};
        refresh_impl(host,wire,true);
    }
    template<class Host,class Wire> void done(Host& host,Wire& wire,const WaylandImeEvent& event){
        if(busy_||!epoch_||event.epoch!=*epoch_||!owner_)return;
        busy_=true;struct Reset final{bool& value;~Reset(){value=false;}} reset{busy_};
        const auto found=std::find_if(acks_.rbegin(),acks_.rend(),[&](const Ack& value){return value.serial==event.serial;});
        if(found==acks_.rend()||found->owner!=*owner_||editable(host.focus())!=owner_){refresh_impl(host,wire,true);return;}
        const auto result=ime_.apply(host,event.batch);
        if(!result){refresh_impl(host,wire,true);return;}
        if(event.serial!=last_serial_){waiting_=true;return;}
        waiting_=false;refresh_impl(host,wire,false);
    }
    template<class Host,class Wire> void leave(Host& host,Wire&,std::uint64_t epoch){
        if(busy_||!epoch_||*epoch_!=epoch)return;
        busy_=true;struct Reset final{bool& value;~Reset(){value=false;}} reset{busy_};
        cancel(host);epoch_.reset();owner_.reset();published_.reset();acks_.clear();waiting_=false;
    }
    template<class Host,class Wire> void retire(Host& host,Wire& wire){
        if(busy_||!epoch_)return;
        busy_=true;struct Reset final{bool& value;~Reset(){value=false;}} reset{busy_};
        if(owner_){cancel(host);(void)wire.disable(*epoch_);}
        epoch_.reset();owner_.reset();published_.reset();acks_.clear();waiting_=false;
    }
};
} // namespace ii::gui::detail
