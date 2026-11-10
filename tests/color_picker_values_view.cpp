#include <iiGUI/NativeColorPickerValuesView.h>
#include "input_field_policy.h"
#include <iiGUI/WidgetState.h>
#include <expected>
#include <memory>
#include <optional>
#include <stdexcept>
namespace {
using namespace ii::gui;void check(bool v,const char* m){if(!v)throw std::runtime_error(m);}
template<class T>T take(ii::core::Result<T> r){if(!r)throw std::runtime_error(r.error().message);return std::move(*r);}void good(ii::core::Result<void> r){if(!r)throw std::runtime_error(r.error().message);}
enum class SurfaceErrorCode{stale,invalid};struct SurfaceError{SurfaceErrorCode code;std::string message;};enum class SelectionErrorCode{stale,invalid};struct SelectionError{SelectionErrorCode code;std::string message;};
enum class Channel{alpha};struct Snapshot{WidgetColor color{},baseline{};std::uint64_t revision=1;};
struct Editor{Snapshot snapshot()const{return {};}};struct Picker{std::shared_ptr<Editor> editor()const{return std::make_shared<Editor>();}
 static std::string format_hex(WidgetColor){return "#000000";}static bool parse_hex(std::string_view,double){return true;}
 std::expected<std::uint64_t,SelectionError> set_hex_if_current(std::uint64_t,std::string_view){return 1;}};
struct Session{std::shared_ptr<Picker> picker()const{return std::make_shared<Picker>();}std::uint64_t revision()const{return 1;}
 std::expected<std::uint64_t,SurfaceError> prepare(std::uint64_t n){return n;}std::expected<std::uint64_t,SurfaceError> edit_channel(Channel,double,std::uint64_t){return 1;}};
struct Input{std::shared_ptr<Session> session;std::int64_t source_revision=0;double width=296;bool enabled=true;};
struct Policy{using Input=::Input;using Snapshot=::Snapshot;using Color=WidgetColor;using Field=BasicNativeInputField<input_field_test::Policy>;using FieldConfiguration=input_field_test::Configuration;using FieldCallbacks=input_field_test::Callbacks;using Picker=::Picker;using SurfaceErrorCode=::SurfaceErrorCode;using SelectionErrorCode=::SelectionErrorCode;using SelectionError=::SelectionError;using Channel=::Channel;using SelectionResult=std::expected<std::uint64_t,::SelectionError>;static bool valid_color(WidgetColor){return true;}};
}
int main(){using View=ii::gui::BasicNativeColorPickerValues<Policy>;auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task t){t();});auto host=take(tree->content_host({},"host",{300,32}));
 auto session=std::make_shared<Session>();std::weak_ptr<Session> weak_session=session;Input input;input.session=session;auto owner=std::make_shared<int>(1);std::weak_ptr<int> weak_owner=owner;std::optional<View> view;
 good(host.set_content([&](WidgetBuilder& b)->ii::core::Result<void>{auto v=View::create(b,{},"values",input,[held=owner](WidgetColor){});if(!v)return std::unexpected(v.error());view=*v;return {};}));session.reset();input.session.reset();owner.reset();
 (void)take(tree->drawing_snapshot({300,32}));check(!weak_session.expired(),"Active value row lost session");good(host.clear_content());
 check(weak_session.expired()&&weak_owner.expired()&&!view->set_options(input),"Retired value row retained resources or accepted updates");tree->close();}
