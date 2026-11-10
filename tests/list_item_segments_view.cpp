#include <iiGUI/NativeListItemSegmentsView.h>
#include <iiGUI/WidgetState.h>
#include <memory>
#include <optional>
#include <stdexcept>
namespace {void check(bool v,const char* m){if(!v)throw std::runtime_error(m);}
template<class T>T take(ii::core::Result<T> r){if(!r)throw std::runtime_error(r.error().message);return std::move(*r);}
void good(ii::core::Result<void> r){if(!r)throw std::runtime_error(r.error().message);}
struct Command final:ii::gui::ListItemSegmentCommand{int* count;explicit Command(int* n):count(n){}void invoke()const override{++*count;}};}
void configure_before_publication(){using namespace ii::gui;auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task t){t();});
 auto root=take(tree->content_host({},"root",{300,80}));ListItemSegmentsInput input;std::optional<ListItemSegmentsView> view;
 good(root.set_content([&](WidgetBuilder& b)->ii::core::Result<void>{auto item=ListItemSegmentsView::create(b,{},"segments",input);
  if(!item)return std::unexpected(item.error());view=*item;return item->set_options(input);}));
 check(bool(view->options()),"Segment configuration failed before publication");tree->close();}
void callback_retirement(){using namespace ii::gui;auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task t){t();});
 auto root=take(tree->content_host({},"root",{300,80}));auto capture=std::make_shared<int>(1);std::weak_ptr<int> weak_capture=capture;
 int count=0;auto command=std::make_shared<Command>(&count);std::weak_ptr<Command> weak_command=command;ListItemSegmentsInput input;
 input.configuration.options={{"one",true,{}},{"two",true,command}};std::optional<ListItemSegmentsView> view;
 good(root.set_content([&](WidgetBuilder& b)->ii::core::Result<void>{auto item=ListItemSegmentsView::create(b,{},"segments",input,
  [&,held=capture](int){(void)root.clear_content();});if(!item)return std::unexpected(item.error());view=*item;return {};}));
 capture.reset();command.reset();input.configuration.options.clear();
 good(take(view->segment(1)).activate());check(count==0,"Retired selection invoked a stale command");
 check(weak_capture.expired()&&weak_command.expired(),"Retired segments retained callbacks or commands");
 check(!view->options(),"Retired segment handle remained active");tree->close();}

void throwing_retirement(){using namespace ii::gui;struct Captured final {std::shared_ptr<int> value;};
 auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task t){t();});auto root=take(tree->content_host({},"root",{300,80}));
 auto capture=std::make_shared<int>(1);std::weak_ptr<int> weak=capture;ListItemSegmentsInput input;
 std::optional<ListItemSegmentsView> view;good(root.set_content([&](WidgetBuilder& b)->ii::core::Result<void>{auto item=ListItemSegmentsView::create(b,{},"segments",input,
  [&,held=capture](int){(void)root.clear_content();throw Captured{held};});if(!item)return std::unexpected(item.error());view=*item;return {};}));
 capture.reset();good(take(view->segment(1)).activate());check(weak.expired(),"Retired segments retained an exception and its callback resources");tree->close();}
int main(){configure_before_publication();callback_retirement();throwing_retirement();}
