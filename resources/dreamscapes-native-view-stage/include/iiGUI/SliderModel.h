#pragma once
#include <iiGUI/NativeWidgets.h>
#include <iiSDKCore/Core/Text/Utf8.h>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
namespace ii::gui {
enum class SliderSize {mini,small,regular,large};
enum class SliderType {normal,center_biased,ticks,center_biased_ticks,filled,min_max_labels,segmented};
struct SliderPalette final {WidgetColor track{},fill{},thumb{},focus{},mark{},label{},symbol{},inactive_fill{};
 friend bool operator==(const SliderPalette&,const SliderPalette&)=default;};
struct SliderConfiguration final {double minimum=0,maximum=1,value=.5,step=0;
 double width=320,height=32,thumb_diameter=18,track_height=6,range_inset=9;
 bool enabled=true,show_ticks=false,show_symbol=false,show_min_max=false,show_labels=true,show_endpoint_icons=false,right_to_left=false;
 std::size_t tick_count=11,segment_count=5;bool automatic_segment_step=true;SliderPalette palette;
 std::string symbol_name="symbol",minimum_label="0%",maximum_label="100%",minimum_icon_name="symbol",maximum_icon_name="symbol",accessibility_name="Slider";
 bool center_biased=false;SliderType type=SliderType::normal;SliderSize size=SliderSize::regular;bool active=true;
 friend bool operator==(const SliderConfiguration&,const SliderConfiguration&)=default;};
struct SliderRect final {double x=0,y=0,width=0,height=0;};
struct SliderGeometry final {SliderRect track,fill,thumb,center_mark;};
struct SliderEndpointWidths final {double minimum=0,maximum=0;};
struct SliderEndpointLayout final {SliderRect control,minimum_label,maximum_label,minimum_icon,maximum_icon;};
struct DefaultSliderModelPolicy final {
 using Configuration=SliderConfiguration;using Type=SliderType;using Size=SliderSize;using Color=WidgetColor;
 using Rect=SliderRect;using Geometry=SliderGeometry;using EndpointWidths=SliderEndpointWidths;using EndpointLayout=SliderEndpointLayout;
 inline static constexpr double endpoint_minimum_control_width=18,endpoint_outer_gap=4,endpoint_inner_gap=4,endpoint_line_height=13;
 inline static constexpr double symbol_size=16,center_width=2,center_overhang=2;
 [[nodiscard]] static bool valid_text(std::string_view text)noexcept{return text.size()<=65536&&text.find('\0')==std::string_view::npos&&ii::core::valid_utf8(text);}
 [[nodiscard]] static bool has_filled_track(Type type)noexcept{return type==Type::filled||type==Type::min_max_labels||type==Type::segmented;}
};
// The framework owns range validation, selection, input mapping and geometry.
template<class Policy>
class BasicSlider final {
    using SliderConfiguration=typename Policy::Configuration;using SliderType=typename Policy::Type;using SliderSize=typename Policy::Size;
    using StyleColor=typename Policy::Color;using SliderRect=typename Policy::Rect;using SliderGeometry=typename Policy::Geometry;
    using SliderEndpointWidths=typename Policy::EndpointWidths;using SliderEndpointLayout=typename Policy::EndpointLayout;
    SliderConfiguration configuration_;
    [[nodiscard]] static double resolved_step(const SliderConfiguration& config)noexcept {
        if(config.type==SliderType::segmented&&config.automatic_segment_step&&config.step==0)
            return (config.maximum-config.minimum)/double(config.segment_count-1);
        return config.step;
    }
    [[nodiscard]] static double resolved_value(const SliderConfiguration& config,double raw)noexcept {
        auto value=std::clamp(raw,config.minimum,config.maximum);
        const auto step=resolved_step(config);
        if(step<=0||value<=config.minimum||value>=config.maximum)return value;
        const auto steps=(value-config.minimum)/step;
        // Above exact integer precision the step is smaller than the value's
        // representable spacing, so rounding cannot improve the result.
        if(!std::isfinite(steps)||steps>9007199254740991.)return value;
        return std::clamp(config.minimum+std::round(steps)*step,config.minimum,config.maximum);
    }
public:
    [[nodiscard]] static bool validate(const SliderConfiguration& value)noexcept {
        const auto range=[](double n,double low,double high){return std::isfinite(n)&&n>=low&&n<=high;};
        const auto color=[&](StyleColor c){return range(c.red,0,1)&&range(c.green,0,1)&&range(c.blue,0,1)&&range(c.alpha,0,1);};
        const auto icon=[](const std::string& name){return name.empty()||(name.size()<=256&&
            name.find_first_not_of(" \t\r\n")!=std::string::npos&&Policy::valid_text(name));};
        const auto valid_type=value.type==SliderType::normal||value.type==SliderType::center_biased||value.type==SliderType::ticks||
            value.type==SliderType::center_biased_ticks||value.type==SliderType::filled||
            value.type==SliderType::min_max_labels||value.type==SliderType::segmented;
        const auto valid_size=value.size==SliderSize::mini||value.size==SliderSize::small||value.size==SliderSize::regular||value.size==SliderSize::large;
        return valid_type&&valid_size&&range(value.minimum,-1e12,1e12)&&range(value.maximum,-1e12,1e12)&&value.minimum<value.maximum&&
            range(value.value,-1e12,1e12)&&range(value.step,0,32767)&&range(value.width,18,8192)&&
            range(value.height,1,8192)&&range(value.thumb_diameter,1,256)&&range(value.track_height,1,256)&&
            value.width>=value.thumb_diameter&&value.height>=value.thumb_diameter&&value.height>=value.track_height&&
            range(value.range_inset,0,128)&&value.tick_count>=2&&value.tick_count<=512&&
            value.segment_count>=2&&value.segment_count<=512&&color(value.palette.track)&&color(value.palette.fill)&&
            color(value.palette.thumb)&&color(value.palette.focus)&&color(value.palette.mark)&&color(value.palette.label)&&
            color(value.palette.symbol)&&color(value.palette.inactive_fill)&&Policy::valid_text(value.accessibility_name)&&
            !value.symbol_name.empty()&&value.symbol_name.size()<=256&&
            value.symbol_name.find_first_not_of(" \t\r\n")!=std::string::npos&&Policy::valid_text(value.symbol_name)&&
            Policy::valid_text(value.minimum_label)&&Policy::valid_text(value.maximum_label)&&
            icon(value.minimum_icon_name)&&icon(value.maximum_icon_name);
    }
    bool set_configuration(SliderConfiguration value) {
        if(!validate(value))return false;
        value.value=resolved_value(value,value.value);configuration_=std::move(value);return true;
    }
    [[nodiscard]] const SliderConfiguration& configuration()const noexcept{return configuration_;}
    [[nodiscard]] double value()const noexcept{return configuration_.value;}
    [[nodiscard]] double normalized()const noexcept{
        return (configuration_.value-configuration_.minimum)/(configuration_.maximum-configuration_.minimum);
    }
    bool set_value(double value)noexcept {
        if(!std::isfinite(value))return false;
        configuration_.value=resolved_value(configuration_,value);return true;
    }
    [[nodiscard]] std::optional<SliderEndpointLayout> endpoint_layout(double width,double height,SliderEndpointWidths labels={})const noexcept {
        if(!std::isfinite(width)||!std::isfinite(height)||width<0||height<0||width>8192||height>8192||
           !std::isfinite(labels.minimum)||!std::isfinite(labels.maximum)||labels.minimum<0||labels.maximum<0)return std::nullopt;
        SliderEndpointLayout result;result.control={0,0,width,height};
        const auto& c=configuration_;if(!c.show_min_max)return result;
        const auto reserve=(std::min)(width,(std::max)(Policy::endpoint_minimum_control_width,c.range_inset*2));
        const auto cap=(std::max)(0.,(width-reserve-2*Policy::endpoint_outer_gap)/2);
        struct Slot {double label=0,icon=0,gap=0,width=0;};
        const auto slot=[&](double measured,const std::string& label,const std::string& name){
            Slot value;
            if(c.show_endpoint_icons&&!name.empty()&&cap>=Policy::symbol_size)value.icon=Policy::symbol_size;
            const bool wants_label=c.show_labels&&!label.empty()&&measured>0;
            const auto gap=value.icon>0&&wants_label?Policy::endpoint_inner_gap:0.;
            if(wants_label)value.label=(std::min)(measured,(std::max)(0.,cap-value.icon-gap));
            if(value.icon>0&&value.label>0)value.gap=gap;
            value.width=value.icon+value.gap+value.label;return value;
        };
        const auto low=slot(labels.minimum,c.minimum_label,c.minimum_icon_name);
        const auto high=slot(labels.maximum,c.maximum_label,c.maximum_icon_name);
        const auto& left=c.right_to_left?high:low;const auto& right=c.right_to_left?low:high;
        const auto left_extent=left.width+(left.width>0?Policy::endpoint_outer_gap:0);
        const auto right_extent=right.width+(right.width>0?Policy::endpoint_outer_gap:0);
        result.control={left_extent,0,(std::max)(0.,width-left_extent-right_extent),height};
        const auto set=[&](const Slot& value,double x,SliderRect& text,SliderRect& icon){
            const auto label_x=x+(c.right_to_left?0:value.icon+value.gap);
            const auto icon_x=x+(c.right_to_left?value.label+value.gap:0);
            if(value.label>0)text={label_x,(height-Policy::endpoint_line_height)/2,value.label,Policy::endpoint_line_height};
            if(value.icon>0)icon={icon_x,(height-value.icon)/2,value.icon,value.icon};
        };
        set(low,c.right_to_left?width-low.width:0,result.minimum_label,result.minimum_icon);
        set(high,c.right_to_left?0:width-high.width,result.maximum_label,result.maximum_icon);
        return result;
    }
    bool set_from_position(double x,double width,SliderEndpointWidths labels={})noexcept {
        if(!configuration_.enabled||!std::isfinite(x)||!std::isfinite(width)||width<0||width>8192)return false;
        const auto layout=endpoint_layout(width,configuration_.height,labels);if(!layout)return false;
        const auto control_width=layout->control.width;
        const auto inset=(std::min)(configuration_.range_inset,control_width/2),length=control_width-2*inset;
        if(length<=0)return true;
        auto amount=std::clamp((x-layout->control.x-inset)/length,0.,1.);
        if(configuration_.right_to_left)amount=1-amount;
        return set_value(configuration_.minimum+amount*(configuration_.maximum-configuration_.minimum));
    }
    bool nudge(int direction)noexcept {
        if(!configuration_.enabled||direction==0)return false;
        const auto explicit_step=resolved_step(configuration_);
        const auto step=explicit_step>0?explicit_step:(configuration_.maximum-configuration_.minimum)/100;
        return set_value(configuration_.value+(direction>0?step:-step));
    }
    [[nodiscard]] std::optional<SliderGeometry> geometry(double width,double height,SliderEndpointWidths labels={})const noexcept {
        const auto layout=endpoint_layout(width,height,labels);if(!layout)return std::nullopt;
        const auto control_width=layout->control.width,origin=layout->control.x;
        const auto inset=(std::min)(configuration_.range_inset,control_width/2),length=(std::max)(0.,control_width-2*inset);
        const auto track_y=(height-configuration_.track_height)/2,thumb_y=(height-configuration_.thumb_diameter)/2;
        const auto amount=length*normalized();
        const auto position=configuration_.right_to_left?length-amount:amount;
        if(Policy::has_filled_track(configuration_.type)){
            const auto filled_width=(std::min)(control_width,amount+2*inset);
            const auto filled_x=configuration_.right_to_left?origin+control_width-filled_width:origin;
            return SliderGeometry{{origin,track_y,control_width,configuration_.track_height},
                {filled_x,track_y,filled_width,configuration_.track_height},
                {origin+inset+position-configuration_.thumb_diameter/2,thumb_y,configuration_.thumb_diameter,configuration_.thumb_diameter},{}};
        }
        const auto fill=configuration_.center_biased?std::abs(position-length/2):amount;
        const auto fill_x=configuration_.center_biased?origin+inset+(std::min)(position,length/2):
            configuration_.right_to_left?origin+inset+length-fill:origin+inset;
        SliderRect mark;
        if(configuration_.center_biased&&length>0){const auto mark_width=Policy::center_width;
            mark={origin+inset+length/2-mark_width/2,track_y-Policy::center_overhang,
                mark_width,configuration_.track_height+2*Policy::center_overhang};}
        return SliderGeometry{{origin+inset,track_y,length,configuration_.track_height},{fill_x,track_y,fill,configuration_.track_height},
            {origin+inset+position-configuration_.thumb_diameter/2,thumb_y,configuration_.thumb_diameter,configuration_.thumb_diameter},mark};
    }
};
using SliderModel=BasicSlider<DefaultSliderModelPolicy>;
} // namespace ii::gui
