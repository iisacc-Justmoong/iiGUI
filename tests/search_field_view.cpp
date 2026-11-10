#include <iiGUI/NativeSearchFieldView.h>
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
struct Configuration{std::string text,placeholder,accessibility_name="Search",clear_accessibility_name="Clear";double width=200;bool enabled=true,read_only=false,clear_button_visible=true;input_field_test::Surface surface=input_field_test::Surface::rounded;friend bool operator==(const Configuration&,const Configuration&)=default;};
struct Callbacks{std::function<void(std::string)> edited;std::function<void()> submitted;};
struct Ids{std::string editor,search_icon,clear;};
struct Material{inline static std::weak_ptr<int> retained;std::shared_ptr<int> resource=std::make_shared<int>(1);Material(){retained=resource;} ii::core::Result<std::shared_ptr<const WidgetDrawing>> paint(const WidgetPaintState&,bool,double,input_field_test::MaterialVariant){return std::shared_ptr<const WidgetDrawing>(std::make_shared<WidgetDrawing>());}};
struct Cache{ii::core::Result<std::shared_ptr<const RasterImage>> resolve(std::string_view,std::shared_ptr<const void>,double){return rgba_image(1,1,{255,255,255,255});}};
struct Metrics{double height=30,icon=12,padding=4,vertical_padding=4,gap=4,radius=8,font_size=13,line_height=15,clear_line_height=13;WidgetColor background{},text{},disabled_text{},description{},accent{};std::string font;};
struct Policy{using Field=BasicNativeInputField<input_field_test::Policy>;using Configuration=::Configuration;using InputConfiguration=input_field_test::Configuration;using Surface=input_field_test::Surface;using Color=WidgetColor;using FieldCallbacks=input_field_test::Callbacks;using Callbacks=::Callbacks;using Ids=::Ids;using Snapshot=input_field_test::Snapshot;using Material=::Material;using MaterialVariant=input_field_test::MaterialVariant;using IconCache=Cache;using Metrics=::Metrics;
 static constexpr double cylinder_radius=15;inline static constexpr std::string_view icon_name="search";static Metrics metrics(){return {};}
 static bool valid_configuration(const Configuration& c){return c.width>=1&&c.surface!=Surface::flat;}
 static bool valid_text(std::string_view v){return DefaultTextInputModelPolicy::valid_text(v);}};
}
int main(){using View=ii::gui::BasicNativeSearchField<Policy>;auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task t){t();});
 auto host=take(tree->content_host({},"host",{300,80}));std::optional<View> view;auto owner=std::make_shared<int>(1);std::weak_ptr<int> weak=owner;int edits=0;Configuration c;c.text="query";
 good(host.set_content([&](WidgetBuilder& b)->ii::core::Result<void>{Callbacks callbacks;callbacks.edited=[&,keep=owner](std::string){++edits;};auto result=View::create(b,{},"search",c,std::move(callbacks));if(!result)return std::unexpected(result.error());view=*result;return {};}));owner.reset();
 check(take(view->snapshot()).text=="query","Search text missing");good(view->set_text("changed"));check(take(view->snapshot()).text=="changed","Search text update lost");
 auto invalid=c;invalid.width=0;check(!view->set_options(invalid)&&take(view->snapshot()).text=="changed","Invalid options replaced search state");
 good(host.clear_content());check(weak.expired()&&Material::retained.expired()&&!view->snapshot(),"Retired search retained callbacks, material or remained live");tree->close();}
