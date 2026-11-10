#include <iiGUI/NativeTableCellItemView.h>
#include "input_field_policy.h"
#include <iiGUI/WidgetState.h>
#include <memory>
#include <optional>
#include <stdexcept>
namespace {
using namespace ii::gui;
void check(bool b,const char* m){if(!b)throw std::runtime_error(m);}
template<class T>T take(ii::core::Result<T> r){if(!r)throw std::runtime_error(r.error().message);return std::move(*r);}
void good(ii::core::Result<void> r){if(!r)throw std::runtime_error(r.error().message);}
struct Palette{WidgetColor background{},divider{},cell_text{},selection{},current{};friend bool operator==(const Palette&,const Palette&)=default;};
struct Configuration{std::string text="value",value_type="string",accessibility_name;double width=120,height=24,padding=8,font_size=13,current_border_width=1;bool enabled=true,visible=true,inputable=true,selected=false,current=false,show_divider=true,clip_content=true;Palette palette;friend bool operator==(const Configuration&,const Configuration&)=default;};
struct Validator{};struct Input{Configuration configuration;std::shared_ptr<const Validator> validator;friend bool operator==(const Input&,const Input&)=default;};
struct Callbacks{std::function<void(std::string)> edited,submitted;std::function<void(std::string,std::string)> rejected;};
struct Box{double x=0,y=0,width=0,height=0;};struct Geometry{Box surface,text,divider;};
struct Visual{bool selected=false,current=false;WidgetColor text{},selection{},border{},divider{};};
struct Problem{std::string message;};
struct Model{Configuration config;static bool validate(const Configuration& c){return c.width>0&&c.height>0;}bool set_configuration(Configuration c){if(!validate(c))return false;config=std::move(c);return true;}const Configuration& configuration()const{return config;}
 std::optional<Geometry> geometry(double w,double h)const{return Geometry{{0,0,w,h},{8,0,w-16,h},{0,0,1,h}};}Visual visual()const{return {config.selected,config.current,config.palette.cell_text,config.palette.selection,config.palette.current,config.palette.divider};}
 std::expected<void,Problem> accept(std::string_view,std::shared_ptr<const Validator>)const{return {};}};
struct Policy{using Model=::Model;using Input=::Input;using Callbacks=::Callbacks;using Rect=Box;using Field=BasicNativeInputField<input_field_test::Policy>;using InputConfiguration=input_field_test::Configuration;using Validator=::Validator;
 static std::string display_text(std::string_view s){return std::string(s);}};
}
int main(){using View=ii::gui::BasicNativeTableCellItem<Policy>;auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task t){t();});auto host=take(tree->content_host({},"host",{300,80}));std::optional<View> view;Input input;auto owner=std::make_shared<int>(1);std::weak_ptr<int> weak=owner;
 good(host.set_content([&](WidgetBuilder& b)->ii::core::Result<void>{Callbacks cb;cb.edited=[keep=owner](std::string){};auto v=View::create(b,{},"cell",input,std::move(cb));if(!v)return std::unexpected(v.error());view=*v;return {};}));owner.reset();
 (void)take(tree->drawing_snapshot({300,80}));check(take(view->options()).configuration.text=="value","Cell text missing");good(view->begin_edit());check(take(view->editor().value().snapshot()).text=="value","Cell editor missing text");
 auto invalid=input;invalid.configuration.width=0;check(!view->set_options(invalid)&&take(view->options()).configuration.text=="value","Invalid cell update replaced state");good(host.clear_content());
 check(weak.expired()&&!view->options(),"Retired cell retained callback or remained active");tree->close();
 bool retire=false;std::optional<Widget> holder;std::optional<View> second;
 detail::WidgetFrameHooks hooks;hooks.accessibility_changed=[&]{if(retire){retire=false;good(holder->clear_content());}};
 auto tree2=std::make_shared<detail::WidgetState>([](ii::runtime::Task t){t();},WidgetAppearance{},hooks);holder=take(tree2->content_host({},"root",{300,80}));
 good(holder->set_content([&](WidgetBuilder& b)->ii::core::Result<void>{auto v=View::create(b,{},"cell",input);if(!v)return std::unexpected(v.error());second=*v;return {};}));
 retire=true;auto changed=input;changed.configuration.text="different";auto result=second->set_options(changed);
 check(!retire&&!second->options(),"Reentrant retirement failed to invalidate the cell");tree2->close();}
