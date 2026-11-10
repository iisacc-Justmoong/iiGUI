#include <iiGUI/NativeColorPickerView.h>
#include <iiGUI/WidgetState.h>
#include <expected>
#include <memory>
#include <optional>
#include <stdexcept>
#include <vector>
namespace {
using namespace ii::gui;void check(bool v,const char* m){if(!v)throw std::runtime_error(m);}
template<class T>T take(ii::core::Result<T> r){if(!r)throw std::runtime_error(r.error().message);return std::move(*r);}void good(ii::core::Result<void> r){if(!r)throw std::runtime_error(r.error().message);}
enum class Kind{wheel,hue_rail};enum class Channel{red};enum class Tone{normal,primary};
struct Config{double width=320,padding=12;bool enabled=true,show_recent_colors=false,show_actions=true;std::string accessibility_name="Picker",cancel_text="Cancel",apply_text="Apply";std::vector<WidgetColor> recent_colors;};
struct Rect{double x=0,y=0,width=0,height=0;};struct Layout{double width=320,height=100;Kind surface_kind=Kind::wheel;std::optional<Kind> rail_kind;Rect surface{0,0,20,20},rail,divider,comparison,hex_label,hex_field,alpha_label,alpha_field,alpha_suffix,recent_label,eyedropper,color_space,cancel,apply;std::vector<std::pair<Channel,Rect>> channels;std::vector<Rect> recent;};
struct LayoutModel{static std::optional<Layout> compute(const Config& c){Layout l;l.width=c.width;return l;}};
struct Snapshot{WidgetColor color{},baseline{};std::uint64_t revision=1;};struct Err{std::string message;};struct Editor{Snapshot snapshot()const{return {};}};
struct Picker{Config c;bool editing=false;std::shared_ptr<Editor> editor()const{return std::make_shared<Editor>();}static bool validate(const Config& v){return v.width>0;}static std::string format_hex(WidgetColor){return "#000000";}bool active()const{return editing;}bool set_configuration(Config v){c=std::move(v);return true;}Config configuration()const{return c;}
 std::expected<std::uint64_t,Err> select_recent_if_current(std::uint64_t,std::size_t){return 1;}std::expected<Snapshot,Err> accept(){editing=false;return Snapshot{};}std::expected<Snapshot,Err> cancel(){editing=false;return Snapshot{};}};
struct Session{std::shared_ptr<Picker> p=std::make_shared<Picker>();std::shared_ptr<Picker> picker(){return p;}std::uint64_t revision()const{return 1;}std::expected<std::uint64_t,Err> prepare(std::uint64_t n){p->editing=true;return n;}};
struct Input{Config configuration;std::shared_ptr<Session> session;std::int64_t source_revision=0;bool preserve_recent_colors=false;};
struct SurfaceConfig{Kind kind=Kind::wheel;double width=1,height=1;bool enabled=true;std::string accessibility_name;};struct SurfaceInput{std::shared_ptr<Session> source;std::int64_t source_revision=0;SurfaceConfig configuration;};
struct ChannelInput{std::shared_ptr<Session> source;std::int64_t source_revision=0;Channel channel=Channel::red;double width=1;bool enabled=true;};
struct ValuesInput{std::shared_ptr<Session> session;std::int64_t source_revision=0;double width=1;bool enabled=true;};
struct ButtonConfig{bool enabled=true;double width=1,height=1;Tone tone=Tone::normal;bool icon_mode=false;double icon_size=1,horizontal_padding=0;std::string accessibility_name,text;};struct ButtonInput{ButtonConfig configuration;std::string icon_name;};
struct Callbacks{std::function<void(WidgetColor)> edited,accepted;std::function<void()> canceled,eyedropper_requested;};
struct Surface{Widget w;static ii::core::Result<Surface> create(WidgetBuilder& b,Widget parent,std::string_view id,SurfaceInput,std::function<void(WidgetColor)>){auto v=b.action_surface(parent,id,{});if(!v)return std::unexpected(v.error());return Surface{*v};}Widget root()const{return w;}ii::core::Result<void> set_options(SurfaceInput){return {};}};
struct ChannelView{Widget w;static ii::core::Result<ChannelView> create(WidgetBuilder& b,Widget parent,std::string_view id,ChannelInput,std::function<void(WidgetColor)>){auto v=b.action_surface(parent,id,{});if(!v)return std::unexpected(v.error());return ChannelView{*v};}Widget root()const{return w;}Widget field()const{return w;}ii::core::Result<void> set_options(ChannelInput){return {};}};
struct Values{Widget w;static ii::core::Result<Values> create(WidgetBuilder& b,Widget parent,std::string_view id,ValuesInput,std::function<void(WidgetColor)>){auto v=b.action_surface(parent,id,{});if(!v)return std::unexpected(v.error());return Values{*v};}Widget root()const{return w;}Widget hex_field()const{return w;}Widget alpha_field()const{return w;}ii::core::Result<void> set_options(ValuesInput){return {};}};
struct Button{Widget w;static ii::core::Result<Button> create(WidgetBuilder& b,Widget parent,std::string_view id,ButtonInput,ii::runtime::Task cb){auto v=b.action_surface(parent,id,{},std::move(cb));if(!v)return std::unexpected(v.error());return Button{*v};}Widget root()const{return w;}ii::core::Result<void> set_options(ButtonInput){return {};}};
struct Policy{using Input=::Input;using Configuration=Config;using Layout=::Layout;using Rect=::Rect;using Snapshot=::Snapshot;using Color=WidgetColor;using Picker=::Picker;using Surface=::Surface;using ChannelView=::ChannelView;using Values=::Values;using Button=::Button;using SurfaceInput=::SurfaceInput;using ChannelInput=::ChannelInput;using ValuesInput=::ValuesInput;using ButtonInput=::ButtonInput;using SurfaceKind=Kind;using Channel=::Channel;using ButtonTone=Tone;using Callbacks=::Callbacks;using LayoutModel=::LayoutModel;static bool valid_color(WidgetColor){return true;}};
}
int main(){using View=ii::gui::BasicNativeColorPicker<Policy>;auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task t){t();});auto host=take(tree->content_host({},"host",{320,100}));auto session=std::make_shared<Session>();std::weak_ptr<Session> weak_session=session;Input input;input.session=session;auto owner=std::make_shared<int>(1);std::weak_ptr<int> weak_owner=owner;std::optional<View> view;int accepted=0;
 good(host.set_content([&](WidgetBuilder& b)->ii::core::Result<void>{Callbacks c;c.accepted=[&,held=owner](WidgetColor){++accepted;};auto v=View::create(b,{},"picker",input,std::move(c));if(!v)return std::unexpected(v.error());view=*v;return {};}));input.session.reset();session.reset();owner.reset();(void)take(tree->drawing_snapshot({320,100}));good(view->accept());check(accepted==1,"Picker accept event missing");good(host.clear_content());check(weak_session.expired()&&weak_owner.expired()&&!view->options(),"Retired picker retained resources");tree->close();}
