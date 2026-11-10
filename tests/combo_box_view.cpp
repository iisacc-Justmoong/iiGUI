#include <iiGUI/NativeComboBoxView.h>
#include <iiGUI/WidgetState.h>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>
namespace {
void require(bool v,const char* m){if(!v)throw std::runtime_error(m);}
template<class T>T take(ii::core::Result<T> r){if(!r)throw std::runtime_error(r.error().message);return std::move(*r);}
void good(ii::core::Result<void> r){if(!r)throw std::runtime_error(r.error().message);}
}
int main(){using namespace ii::gui;
 auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
 auto host=take(tree->content_host({},"host",{180,100}));std::optional<NativeComboBox> combo;
 auto owner=std::make_shared<int>(1);std::weak_ptr<int> weak=owner;unsigned clicks=0;
 good(host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
  ComboBoxCallbacks cb;cb.clicked=[&,keep=owner]{++clicks;good(host.clear_content());};
  auto value=NativeComboBox::create(builder,{},"combo",{},std::move(cb));if(!value)return std::unexpected(value.error());combo=*value;return {};
 }));owner.reset();
 auto method_owner=std::make_shared<int>(2);std::weak_ptr<int> method_weak=method_owner;
 good(combo->set_method([keep=method_owner](const ButtonEvent&){}));method_owner.reset();
 (void)take(tree->drawing_snapshot({180,100}));good(combo->root().activate());
 require(clicks==1&&!combo->root().snapshot(),"Retirement during callback failed");
 require(weak.expired()&&method_weak.expired(),"Retired combo retained callback resources");
 tree->close();
}
