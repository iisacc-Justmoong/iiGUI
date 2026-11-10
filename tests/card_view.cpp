#include <iiGUI/NativeCardView.h>
#include <iiGUI/WidgetState.h>
#include <functional>
#include <memory>
#include <utility>
#include <optional>
#include <stdexcept>
namespace {void check(bool v,const char* m){if(!v)throw std::runtime_error(m);}
template<class T>T take(ii::core::Result<T> r){if(!r)throw std::runtime_error(r.error().message);return std::move(*r);}
void good(ii::core::Result<void> r){if(!r)throw std::runtime_error(r.error().message);}
struct Source final:ii::gui::ButtonIconSource {mutable std::function<void()> callback;
 ii::core::Result<std::shared_ptr<const ii::gui::RasterImage>> resolve(std::string_view,double)const override{auto f=std::exchange(callback,{});if(f)f();return ii::gui::rgba_image(1,1,{255,255,255,255});}};}
void callback_retirement(){using namespace ii::gui;auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task t){t();});auto host=take(tree->content_host({},"host",{300,300}));
 auto owner=std::make_shared<int>(1);std::weak_ptr<int> weak=owner;std::optional<NativeCard> card;unsigned changed=0;CardInput input;input.configuration.type=CardType::project;input.configuration.title="Project";
 good(host.set_content([&](WidgetBuilder& b)->ii::core::Result<void>{CardCallbacks cb;cb.selected_changed=[&,keep=owner](bool){++changed;(void)host.clear_content();};
  auto v=NativeCard::create(b,{},"card",input,std::move(cb));if(!v)return std::unexpected(v.error());card=*v;return {};}));owner.reset();
 (void)take(tree->drawing_snapshot({300,300}));good(card->surface().activate());check(changed==1&&weak.expired()&&!card->options(),"Retired card retained callbacks");tree->close();}

void source_reentry(){using namespace ii::gui;auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task t){t();});auto host=take(tree->content_host({},"host",{300,300}));
 auto source=std::make_shared<Source>();CardInput input;input.configuration.icon_name="one";input.icon_source=source;std::optional<NativeCard> card;
 good(host.set_content([&](WidgetBuilder& b)->ii::core::Result<void>{auto v=NativeCard::create(b,{},"card",input);if(!v)return std::unexpected(v.error());card=*v;return {};}));
 auto older=input;older.configuration.icon_name="two";source->callback=[&]{good(card->set_options(input));};good(card->set_options(older));
 check(take(card->options()).configuration.icon_name=="one","An older icon result replaced the latest request");
 source->callback=[&]{(void)host.clear_content();};older.configuration.icon_name="three";auto result=card->set_options(older);
 check(!result&&result.error().code==ii::core::ErrorCode::closed,"Icon source retirement did not stop the card update");tree->close();}
int main(){callback_retirement();source_reentry();}
