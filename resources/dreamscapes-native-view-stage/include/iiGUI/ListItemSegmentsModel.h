#pragma once
#include <iiGUI/ButtonModel.h>
#include <cmath>
#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
namespace ii::gui {
class ListItemSegmentCommand {public:virtual ~ListItemSegmentCommand()=default;virtual void invoke()const=0;};
struct ListItemSegment final {std::string text;bool enabled=true;std::shared_ptr<const ListItemSegmentCommand> command;
 friend bool operator==(const ListItemSegment&,const ListItemSegment&)=default;};
struct ListItemSegmentsConfiguration final {
 std::vector<ListItemSegment> options{{"Original",true,{}},{"Edited",true,{}}};int selected_index=0;bool enabled=true;
 double segment_width=56,segment_height=22,horizontal_padding=0,vertical_padding=0,spacing=2;
 friend bool operator==(const ListItemSegmentsConfiguration&,const ListItemSegmentsConfiguration&)=default;};
struct ListItemSegmentsSize final {double width=0,height=0;};
struct DefaultListItemSegmentsModelPolicy final {using Configuration=ListItemSegmentsConfiguration;using Segment=ListItemSegment;using Size=ListItemSegmentsSize;
 [[nodiscard]] static bool valid_text(std::string_view value)noexcept{return DefaultButtonPolicy::valid_text(value);}};
template<class Policy>
class BasicListItemSegments final {
    using ListItemSegmentsConfiguration=typename Policy::Configuration;
    using ListItemSegment=typename Policy::Segment;
    using ListItemSize=typename Policy::Size;
    ListItemSegmentsConfiguration configuration_;
public:
    [[nodiscard]] static bool validate_options(const std::vector<ListItemSegment>& options)noexcept{
        if(options.size()>400)return false;
        std::size_t bytes=0;for(const auto& option:options){
            if(option.text.size()>8192||!Policy::valid_text(option.text)||option.text.size()>1024*1024-bytes)return false;
            bytes+=option.text.size();
        }
        return true;
    }
    [[nodiscard]] static bool validate(const ListItemSegmentsConfiguration& c)noexcept{
        const auto bound=[](double value,double low,double high){return std::isfinite(value)&&value>=low&&value<=high;};
        if(!validate_options(c.options)||c.selected_index<0||(c.options.empty()?c.selected_index!=0:std::size_t(c.selected_index)>=c.options.size())||
           !bound(c.segment_width,1,8192)||!bound(c.segment_height,1,8192)||!bound(c.horizontal_padding,0,8192)||
           !bound(c.vertical_padding,0,8192)||!bound(c.spacing,0,8192))return false;
        const auto width=2*c.horizontal_padding+double(c.options.size())*c.segment_width+
            double(c.options.empty()?0:c.options.size()-1)*c.spacing;
        const auto height=2*c.vertical_padding+(c.options.empty()?0:c.segment_height);
        if(width>8192||height>8192)return false;
        return true;
    }
    [[nodiscard]] bool set_configuration(ListItemSegmentsConfiguration value){
        if(!validate(value))return false;configuration_=std::move(value);return true;
    }
    [[nodiscard]] const ListItemSegmentsConfiguration& configuration()const noexcept{return configuration_;}
    [[nodiscard]] ListItemSize intrinsic_size()const noexcept{
        const auto& c=configuration_;return {2*c.horizontal_padding+double(c.options.size())*c.segment_width+
            double(c.options.empty()?0:c.options.size()-1)*c.spacing,2*c.vertical_padding+(c.options.empty()?0:c.segment_height)};
    }
    void release()noexcept{configuration_.options.clear();}
    [[nodiscard]] std::optional<int> choose(int index)noexcept{
        auto& c=configuration_;if(!c.enabled||index<0||std::size_t(index)>=c.options.size()||
            !c.options[std::size_t(index)].enabled||index==c.selected_index)return std::nullopt;
        c.selected_index=index;return index;
    }
    [[nodiscard]] std::optional<int> navigate(int direction)noexcept{
        if(direction!=1&&direction!=-1)return std::nullopt;
        for(auto index=configuration_.selected_index+direction;index>=0&&std::size_t(index)<configuration_.options.size();index+=direction)
            if(configuration_.options[std::size_t(index)].enabled)return choose(index);
        return std::nullopt;
    }
};

using ListItemSegmentsModel=BasicListItemSegments<DefaultListItemSegmentsModelPolicy>;
} // namespace ii::gui
