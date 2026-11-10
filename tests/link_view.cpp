#include <iiGUI/NativeLinkView.h>
#include <iiGUI/WidgetState.h>
#include <memory>
#include <optional>
#include <stdexcept>
namespace {void require(bool v,const char* m){if(!v)throw std::runtime_error(m);}
template<class T>T take(ii::core::Result<T> r){if(!r)throw std::runtime_error(r.error().message);return std::move(*r);}
void good(ii::core::Result<void> r){if(!r)throw std::runtime_error(r.error().message);}}
int main(){using namespace ii::gui;auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task t){t();});
 auto host=take(tree->content_host({},"host",{200,100}));auto context=std::make_shared<LinkNavigationContext>();
 auto page=std::make_shared<LinkPage>();std::weak_ptr<LinkPage> page_weak=page;LinkConfiguration options;options.target=page;
 std::optional<NativeLink> link;auto owner=std::make_shared<int>(1);std::weak_ptr<int> weak=owner;unsigned clicks=0;
 good(host.set_content([&](WidgetBuilder& b)->ii::core::Result<void>{LinkCallbacks c;c.clicked=[&,keep=owner]{++clicks;good(host.clear_content());};
  auto v=NativeLink::create(b,{},"link",context,options,std::move(c));if(!v)return std::unexpected(v.error());link=*v;return {};}));owner.reset();options.target.reset();page.reset();
 (void)take(tree->drawing_snapshot({200,100}));good(link->root().activate());
 require(clicks==1&&!link->root().snapshot()&&weak.expired()&&page_weak.expired(),"Retired link retained its callback, page, or remained active");tree->close();}
