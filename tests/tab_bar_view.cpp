#include <iiGUI/NativeTabBarView.h>
#include <iiGUI/NativeTabView.h>
#include <iiGUI/WidgetState.h>
#include <memory>
#include <optional>
#include <stdexcept>
namespace {void check(bool v,const char* m){if(!v)throw std::runtime_error(m);}
template<class T>T take(ii::core::Result<T> r){if(!r)throw std::runtime_error(r.error().message);return std::move(*r);}
void good(ii::core::Result<void> r){if(!r)throw std::runtime_error(r.error().message);}
struct Source final:ii::gui::ButtonIconSource {ii::core::Result<std::shared_ptr<const ii::gui::RasterImage>> resolve(std::string_view,double)const override{return ii::gui::rgba_image(1,1,{255,255,255,255});}};}
int main(){using namespace ii::gui;auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task t){t();});auto host=take(tree->content_host({},"host",{400,100}));
 auto source=std::make_shared<Source>();auto owner=std::make_shared<int>(1);std::weak_ptr<int> weak=owner;std::optional<NativeTabBar> bar;std::optional<NativeTab> first,second;unsigned changed=0;
 good(host.set_content([&](WidgetBuilder& b)->ii::core::Result<void>{auto v=NativeTabBar::create(b,{},"bar",{},[&,keep=owner](int){++changed;(void)host.clear_content();});if(!v)return std::unexpected(v.error());bar=*v;
  TabInput input;input.configuration.icon_name="one";input.icon_source=source;auto a=NativeTab::create(b,bar->root(),"first",input);if(!a)return std::unexpected(a.error());first=*a;
  if(auto r=bar->register_tab(first->weak_group_port());!r)return r;input.configuration.icon_name="two";auto z=NativeTab::create(b,bar->root(),"second",input);if(!z)return std::unexpected(z.error());second=*z;
  if(auto r=bar->register_tab(second->weak_group_port());!r)return r;return bar->finalize();}));owner.reset();
 (void)take(tree->drawing_snapshot({400,100}));good(second->root().activate());
 check(changed==1&&weak.expired()&&!bar->options(),"Retirement during tab selection retained callbacks");tree->close();}
