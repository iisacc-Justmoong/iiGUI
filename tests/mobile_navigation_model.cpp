#include <iiGUI/MobileNavigationModel.h>
#include <iiGUI/MobileNavigationMotion.h>
#include <iiGUI/NativeWidgets.h>
#include <iiSDKCore/Core/Text/Utf8.h>
#include <chrono>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>
namespace {using namespace ii::gui;using namespace std::chrono_literals;
void check(bool v,const char* m){if(!v)throw std::runtime_error(m);}
enum class Platform{automatic,ios,android};struct Item{std::string key,text,icon_name,accessibility_name;bool enabled=true;std::string badge;bool badge_dot=false;std::string badge_accessibility_name;};
struct Configuration{std::vector<Item> items;std::optional<Item> search;double width=402,bottom_safe_inset=0;int current_index=-1;bool auto_select=true,enabled=true,material_enabled=false,motion_enabled=true;Platform platform=Platform::automatic;WidgetColor focus_color{};std::string accessibility_name="Main";};
struct Layout{double height=0,group_x=0,group_y=0,group_width=0,viewport_width=0,content_width=0,tab_width=0,search_x=0;};
struct Geometry{double left=0,right=0,height=48;friend bool operator==(const Geometry&,const Geometry&)=default;};
struct Policy{using Configuration=::Configuration;using Item=::Item;using Layout=::Layout;using Platform=::Platform;inline static constexpr double outer=8,group_height=72,gap=4,inset=4,tab_width=56,touch_size=48;static bool valid_text(std::string_view v){return v.find('\0')==v.npos&&ii::core::valid_utf8(v);}};
struct MotionPolicy{using Geometry=::Geometry;inline static constexpr double touch_size=48,stretch_duration=140,stretch_fraction=.75,stretch_amount=6,leading_forward=.15,trailing_forward=.88,overshoot=1.1;};
}
int main(){using Model=ii::gui::BasicMobileNavigationModel<Policy>;Configuration c;c.items={{"a","Home","home","Home"},{"b","Tools","plus","Tools"}};c.search=Item{"search","","search","Search"};c.current_index=0;check(Model::validate(c),"Mobile options rejected");auto g=Model::layout(c,402);check(g.tab_width==56&&g.content_width==112,"Mobile layout failed");c.items[1].key="a";check(!Model::validate(c),"Duplicate tab accepted");
 ii::gui::BasicMobileNavigationMotion<MotionPolicy> motion;auto now=decltype(motion)::TimePoint{};check(motion.reset(0,56)&&motion.retarget(2,56,true,360ms,now)&&motion.active(),"Motion did not start");check(motion.advance(now+140ms)&&!motion.advance(now+360ms)&&motion.geometry().left==112,"Motion frame timing failed");}
