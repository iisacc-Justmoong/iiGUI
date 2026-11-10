#include <iiGUI/HierarchyItemModel.h>
#include <iiGUI/NativeWidgets.h>
#include <iiSDKCore/Core/Text/Utf8.h>
#include <optional>
#include <stdexcept>
#include <string>
namespace {using namespace ii::gui;void check(bool v,const char* m){if(!v)throw std::runtime_error(m);}
struct Palette{WidgetColor idle{},hover{},pressed{},active{},inactive{},drag{},text{},disabled_text{},focus{};};
struct Config{std::string key,label,icon_name,accessibility_name;std::optional<std::int64_t> id;std::int64_t count=-1;std::size_t depth=0;double width=200,height=20,indent_step=8,icon_size=18,chevron_size=16,left_padding=8,right_padding=8,leading_spacing=2,count_width=24,corner_radius=5;bool enabled=true,activatable=true,selected=false,expanded=false,show_chevron=true,has_children=false,drag_preview=false;Palette palette;};
struct Rect{double x=0,y=0,width=0,height=0;};struct Geometry{Rect surface,icon,label,count,chevron;bool show_icon=false,show_count=false,show_chevron=false;};
enum class Phase{idle,hover,pressed,active,inactive,drag};struct Interaction{bool hovered=false,pressed=false,focused=false;};struct Visual{Phase phase;WidgetColor background,text,focus;bool focused=false;};
struct Entry{std::string key,label,icon_name;std::optional<std::int64_t> id;std::int64_t count=-1;bool enabled=true,selectable=true;};struct Row{Entry entry;std::size_t depth=0,child_count=0;bool selected=false,expanded=false;std::optional<double> row_extent;};
struct Policy{using Configuration=Config;using Geometry=::Geometry;using Interaction=::Interaction;using Visual=::Visual;using Phase=::Phase;using Color=WidgetColor;inline static constexpr double count_gap=8;static bool valid_text(std::string_view v){return v.find('\0')==v.npos&&ii::core::valid_utf8(v);}};
}
int main(){using Item=ii::gui::BasicHierarchyItem<Policy>;Item m;Config c;c.key="camera";c.label="Camera";c.icon_name="home";c.depth=1;c.count=12;c.has_children=true;check(m.set_configuration(c),"Valid row rejected");auto g=m.geometry(200,20);check(g&&g->icon.x==16&&g->chevron.x==176,"Row geometry wrong");check(m.visual({.hovered=true}).phase==Phase::hover&&m.toggle_expansion()==true,"Row state failed");
 Row row;row.entry.key="child";row.depth=2;row.selected=true;row.row_extent=48.;auto projected=Item::project(row,c);check(projected&&projected->key=="child"&&projected->depth==2&&projected->height==48&&projected->selected&&!projected->has_children,"Row projection failed");}
