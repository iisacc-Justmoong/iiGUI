#include "list_item_selector_policy.h"
#include <iiGUI/WidgetState.h>
#include <memory>
#include <optional>
#include <stdexcept>
namespace {void check(bool v,const char* m){if(!v)throw std::runtime_error(m);}
template<class T>T take(ii::core::Result<T> r){if(!r)throw std::runtime_error(r.error().message);return std::move(*r);}
void good(ii::core::Result<void> r){if(!r)throw std::runtime_error(r.error().message);}}
void keyboard_retirement(){using namespace ii::gui;using View=BasicNativeListItemSelector<selector_test::HostPolicy>;
 auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task t){t();});auto root=take(tree->content_host({},"root",{300,80}));
 auto capture=std::make_shared<int>(1);std::weak_ptr<int> weak=capture;selector_test::Input input;input.configuration.options={{"one",true},{"two",true}};
 std::optional<View> view;good(root.set_content([&](WidgetBuilder& b)->ii::core::Result<void>{auto item=View::create(b,{},"selector",input,
  [&,held=capture](int){(void)root.clear_content();});if(!item)return std::unexpected(item.error());view=*item;return item->set_options(input);}));
 capture.reset();auto control=take(view->control());good(control.focus());good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::down}));
 check(weak.expired(),"Retired selector retained its callback");check(!view->options(),"Retired selector handle remained active");tree->close();}

void menu_retirement(){using namespace ii::gui;using View=BasicNativeListItemSelector<selector_test::HostPolicy>;
 auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task t){t();});auto root=take(tree->content_host({},"root",{300,100}));
 auto capture=std::make_shared<int>(1);std::weak_ptr<int> weak=capture;selector_test::Input input;input.configuration.options={{"one",true},{"two",true}};
 std::optional<View> view;int selected=-1;good(root.set_content([&](WidgetBuilder& b)->ii::core::Result<void>{auto item=View::create(b,{},"selector",input,
  [&,held=capture](int index){selected=index;(void)root.clear_content();});if(!item)return std::unexpected(item.error());view=*item;return {};}));
 capture.reset();good(take(view->control()).activate());check(take(take(view->menu()).snapshot()).opened,"Selector did not open its menu");
 good(take(view->option(1)).activate());check(selected==1&&weak.expired(),"Selector menu retirement lost selection or retained callback");
 check(!view->options(),"Retired selector handle remained active");tree->close();}
void throwing_retirement(){using namespace ii::gui;using View=BasicNativeListItemSelector<selector_test::HostPolicy>;
 struct Captured final {std::shared_ptr<int> value;};
 auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task t){t();});auto root=take(tree->content_host({},"root",{300,100}));
 auto capture=std::make_shared<int>(1);std::weak_ptr<int> weak=capture;selector_test::Input input;input.configuration.options={{"one",true},{"two",true}};
 std::optional<View> view;good(root.set_content([&](WidgetBuilder& b)->ii::core::Result<void>{auto item=View::create(b,{},"selector",input,
  [&,held=capture](int){(void)root.clear_content();throw Captured{held};});if(!item)return std::unexpected(item.error());view=*item;return {};}));
 capture.reset();good(take(view->control()).focus());good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::down}));
 check(weak.expired(),"Retired selector retained an exception and its callback resources");tree->close();}
int main(){keyboard_retirement();menu_retirement();throwing_retirement();}
