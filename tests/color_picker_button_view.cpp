#include <iiGUI/NativeColorPickerButtonView.h>
#include <iiGUI/WidgetState.h>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
namespace {
using namespace ii::gui;using ii::core::Result;
void require(bool v,const char* m){if(!v)throw std::runtime_error(m);}
template<class T>T take(Result<T> v){if(!v)throw std::runtime_error(v.error().message);return std::move(*v);}
void good(Result<void> v){if(!v)throw std::runtime_error(v.error().message);}
struct Source{mutable std::function<void()> on_read;std::uint64_t revision()const noexcept{return 0;}
    Result<WidgetColor> color()const{auto fn=std::move(on_read);if(fn)fn();return WidgetColor{1,0,0,1};}};
struct Input{ColorPickerButtonConfiguration configuration;std::shared_ptr<const Source> source;std::int64_t source_revision=0;bool require_source=false;
    friend bool operator==(const Input&,const Input&)=default;};
struct Callbacks{std::function<void()> clicked,pressed,released,canceled;};
struct Policy{using Model=ColorPickerButtonModel;using Input=::Input;using Callbacks=::Callbacks;using Color=WidgetColor;using Rect=ColorPickerButtonRect;
    static std::span<const std::uint8_t> hue_ring(){return {};}};
void lifetime(bool retire_during_read){using Button=ii::gui::BasicNativeColorPickerButton<Policy>;
    auto tree=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{200,100}));std::optional<Button> button;
    auto payload=std::make_shared<int>(7);std::weak_ptr<int> weak=payload;
    auto source=std::make_shared<Source>();std::weak_ptr<Source> weak_source=source;
    good(host.set_content([&](auto& builder)->Result<void>{Input input;input.configuration.show_hue_ring=false;input.source=source;
        auto created=Button::create(builder,{},"color",input,Callbacks{.clicked=[payload]{}});if(!created)return std::unexpected(created.error());button=*created;return {};}));
    payload.reset();require(!weak.expired()&&!weak_source.expired(),"Live button lost its resources");
    if(retire_during_read){source->on_read=[&]{good(host.clear_content());};auto input=take(button->options());
        auto changed=button->set_options(std::move(input));require(!changed&&changed.error().code==ii::core::ErrorCode::closed,"Retirement during color read was not cancelled");}
    else good(host.clear_content());
    source.reset();require(weak.expired()&&weak_source.expired(),"Retired button retained callback or source");tree->close();
}
}
int main(){lifetime(false);lifetime(true);}
