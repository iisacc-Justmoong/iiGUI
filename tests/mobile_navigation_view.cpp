#include <iiGUI/NativeMobileNavigationView.h>
#include <iiGUI/MobileNavigationModel.h>
#include <iiGUI/MobileNavigationMotion.h>
#include <iiGUI/WidgetState.h>
#include <iiSDKCore/Core/Text/Utf8.h>
#include <memory>
#include <optional>
#include <stdexcept>
#include <vector>
namespace {using namespace ii::gui;void check(bool v,const char* m){if(!v)throw std::runtime_error(m);}
template<class T>T take(ii::core::Result<T> r){if(!r)throw std::runtime_error(r.error().message);return std::move(*r);}void good(ii::core::Result<void> r){if(!r)throw std::runtime_error(r.error().message);}
enum class Platform{automatic,ios,android};struct Item{std::string key,text,icon_name,accessibility_name;bool enabled=true;std::string badge;bool badge_dot=false;std::string badge_accessibility_name;friend bool operator==(const Item&,const Item&)=default;};
struct Configuration{std::vector<Item> items;std::optional<Item> search;double width=402,bottom_safe_inset=0;int current_index=-1;bool auto_select=true,enabled=true,material_enabled=false,motion_enabled=true;Platform platform=Platform::automatic;WidgetColor focus_color{};std::string accessibility_name="Main";friend bool operator==(const Configuration&,const Configuration&)=default;};
struct Layout{double height=0,group_x=0,group_y=0,group_width=0,viewport_width=0,content_width=0,tab_width=0,search_x=0;};struct Geometry{double left=0,right=0,height=48;friend bool operator==(const Geometry&,const Geometry&)=default;};
struct Tokens{inline static constexpr double outer=16,gap=12,inset=4,group_height=56,tab_width=56,touch_size=48,ios_radius=28,ios_selected_radius=24,android_radius=12,android_selected_radius=8,icon_size=24,label_size=11,label_line_height=11,icon_label_gap=2,border_width=1,badge_dot=6,badge_height=16,badge_padding=4,badge_offset=2;};
struct ModelPolicy{using Configuration=::Configuration;using Item=::Item;using Layout=::Layout;using Platform=::Platform;inline static constexpr double outer=Tokens::outer,group_height=Tokens::group_height,gap=Tokens::gap,inset=Tokens::inset,tab_width=Tokens::tab_width,touch_size=Tokens::touch_size;static bool valid_text(std::string_view v){return v.find('\0')==v.npos&&ii::core::valid_utf8(v);}};
struct MotionPolicy{using Geometry=::Geometry;inline static constexpr double touch_size=48,stretch_duration=140,stretch_fraction=.75,stretch_amount=6,leading_forward=.15,trailing_forward=.88,overshoot=1.1;};
enum class Density{glass25};enum class Backdrop{previous_layers};struct MaterialConfig{Density density=Density::glass25;WidgetColor color,primary_color,border_color;double tint_opacity=1,radius=0,border_width=0,intense_opacity=0,faint_opacity=0,width=1,height=1;Backdrop backdrop=Backdrop::previous_layers;bool motion_enabled=false;};
struct Material{Widget w;static ii::core::Result<Material> create(WidgetBuilder& b,Widget parent,std::string_view id,MaterialConfig){auto v=b.absolute_stack(parent,id,{1,1});if(!v)return std::unexpected(v.error());return Material{*v};}Widget root()const{return w;}ii::core::Result<void> set_options(MaterialConfig){return {};}};
struct Cache{inline static std::vector<std::weak_ptr<int>> retained;std::shared_ptr<int> marker=std::make_shared<int>(1);Cache(){retained.push_back(marker);}ii::core::Result<std::shared_ptr<const RasterImage>> resolve(std::string_view,std::shared_ptr<const void>,double){return rgba_image(1,1,{255,255,255,255});}};
struct Callbacks{std::function<void(int)> activated;std::function<void()> search_requested;};
struct Colors{inline static constexpr WidgetColor panel_window_alt{},panel_surface_panel{},panel_surface_input_hover{},panel_surface_input{},text_disabled_color{},text_title_header_color{},text_body_color{},accent_red_default{},material_glass_fill{};};
struct Policy{using Model=BasicMobileNavigationModel<ModelPolicy>;using Motion=BasicMobileNavigationMotion<MotionPolicy>;using Configuration=::Configuration;using Item=::Item;using Platform=::Platform;using Color=WidgetColor;using Tokens=::Tokens;using Colors=::Colors;using Material=::Material;using MaterialConfiguration=MaterialConfig;using MaterialDensity=Density;using MaterialBackdrop=Backdrop;using IconCache=Cache;using Callbacks=::Callbacks;};
}
int main(){using View=ii::gui::BasicNativeMobileNavigation<Policy>;auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task t){t();});auto host=take(tree->content_host({},"host",{402,88}));Configuration c;c.items={{"a","Home","home","Home"},{"b","Tools","plus","Tools"}};c.current_index=0;auto owner=std::make_shared<int>(1);std::weak_ptr<int> weak=owner;std::optional<View> view;
 good(host.set_content([&](WidgetBuilder& b)->ii::core::Result<void>{Callbacks cb;cb.activated=[held=owner](int){};auto v=View::create(b,{},"tabs",c,std::move(cb));if(!v)return std::unexpected(v.error());view=*v;return {};}));owner.reset();(void)take(tree->drawing_snapshot({402,88}));good(host.clear_content());for(const auto& marker:Cache::retained)check(marker.expired(),"Retired navigation retained its icon cache");check(weak.expired()&&!view->set_options(c),"Retired navigation retained callback or accepted update");tree->close();}
