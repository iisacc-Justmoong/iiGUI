#include <iiGUI/ColorPickerLayout.h>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>
namespace {
enum class Mode{wheel,grayscale,rgb,cmyk,hue_saturation,saturation_brightness};enum class Channel{hue,saturation,brightness,gray,red,green,blue,cyan,magenta,yellow,black};
enum class Kind{wheel,spectrum,hue_saturation,brightness_rail,saturation_brightness,hue_rail};
struct Rect{double x=0,y=0,width=0,height=0;};
struct Layout{double width=0,height=0;Kind surface_kind=Kind::wheel;std::optional<Kind> rail_kind;Rect surface,rail,divider,comparison,hex_label,hex_field,alpha_label,alpha_field,alpha_suffix,recent_label,eyedropper,color_space,cancel,apply;std::vector<std::pair<Channel,Rect>> channels;std::vector<Rect> recent;};
struct Configuration{double width=320,padding=12;Mode mode=Mode::wheel;bool show_recent_colors=true,show_actions=true;std::vector<int> recent_colors;};
struct Metrics{double section_gap=12,channel_height=22,channel_gap=4,plane_reserve=36,wheel_max=232,spectrum_height=184,rail_width=24,divider_height=1,values_height=28,comparison_width=40,label_leading=48,hex_label_width=25,hex_field_leading=81,field_top=3,hex_field_min=40,hex_field_reserve=187,alpha_label_reserve=98,alpha_label_width=10,alpha_field_reserve=80,alpha_field_width=56,suffix_reserve=16,suffix_width=16,recent_height=20,recent_label_width=40,recent_gap=8,recent_leading=48,action_height=22,action_icon_width=22,action_button_width=68,action_button_gap=8,color_space_leading=30,color_space_reserve=38;};
struct Policy{using Configuration=::Configuration;using Layout=::Layout;using Rect=::Rect;using Mode=::Mode;using Channel=::Channel;using Kind=::Kind;static Metrics metrics(){return {};}static bool valid(const Configuration& c){return c.width>=100&&c.padding<64;}};
}
int main(){using View=ii::gui::BasicColorPickerLayout<Policy>;Configuration c;auto a=View::compute(c);if(!a||a->channels.size()!=3||a->height!=461)throw std::runtime_error("Wheel layout failed");c.mode=Mode::hue_saturation;auto b=View::compute(c);if(!b||!b->rail_kind||b->surface.width!=260||b->height!=437)throw std::runtime_error("Plane layout failed");c.padding=64;if(View::compute(c))throw std::runtime_error("Invalid layout accepted");}
