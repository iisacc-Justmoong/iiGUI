#pragma once
#include <iiGUI/ButtonModel.h>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
namespace ii::gui {
struct SelectorOption final {std::string text;bool enabled=true;friend bool operator==(const SelectorOption&,const SelectorOption&)=default;};
struct SelectorConfiguration final {std::vector<SelectorOption> options;int selected_index=0;std::string placeholder="Option";bool enabled=true;
 friend bool operator==(const SelectorConfiguration&,const SelectorConfiguration&)=default;};
struct DefaultListItemSelectorModelPolicy final {using Configuration=SelectorConfiguration;using Option=SelectorOption;
 [[nodiscard]] static bool valid_text(std::string_view value)noexcept{return DefaultButtonPolicy::valid_text(value);}};
// Selection is separate from the view that opens and presents its options.
template<class Policy>
class BasicListItemSelector final {
    using ListItemSelectorConfiguration=typename Policy::Configuration;using ListItemOption=typename Policy::Option;
public:
    [[nodiscard]] static bool validate_options(const std::vector<ListItemOption>& options,std::size_t bytes=0)noexcept{
        if(options.size()>400||bytes>1024*1024)return false;
        for(const auto& option:options){
            if(option.text.size()>8192||!Policy::valid_text(option.text)||option.text.size()>1024*1024-bytes)return false;
            bytes+=option.text.size();
        }
        return true;
    }
    [[nodiscard]] static bool validate(const ListItemSelectorConfiguration& value)noexcept{
        if(value.options.size()>400||value.selected_index<0||
           (!value.options.empty()&&std::size_t(value.selected_index)>=value.options.size())||
           (value.options.empty()&&value.selected_index!=0)||value.placeholder.size()>8192||!Policy::valid_text(value.placeholder))return false;
        return validate_options(value.options,value.placeholder.size());
    }
    bool set_configuration(ListItemSelectorConfiguration value){
        if(!validate(value))return false;configuration_=std::move(value);return true;
    }
    [[nodiscard]] const ListItemSelectorConfiguration& configuration()const noexcept{return configuration_;}
    [[nodiscard]] const std::string& display_text()const noexcept{
        return configuration_.options.empty()?configuration_.placeholder:configuration_.options[std::size_t(configuration_.selected_index)].text;
    }
    void release()noexcept{configuration_.options.clear();configuration_.placeholder.clear();}
    [[nodiscard]] std::optional<int> choose(int index)noexcept{
        auto& c=configuration_;if(!c.enabled||index<0||std::size_t(index)>=c.options.size()||!c.options[std::size_t(index)].enabled||index==c.selected_index)return std::nullopt;
        c.selected_index=index;return index;
    }
    [[nodiscard]] std::optional<int> navigate(int direction)noexcept{
        auto& c=configuration_;if(!c.enabled||(direction!=-1&&direction!=1)||c.options.empty())return std::nullopt;
        for(int index=c.selected_index+direction;index>=0&&std::size_t(index)<c.options.size();index+=direction)
            if(c.options[std::size_t(index)].enabled)return choose(index);
        return std::nullopt;
    }
private:
    ListItemSelectorConfiguration configuration_;
};

using ListItemSelectorModel=BasicListItemSelector<DefaultListItemSelectorModelPolicy>;
} // namespace ii::gui
