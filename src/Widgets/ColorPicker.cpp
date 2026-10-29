#include <iiGUI/ColorPicker.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <thread>
#include <vector>

namespace ii::gui {
namespace {
bool valid(WidgetColor c) {
    for(auto v:{c.red,c.green,c.blue,c.alpha}) if(!std::isfinite(v) || v<0 || v>1) return false;
    return true;
}
double unit(double v) { return std::clamp(v,0.0,1.0); }
WidgetColor hsv(double h,double s,double v,double a=1) {
    h=(h==360?0:h)/60;const int sector=int(h);const double f=h-sector,p=v*(1-s),q=v*(1-s*f),t=v*(1-s*(1-f));
    switch(sector%6) {case 0:return {v,t,p,a};case 1:return {q,v,p,a};case 2:return {p,v,t,a};case 3:return {p,q,v,a};case 4:return {t,p,v,a};default:return {v,p,q,a};}
}
WidgetColor cmyk(std::array<double,4> c) {return {(1-c[0])*(1-c[3]),(1-c[1])*(1-c[3]),(1-c[2])*(1-c[3]),1};}
int byte(double v) {return int(std::lround(unit(v)*255));}
std::string hexadecimal(WidgetColor c) {
    constexpr char digits[]="0123456789ABCDEF";std::string result;
    for(auto v:{c.red,c.green,c.blue}) {auto n=byte(v);result+=digits[n>>4];result+=digits[n&15];}return result;
}
}
struct ColorPickerModel::Impl final {
    std::thread::id owner=std::this_thread::get_id();
    ColorPickerState state;
    std::array<double,4> inks{};
    bool explicit_previous=false,notifying=false;
    std::vector<std::weak_ptr<const ColorChangePort>> receivers;
    core::Result<void> ready(bool edit=false) const {
        if(owner!=std::this_thread::get_id()) return core::fail(core::ErrorCode::wrong_executor,"Color model requires its owning thread");
        if(edit && notifying) return core::fail(core::ErrorCode::busy,"Color edits cannot recurse from a change receiver");
        return {};
    }
    core::Result<void> notify(ColorPickerChange change) {
        change.revision=state.revision;std::array<std::shared_ptr<const ColorChangePort>,256> pending;std::size_t count=0;
        std::erase_if(receivers,[](const auto& r){return r.expired();});
        for(auto& weak:receivers) if(auto p=weak.lock()) pending[count++]=std::move(p);
        notifying=true;struct Reset final {bool& value;~Reset(){value=false;}}reset{notifying};
        std::optional<runtime::QueryError> failure;
        for(std::size_t i=0;i<count;++i) {auto r=pending[i]->send(change);if(!r && r.error().code!=runtime::QueryErrorCode::receiver_expired && !failure) failure=r.error();}
        if(failure) return core::fail(core::ErrorCode::failed,"Color edit committed; a change receiver failed",failure->cause);
        return {};
    }
    void derive(bool keep_hsv,bool keep_inks) {
        const auto c=state.color;const auto maximum=std::max({c.red,c.green,c.blue}),minimum=std::min({c.red,c.green,c.blue}),d=maximum-minimum;
        if(!keep_hsv) {
            if(d>0) {
                auto h=maximum==c.red?60*std::fmod((c.green-c.blue)/d,6):maximum==c.green?60*((c.blue-c.red)/d+2):60*((c.red-c.green)/d+4);
                state.hue=h<0?h+360:h;
            }
            state.saturation=maximum>0?d/maximum:0;state.brightness=maximum;
        }
        if(!keep_inks) {const auto k=1-maximum;inks=maximum>0?std::array<double,4>{(maximum-c.red)/maximum,(maximum-c.green)/maximum,(maximum-c.blue)/maximum,k}:std::array<double,4>{0,0,0,1};}
        state.hex=hexadecimal(c);
    }
    core::Result<void> update(WidgetColor c,bool edited,bool keep_hsv=false,bool keep_inks=false,bool previous=false) {
        const bool changed=c!=state.color;state.color=c;derive(keep_hsv,keep_inks);++state.revision;
        return notify({.color_changed=changed,.previous_changed=previous,.channels_changed=true,.edited=edited && changed});
    }
};
ColorPickerModel::ColorPickerModel():impl_(std::make_unique<Impl>()) {}
ColorPickerModel::~ColorPickerModel()=default;
core::Result<ColorPickerState> ColorPickerModel::state() const {if(auto r=impl_->ready();!r)return std::unexpected(r.error());return impl_->state;}
core::Result<void> ColorPickerModel::observe(std::shared_ptr<const ColorChangePort> receiver) {
    if(auto r=impl_->ready();!r)return r;
    if(!receiver) return core::fail(core::ErrorCode::invalid_argument,"Color receiver must be nonnull");
    std::erase_if(impl_->receivers,[](const auto& r){return r.expired();});
    for(auto& r:impl_->receivers) if(r.lock()==receiver) return {};
    if(impl_->receivers.size()>=256) return core::fail(core::ErrorCode::full,"Color model has 256 live change receivers");
    impl_->receivers.push_back(receiver);return {};
}
core::Result<void> ColorPickerModel::set_color(WidgetColor c) {
    if(auto r=impl_->ready(true);!r)return r;if(!valid(c))return core::fail(core::ErrorCode::invalid_argument,"Color components must be finite encoded sRGB in [0,1]");
    if(c==impl_->state.color)return {};bool previous=!impl_->explicit_previous;
    if(previous) impl_->state.previous_color=c;return impl_->update(c,false,false,false,previous);
}
core::Result<void> ColorPickerModel::set_previous_color(WidgetColor c) {
    if(auto r=impl_->ready(true);!r)return r;if(!valid(c))return core::fail(core::ErrorCode::invalid_argument,"Invalid previous color");
    impl_->explicit_previous=true;if(c==impl_->state.previous_color)return {};impl_->state.previous_color=c;return impl_->notify({.previous_changed=true});
}
core::Result<void> ColorPickerModel::edit_color(WidgetColor c) {
    if(auto r=impl_->ready(true);!r)return r;if(!valid(c))return core::fail(core::ErrorCode::invalid_argument,"Invalid edited color");return impl_->update(c,true);
}
core::Result<void> ColorPickerModel::edit_hsv(double h,double s,double v) {
    if(auto r=impl_->ready(true);!r)return r;
    if(!std::isfinite(h)||!std::isfinite(s)||!std::isfinite(v))return core::fail(core::ErrorCode::invalid_argument,"HSV must be finite");
    auto& state=impl_->state;state.hue=std::clamp(h,0.0,360.0);state.saturation=unit(s);state.brightness=unit(v);
    return impl_->update(hsv(state.hue,state.saturation,state.brightness,state.color.alpha),true,true);
}
core::Result<double> ColorPickerModel::channel_value(ColorChannel channel) const {
    if(auto r=impl_->ready();!r)return std::unexpected(r.error());auto& s=impl_->state;
    switch(channel) {
    case ColorChannel::hue:return s.hue;case ColorChannel::saturation:return s.saturation*100;case ColorChannel::brightness:return s.brightness*100;
    case ColorChannel::red:return s.color.red*255;case ColorChannel::green:return s.color.green*255;case ColorChannel::blue:return s.color.blue*255;
    case ColorChannel::cyan:case ColorChannel::magenta:case ColorChannel::yellow:case ColorChannel::black:return impl_->inks[unsigned(channel)-unsigned(ColorChannel::cyan)]*100;
    case ColorChannel::alpha:return s.color.alpha*100;
    case ColorChannel::gray:return (1-double((byte(s.color.red)*11+byte(s.color.green)*16+byte(s.color.blue)*5)/32)/255)*100;
    }return core::fail(core::ErrorCode::invalid_argument,"Unknown color channel");
}
core::Result<WidgetColor> ColorPickerModel::channel_color(ColorChannel channel,double position) const {
    if(auto r=impl_->ready();!r)return std::unexpected(r.error());if(!std::isfinite(position))return core::fail(core::ErrorCode::invalid_argument,"Ramp position must be finite");
    auto& s=impl_->state;const auto p=unit(position);auto c=s.color;c.alpha=1;
    switch(channel) {
    case ColorChannel::hue:return hsv(p*360,1,1);case ColorChannel::saturation:return hsv(s.hue,p,s.brightness);case ColorChannel::brightness:return hsv(s.hue,s.saturation,p);
    case ColorChannel::red:c.red=p;return c;case ColorChannel::green:c.green=p;return c;case ColorChannel::blue:c.blue=p;return c;
    case ColorChannel::cyan:case ColorChannel::magenta:case ColorChannel::yellow:case ColorChannel::black:{auto inks=impl_->inks;inks[unsigned(channel)-unsigned(ColorChannel::cyan)]=p;return cmyk(inks);}
    case ColorChannel::alpha:c.alpha=p;return c;case ColorChannel::gray:return WidgetColor{1-p,1-p,1-p,1};
    }return core::fail(core::ErrorCode::invalid_argument,"Unknown color channel");
}
core::Result<void> ColorPickerModel::edit_channel(ColorChannel channel,double value) {
    if(auto r=impl_->ready(true);!r)return r;if(!std::isfinite(value) || unsigned(channel)>unsigned(ColorChannel::gray))return core::fail(core::ErrorCode::invalid_argument,"Invalid channel or value");
    const auto limit=channel==ColorChannel::hue?360:channel>=ColorChannel::red && channel<=ColorChannel::blue?255:100;const auto p=unit(value/limit);auto& s=impl_->state;
    if(channel<=ColorChannel::brightness)return edit_hsv(channel==ColorChannel::hue?p*360:s.hue,channel==ColorChannel::saturation?p:s.saturation,channel==ColorChannel::brightness?p:s.brightness);
    auto c=channel_color(channel,p);if(!c)return std::unexpected(c.error());c->alpha=channel==ColorChannel::alpha?p:s.color.alpha;
    const bool inks=channel>=ColorChannel::cyan && channel<=ColorChannel::black;
    if(inks)impl_->inks[unsigned(channel)-unsigned(ColorChannel::cyan)]=p;
    return impl_->update(*c,true,channel==ColorChannel::alpha,inks || channel==ColorChannel::alpha);
}
core::Result<bool> ColorPickerModel::edit_hex(std::string_view text) {
    if(auto r=impl_->ready(true);!r)return std::unexpected(r.error());
    auto begin=text.find_first_not_of(" \t\r\n");if(begin==text.npos)return false;text=text.substr(begin,text.find_last_not_of(" \t\r\n")-begin+1);if(text.front()=='#')text.remove_prefix(1);
    if(text.size()!=6 && text.size()!=8)return false;std::array<double,4> v{0,0,0,impl_->state.color.alpha};
    for(std::size_t i=0;i<text.size()/2;++i) {int value=0;for(char c:text.substr(i*2,2)) {int n=c>='0'&&c<='9'?c-'0':c>='A'&&c<='F'?c-'A'+10:c>='a'&&c<='f'?c-'a'+10:-1;if(n<0)return false;value=value*16+n;}v[i]=value/255.0;}
    auto r=impl_->update({v[0],v[1],v[2],v[3]},true);if(!r)return std::unexpected(r.error());return true;
}
core::Result<void> ColorPickerModel::accept() {
    if(auto r=impl_->ready(true);!r)return r;if(impl_->state.previous_color==impl_->state.color)return {};
    impl_->state.previous_color=impl_->state.color;return impl_->notify({.previous_changed=true});
}
core::Result<void> ColorPickerModel::cancel() {if(auto r=impl_->ready(true);!r)return r;return impl_->update(impl_->state.previous_color,true);}
} // namespace ii::gui
