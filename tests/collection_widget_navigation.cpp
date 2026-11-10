#include <iiGUI/CollectionWidgetNavigation.h>
#include <iiGUI/WidgetState.h>
#include <iiSDKCore/Core/Property/Observable.h>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>
namespace demo {
struct Selection {std::string current_key,anchor_key;std::vector<std::string> selected_keys;friend bool operator==(const Selection&,const Selection&)=default;};
enum class Command {left,right,up,down,home,end,page_previous,page_next,select_all,toggle,activate,clear};
struct Modifiers {bool range=false,additive=false;};struct Layout {std::size_t columns=1,page_rows=1;bool horizontal=false;std::optional<std::size_t> current_index;};
struct Reply {bool handled=false,focus_changed=false,activated=false;};struct Source{};
struct Frame {struct Cell{double x=0,y=0,width=0,height=0;};struct Item{std::size_t index=0;std::string key;};struct Collection{std::vector<Item> items;};
 std::shared_ptr<Source> source;std::vector<Cell> cells;Collection collection;struct {bool horizontal=false;} layout;double width=0,height=0;std::size_t columns=1;};
struct Viewport {void reveal(const std::string&) {}};struct Navigation {ii::core::Observable<Selection> state{Selection{}};bool handle=false;
 template<class F>Reply focus_at(const Source&,std::size_t,Modifiers,bool,F){return {.handled=handle};}
 template<class F=std::function<bool(const int&,std::size_t)>>Reply command(const Source&,Command,Layout={},Modifiers={},F={}){return {};}};
struct Events {std::function<void(std::string)> current_changed,activated;std::function<void(std::vector<std::string>)> selection_changed;};
struct Policy {using Item=int;using Navigation=demo::Navigation;using Frame=demo::Frame;using Viewport=demo::Viewport;using Events=demo::Events;
 using Selection=demo::Selection;using Modifiers=demo::Modifiers;using Reply=demo::Reply;using Command=demo::Command;using Layout=demo::Layout;};
}
namespace {void check(bool v,const char* m){if(!v)throw std::runtime_error(m);}
template<class T>T take(ii::core::Result<T> r){if(!r)throw std::runtime_error(r.error().message);return std::move(*r);}
void good(ii::core::Result<void> r){if(!r)throw std::runtime_error(r.error().message);}}
void callback_retirement(){using namespace ii::gui;using WidgetNavigation=BasicCollectionWidgetNavigation<demo::Policy>;
 auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task t){t();});auto host=take(tree->content_host({},"host",{100,100}));
 demo::Navigation nav;auto viewport=std::make_shared<demo::Viewport>();std::shared_ptr<WidgetNavigation> view;
 auto owner=std::make_shared<int>(1);std::weak_ptr<int> weak=owner;unsigned called=0;
 good(host.set_content([&](WidgetBuilder& b)->ii::core::Result<void>{auto root=b.absolute_stack({},"list",{100,100});if(!root)return std::unexpected(root.error());
  demo::Events events;events.current_changed=[&,keep=owner](std::string){++called;good(host.clear_content());};
  auto created=WidgetNavigation::create(b,*root,nav,viewport,[]{return std::static_pointer_cast<void>(std::make_shared<int>(1));},[]{},std::move(events));
  if(!created)return std::unexpected(created.error());view=*created;return {};}));owner.reset();
 (void)nav.state.set({.current_key="row"});check(called==1&&weak.expired(),"Retirement during navigation retained callbacks");
 (void)nav.state.set({.current_key="second"});check(called==1,"Retired navigation still observed changes");tree->close();}

void admission_retirement(){using namespace ii::gui;using WidgetNavigation=BasicCollectionWidgetNavigation<demo::Policy>;
 auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task t){t();});auto host=take(tree->content_host({},"host2",{100,100}));
 demo::Navigation nav;nav.handle=true;auto viewport=std::make_shared<demo::Viewport>();std::shared_ptr<WidgetNavigation> view;unsigned admissions=0;
 good(host.set_content([&](WidgetBuilder& b)->ii::core::Result<void>{auto root=b.absolute_stack({},"list",{100,100});if(!root)return std::unexpected(root.error());
  auto child=b.absolute_stack(*root,"row",{100,100});if(!child)return std::unexpected(child.error());if(auto r=child->set_frame({0,0,100,100});!r)return r;
  auto created=WidgetNavigation::create(b,*root,nav,viewport,[&]()->std::shared_ptr<void>{if(++admissions==3)good(host.clear_content());return std::static_pointer_cast<void>(std::make_shared<int>(1));},[]{});
  if(!created)return std::unexpected(created.error());view=*created;return {};}));
 demo::Frame frame;frame.source=std::make_shared<demo::Source>();frame.width=100;frame.height=100;frame.cells.push_back({0,0,100,100});frame.collection.items.push_back({0,"row"});view->publish(frame);
 (void)take(tree->drawing_snapshot({100,100}));good(tree->input({.kind=WidgetInputKind::pointer_down,.x=5,.y=5}));
 if(admissions!=3)throw std::runtime_error("Admission count: "+std::to_string(admissions));tree->close();}
int main(){callback_retirement();admission_retirement();}
