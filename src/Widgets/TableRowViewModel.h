#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <optional>
#include <string_view>
#include <unordered_set>
#include <utility>
namespace ii::gui {
template<class Policy>class BasicTableRowView final {
    using TableRowViewConfiguration=typename Policy::Configuration;using TableCellItemConfiguration=typename Policy::CellConfiguration;
    using TableCellItem=typename Policy::CellModel;using TableRowFrame=typename Policy::Frame;
public:
    [[nodiscard]] static bool validate(const TableRowViewConfiguration& c){
        if(c.cells.size()>1024)return false;
        TableCellItemConfiguration base;base.width=c.cell_width;base.height=c.cell_height;base.padding=c.content_spacing;base.font_size=c.font_size;
        base.palette=c.palette;base.enabled=c.enabled;base.inputable=c.inputable;base.visible=c.visible;base.show_divider=c.show_divider;
        if(!std::isfinite(c.width)||c.width<1||c.width>8192||!TableCellItem::validate(base))return false;
        std::unordered_set<std::string_view> keys;keys.reserve(c.cells.size());std::size_t bytes=0;
        for(const auto& cell:c.cells){
            if(cell.key.empty()||cell.key.size()>1024||!Policy::valid_text(cell.key)||!keys.insert(cell.key).second)return false;
            if(cell.value.text.size()>4*1024*1024-bytes)return false;bytes+=cell.value.text.size();
            auto item=base;item.text=cell.value.text;item.value_type=cell.value.value_type;
            if(!TableCellItem::validate(item))return false;
        }
        return true;
    }
    bool set_configuration(TableRowViewConfiguration value){if(!validate(value))return false;configuration_=std::move(value);return true;}
    [[nodiscard]] const TableRowViewConfiguration& configuration()const noexcept{return configuration_;}
    [[nodiscard]] std::optional<TableRowFrame> layout(double width,double offset)const{
        if(!std::isfinite(width)||width<1||width>8192||!std::isfinite(offset)||offset<0||offset>1e15)return std::nullopt;
        const auto& c=configuration_;const auto count=c.cells.size();TableRowFrame frame;frame.width=width;frame.height=c.cell_height;
        frame.spacing=count>1?(std::max)(0.,(width-double(count)*c.cell_width)/double(count-1)):0;
        frame.content_width=count?double(count)*c.cell_width+double(count-1)*frame.spacing:0;
        frame.offset=std::clamp(offset,0.,(std::max)(0.,frame.content_width-width));
        frame.cells.reserve(count);
        for(std::size_t i=0;i<count;++i){const auto x=double(i)*(c.cell_width+frame.spacing)-frame.offset;
            if(x+c.cell_width>0&&x<width)frame.cells.push_back({i,x,c.cell_width});}
        return frame;
    }
private:
    TableRowViewConfiguration configuration_;
};

} // namespace ii::gui
