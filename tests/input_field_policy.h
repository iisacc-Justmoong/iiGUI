#pragma once
#include <iiGUI/NativeInputFieldView.h>
namespace input_field_test {
using namespace ii::gui;
struct Palette{WidgetColor background{},hover{},text{},placeholder{},selection{},caret{},focus{};friend bool operator==(const Palette&,const Palette&)=default;};
enum class Surface{flat,rounded,cylinder,inline_};
struct Configuration{std::string text,placeholder,accessibility_name="Text field",font_family,clear_accessibility_name="Clear";
 double width=206,height=30,horizontal_padding=8,vertical_padding=4,corner_radius=8,font_size=13,line_height=0;
 bool enabled=true,read_only=false,multiline=false,password=false,wrap_enabled=true,clear_button_visible=false;
 Surface surface=Surface::flat;std::optional<std::size_t> maximum_length;TextEditLengthUnit length_unit=TextEditLengthUnit::utf16_code_units;
 Palette palette;double double_click_interval=.5,double_click_distance=4;bool use_native_click_count=true,reveal_caret_in_ancestors=false;
 friend bool operator==(const Configuration&,const Configuration&)=default;};
struct Callbacks{std::function<void(std::string)> edited;std::function<void()> submitted;std::function<void(bool)> focus_changed;
 std::function<void(std::string)> displayed_changed;std::function<void(double)> content_height_changed;};
struct Clipboard{std::function<ii::core::Result<void>(std::string)> write;};
struct Snapshot{std::string text;TextEditRange selection;std::optional<TextEditRange> marked;std::size_t anchor=0,caret=0;
 double horizontal_scroll=0,vertical_scroll=0;NativeTextAffinity affinity=NativeTextAffinity::downstream;bool wrapping_fallback=false;double content_height=0;};
enum class MaterialVariant{standard,inline_};
struct Material{ii::core::Result<std::shared_ptr<const WidgetDrawing>> paint(const WidgetPaintState&,bool,double,MaterialVariant){
 return ii::core::fail(ii::core::ErrorCode::failed,"No material supplied");}};
struct Defaults{static constexpr double icon_size=14,gap=4,cylinder_radius=16,radius=8,font_size=13,clear_line_height=13;
 static constexpr WidgetColor text{},disabled_text{},description{},background{};};
struct Policy{using Configuration=input_field_test::Configuration;using Palette=input_field_test::Palette;using Surface=input_field_test::Surface;
 using Color=WidgetColor;using TextInput=TextInputModel;using Range=TextEditRange;using Error=TextEditError;using ErrorCode=TextEditErrorCode;
 using Callbacks=input_field_test::Callbacks;using Clipboard=input_field_test::Clipboard;using Snapshot=input_field_test::Snapshot;
 using Material=input_field_test::Material;using MaterialVariant=input_field_test::MaterialVariant;using Defaults=input_field_test::Defaults;
 static bool valid_configuration(const Configuration& c){return c.width>=1&&c.height>=1&&c.surface==Surface::flat&&
  DefaultTextInputModelPolicy::valid_text(c.text)&&DefaultTextInputModelPolicy::valid_text(c.accessibility_name);}
 static bool valid_text(std::string_view s){return DefaultTextInputModelPolicy::valid_text(s);}
 template<class Index>static auto boundary(std::string_view s){return make_text_input_boundary(s);}
 static std::size_t replacement_cursor(std::size_t start,std::size_t end,std::size_t length,std::int32_t offset){
  start=(std::min)(start,length);end=(std::min)((std::max)(start,end),length);
  if(offset>0){const auto n=static_cast<std::uint64_t>(offset)-1;return n>=length-end?length:end+static_cast<std::size_t>(n);}
  const auto n=static_cast<std::uint64_t>(-static_cast<std::int64_t>(offset));return n>=start?0:start-static_cast<std::size_t>(n);}
};
}
