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
struct Viewport {void reveal(const std::string&) {}};struct Navigation {ii::core::Observable<Selection> state{Selection{}};bool handle=false,update=false;
 template<class F>Reply focus_at(const Source&,std::size_t index,Modifiers,bool,F){if(handle&&update)(void)state.set({.current_key=std::to_string(index)});return {.handled=handle};}
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
void double_click(){using namespace ii::gui;using WidgetNavigation=BasicCollectionWidgetNavigation<demo::Policy>;
 auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task t){t();});auto host=take(tree->content_host({},"double-host",{100,100}));
 demo::Navigation nav;nav.handle=true;nav.update=true;auto viewport=std::make_shared<demo::Viewport>();std::shared_ptr<WidgetNavigation> view;
 std::vector<std::string> activated;
 good(host.set_content([&](WidgetBuilder& b)->ii::core::Result<void>{auto root=b.absolute_stack({},"list",{100,100});if(!root)return std::unexpected(root.error());
  demo::Events events;events.activated=[&](std::string key){activated.push_back(std::move(key));};
  auto created=WidgetNavigation::create(b,*root,nav,viewport,[]{return std::static_pointer_cast<void>(std::make_shared<int>(1));},[]{},std::move(events));
  if(!created)return std::unexpected(created.error());view=*created;return {};}));
 demo::Frame frame;frame.source=std::make_shared<demo::Source>();frame.width=100;frame.height=100;
 frame.cells={{0,0,100,50},{0,50,100,50}};frame.collection.items={{0,"0"},{1,"1"}};view->publish(frame);
 (void)take(tree->drawing_snapshot({100,100}));
 const auto down=[&](double x,double y,double time,std::uint32_t count,bool meta=false){good(tree->input({.kind=WidgetInputKind::pointer_down,.x=x,.y=y,.meta=meta,.timestamp=time,.buttons=1,.native_click_count=count}));};
 const auto up=[&](double x,double y,double time){good(tree->input({.kind=WidgetInputKind::pointer_up,.x=x,.y=y,.timestamp=time}));};
 down(5,5,1,1);up(5,5,1.01);down(5,5,1.1,2);up(5,5,1.11);
 check(activated.empty(),"double click activated a collection without opt-in");
 view->activate_on_double_click(true);
 down(5,5,2,1);up(5,5,2.01);down(5,5,2.1,2);
 check(activated.empty(),"double click activated before pointer release");up(5,5,2.11);
 check(activated==std::vector<std::string>{"0"},"native double click did not activate exactly once");
 down(5,5,3,1);up(5,5,3.01);down(5,55,3.1,2);up(5,55,3.11);
 check(activated.size()==1,"different rows formed a double click");
 down(5,5,4,0);up(5,5,4.01);down(5,5,4.1,0);up(5,5,4.11);
 check(activated==std::vector<std::string>({"0","0"}),"fallback double click did not activate");
 down(5,5,5,1);up(5,5,5.01);down(5,5,5.1,2);up(30,30,5.11);
 check(activated.size()==2,"a drag activated a row");
 down(5,5,6,1);up(5,5,6.01);down(5,5,6.1,2,true);up(5,5,6.11);
 check(activated.size()==2,"a modified selection click activated a row");
 down(5,5,7,1);up(5,55,7.01);down(5,5,7.1,2);up(5,5,7.11);
 check(activated.size()==2,"an unfinished first click formed a double click");
 down(5,5,8,1);up(5,5,8.01);down(5,5,8.1,2);
 good(tree->input({.kind=WidgetInputKind::pointer_cancel,.x=5,.y=5}));up(5,5,8.11);
 check(activated.size()==2,"a canceled double click activated a row");
 view->activate_on_click(true);
 down(5,5,9,1);check(activated.size()==2,"single click activated before release");up(5,5,9.01);
 check(activated==std::vector<std::string>({"0","0","0"}),"single click did not activate exactly once");
 down(5,55,10,1,true);up(5,55,10.01);
 check(activated==std::vector<std::string>({"0","0","0","1"}),"modified single click did not activate its row");
 down(5,5,11,1);up(30,30,11.01);check(activated.size()==4,"single-click drag activated a row");
 down(5,5,12,1);up(5,55,12.01);check(activated.size()==4,"release on a different row activated");
 down(5,5,13,1);good(tree->input({.kind=WidgetInputKind::pointer_cancel,.x=5,.y=5}));up(5,5,13.01);
 check(activated.size()==4,"canceled single click activated");
 down(5,5,13.5,1);view->activate_on_click(false);up(5,5,13.51);
 check(activated.size()==4,"disabling during a press did not cancel activation");
 view->activate_on_click(false);
 down(5,5,14,1);up(5,5,14.01);check(activated.size()==4,"disabled single click activated a row");
 down(5,5,14.1,2);up(5,5,14.11);check(activated.size()==5,"double click did not resume after disabling single click");
 tree->close();}
void embedded_offset(){using namespace ii::gui;using WidgetNavigation=BasicCollectionWidgetNavigation<demo::Policy>;
 auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task t){t();});
 auto root=take(tree->absolute_stack({},"root",{400,300}));
 auto host=take(tree->content_host(root,"outer",{150,140}));good(host.set_frame({120,70,150,140}));
 demo::Navigation nav;nav.handle=true;nav.update=true;auto viewport=std::make_shared<demo::Viewport>();std::shared_ptr<WidgetNavigation> view;
 std::vector<std::string> activated;unsigned buttons=0;
 good(host.set_content([&](WidgetBuilder& outer)->ii::core::Result<void>{
  auto stack=outer.absolute_stack({},"stack",{150,140});if(!stack)return std::unexpected(stack.error());
  auto nested=outer.content_host(*stack,"inner",{100,100});if(!nested)return std::unexpected(nested.error());
  if(auto r=nested->set_frame({20,15,100,100});!r)return r;
  return nested->set_content([&](WidgetBuilder& b)->ii::core::Result<void>{
   auto list=b.absolute_stack({},"list",{100,100});if(!list)return std::unexpected(list.error());
   auto button=b.button(*list,"button",{.text="Button",.height=20,.width=30},[&]{++buttons;});if(!button)return std::unexpected(button.error());
   if(auto r=button->set_frame({60,0,30,20});!r)return r;
   demo::Events events;events.activated=[&](std::string key){activated.push_back(std::move(key));};
   auto created=WidgetNavigation::create(b,*list,nav,viewport,[]{return std::static_pointer_cast<void>(std::make_shared<int>(1));},[]{},std::move(events));
   if(!created)return std::unexpected(created.error());view=*created;view->activate_on_click(true);return {};
  });
 }));
 demo::Frame frame;frame.source=std::make_shared<demo::Source>();frame.width=100;frame.height=100;
 frame.cells={{0,0,100,50},{0,50,100,50}};frame.collection.items={{0,"0"},{1,"1"}};view->publish(frame);
 (void)take(tree->drawing_snapshot({400,300}));
 good(tree->input({.kind=WidgetInputKind::pointer_down,.x=145,.y=140,.timestamp=1,.buttons=1,.native_click_count=1}));
 good(tree->input({.kind=WidgetInputKind::pointer_up,.x=145,.y=140,.timestamp=1.01}));
 check(nav.state.get().current_key=="1" && activated==std::vector<std::string>{"1"},
       "Nested content offset corrupted collection pointer selection or activation");
 good(tree->input({.kind=WidgetInputKind::pointer_down,.x=205,.y=90,.timestamp=2,.buttons=1,.native_click_count=1}));
 good(tree->input({.kind=WidgetInputKind::pointer_up,.x=205,.y=90,.timestamp=2.01}));
 check(buttons==1 && nav.state.get().current_key=="1" && activated.size()==1,
       "Embedded collection intercepted a nested control");
 view->activate_on_click(false);view->activate_on_double_click(true);
 good(tree->input({.kind=WidgetInputKind::pointer_down,.x=145,.y=90,.timestamp=3,.buttons=1,.native_click_count=1}));
 good(tree->input({.kind=WidgetInputKind::pointer_up,.x=145,.y=90,.timestamp=3.01}));
 good(tree->input({.kind=WidgetInputKind::pointer_down,.x=145,.y=90,.timestamp=3.1,.buttons=1,.native_click_count=2}));
 good(tree->input({.kind=WidgetInputKind::pointer_up,.x=145,.y=90,.timestamp=3.11}));
 check(activated==std::vector<std::string>({"1","0"}),"Embedded collection lost double-click activation");
 tree->close();}
int main(){callback_retirement();admission_retirement();double_click();embedded_offset();}
