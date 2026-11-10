#include <iiGUI/NativeColorChannelView.h>
#include "input_field_policy.h"
#include <iiGUI/WidgetState.h>
#include <expected>
#include <memory>
#include <optional>
#include <stdexcept>
namespace {
using namespace ii::gui;void check(bool v,const char* m){if(!v)throw std::runtime_error(m);}
template<class T>T take(ii::core::Result<T> r){if(!r)throw std::runtime_error(r.error().message);return std::move(*r);}void good(ii::core::Result<void> r){if(!r)throw std::runtime_error(r.error().message);}
enum class Channel{hue,saturation,brightness,red,green,blue,cyan,magenta,yellow,black,alpha,gray};enum class ErrorCode{invalid,stale,unavailable};struct Error{ErrorCode code;std::string message;};
struct Frame{Channel channel=Channel::red;WidgetColor color{};double minimum=0,maximum=255,value=0;std::uint64_t revision=1;};
struct Source{std::uint64_t revision()const{return 1;}std::expected<Frame,Error> channel_frame(Channel c){Frame f;f.channel=c;return f;}std::expected<WidgetColor,Error> color(){return WidgetColor{};}std::expected<std::uint64_t,Error> edit_channel(Channel,double,std::uint64_t){return 1;}};
struct Channels{static bool valid_channel(Channel){return true;}static bool valid_frame(const Frame&){return true;}};
struct Input{std::shared_ptr<Source> source;Channel channel=Channel::red;std::int64_t source_revision=0;double width=220;bool enabled=true;friend bool operator==(const Input&,const Input&)=default;};
struct SurfaceSource{virtual ~SurfaceSource()=default;};struct Ramp:SurfaceSource{Ramp(std::shared_ptr<Source>,Channel){}};enum class Kind{spectrum};
struct SurfaceConfig{Kind kind=Kind::spectrum;double width=1,height=1,marker_diameter=1;bool enabled=true;std::string accessibility_name;};
struct SurfaceInput{SurfaceConfig configuration;std::shared_ptr<SurfaceSource> source;std::int64_t source_revision=0;};
struct Surface{Widget widget;static ii::core::Result<Surface> create(WidgetBuilder& b,Widget parent,std::string_view id,SurfaceInput,std::function<void(WidgetColor)>){auto v=b.action_surface(parent,id,{});if(!v)return std::unexpected(v.error());return Surface{*v};}Widget root()const{return widget;}ii::core::Result<void> set_options(SurfaceInput){return {};}};
struct Policy{using Input=::Input;using Frame=::Frame;using Channel=::Channel;using Channels=::Channels;using SurfaceError=::Error;using SurfaceErrorCode=::ErrorCode;using RampSource=::Ramp;using Surface=::Surface;using SurfaceInput=::SurfaceInput;using SurfaceKind=::Kind;using Field=BasicNativeInputField<input_field_test::Policy>;using FieldConfiguration=input_field_test::Configuration;using FieldCallbacks=input_field_test::Callbacks;using Color=WidgetColor;};
}
int main(){using View=ii::gui::BasicNativeColorChannel<Policy>;auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task t){t();});auto host=take(tree->content_host({},"host",{300,40}));auto source=std::make_shared<Source>();std::weak_ptr<Source> weak_source=source;auto owner=std::make_shared<int>(1);std::weak_ptr<int> weak_owner=owner;Input input;input.source=source;std::optional<View> view;
 good(host.set_content([&](WidgetBuilder& b)->ii::core::Result<void>{auto v=View::create(b,{},"channel",input,[held=owner](WidgetColor){});if(!v)return std::unexpected(v.error());view=*v;return {};}));source.reset();owner.reset();input.source.reset();(void)take(tree->drawing_snapshot({300,40}));
 good(host.clear_content());check(weak_source.expired()&&weak_owner.expired()&&!view->set_options(input),"Retired channel retained resources or accepted updates");tree->close();}
