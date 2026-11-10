#include <iiGUI/NativeTabView.h>
#include <iiGUI/WidgetState.h>
#include <functional>
#include <memory>
#include <utility>
#include <optional>
#include <stdexcept>
namespace {void check(bool v,const char* m){if(!v)throw std::runtime_error(m);}
template<class T>T take(ii::core::Result<T> r){if(!r)throw std::runtime_error(r.error().message);return std::move(*r);}
void good(ii::core::Result<void> r){if(!r)throw std::runtime_error(r.error().message);}
struct Source final:ii::gui::ButtonIconSource {mutable unsigned reads=0;mutable std::function<void()> on_resolve;std::uint64_t revision()const noexcept override{return 0;}
 ii::core::Result<std::shared_ptr<const ii::gui::RasterImage>> resolve(std::string_view,double)const override{++reads;auto callback=std::exchange(on_resolve,{});if(callback)callback();return ii::gui::rgba_image(1,1,{255,255,255,255});}};}
void retention(){using namespace ii::gui;auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task t){t();});auto host=take(tree->content_host({},"host",{200,100}));
 auto source=std::make_shared<Source>();std::weak_ptr<Source> weak_source=source;TabInput input;input.configuration.icon_name="home";input.icon_source=source;
 auto owner=std::make_shared<int>(1);std::weak_ptr<int> weak_owner=owner;std::optional<NativeTab> tab;
 good(host.set_content([&](WidgetBuilder& b)->ii::core::Result<void>{auto v=NativeTab::create(b,{},"tab",input,[keep=owner]{});if(!v)return std::unexpected(v.error());tab=*v;return {};}));
 check(source->reads==1,"Icon was not resolved");good(tab->set_options(input));check(source->reads==1,"Unchanged tab reread its icon");
 (void)take(tree->drawing_snapshot({200,100}));input.icon_source.reset();source.reset();owner.reset();check(!weak_source.expired()&&!weak_owner.expired(),"Live tab lost resources");
 good(host.clear_content());check(weak_source.expired()&&weak_owner.expired()&&!tab->options(),"Retired tab retained resources");tree->close();}

void retire_during_icon(){using namespace ii::gui;auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task t){t();});auto host=take(tree->content_host({},"host",{200,100}));
 auto source=std::make_shared<Source>();TabInput input;input.configuration.icon_name="one";input.icon_source=source;std::optional<NativeTab> tab;
 good(host.set_content([&](WidgetBuilder& b)->ii::core::Result<void>{auto v=NativeTab::create(b,{},"tab",input);if(!v)return std::unexpected(v.error());tab=*v;return {};}));
 source->on_resolve=[&]{(void)host.clear_content();};input.configuration.icon_name="two";auto result=tab->set_options(input);
 check(!result&&result.error().code==ii::core::ErrorCode::closed,"Icon source retirement did not stop the update");tree->close();}
int main(){retention();retire_during_icon();}
