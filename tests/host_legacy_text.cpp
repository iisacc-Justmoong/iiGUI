#include "../src/Platform/HostLegacyText.h"
#include "../src/Widgets/WidgetState.h"
#include <memory>
#include <stdexcept>
#include <utility>
using namespace ii;
template<class T>T take(core::Result<T> r){if(!r)throw std::runtime_error(r.error().message);return std::move(*r);}
void good(core::Result<void> r){if(!r)throw std::runtime_error(r.error().message);}
int main(){
    auto state=std::make_shared<gui::detail::WidgetState>([](runtime::Task t){t();});
    auto host=take(state->content_host({},"host",{800,600}));gui::Widget label;
    good(host.set_content([&](gui::WidgetBuilder& b){return gui::detail::build_legacy_text(b,"Hello",label);}));
    if(take(label.snapshot()).text!="Hello")throw std::runtime_error("Text not mounted");
    good(label.set_text("World"));
    if(take(label.snapshot()).text!="World")throw std::runtime_error("Text update not applied");
    good(host.clear_content());
    if(label.snapshot())throw std::runtime_error("Retired text handle stayed alive");
    state->close();
}
