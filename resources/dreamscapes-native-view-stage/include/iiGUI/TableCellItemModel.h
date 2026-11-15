#pragma once
#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <memory>
#include <locale>
#include <sstream>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
namespace ii::gui {
template<class Policy>class BasicTableCellItem final {
    using TableCellItemConfiguration=typename Policy::Configuration;using TableCellItemGeometry=typename Policy::Geometry;
    using TableCellItemVisual=typename Policy::Visual;using StyleColor=typename Policy::Color;
    using TableCellValueValidator=typename Policy::Validator;using Result=typename Policy::Result;
public:
    [[nodiscard]] static bool validate(const TableCellItemConfiguration& c)noexcept {
        const auto range=[](double n,double low,double high){return std::isfinite(n)&&n>=low&&n<=high;};
        const auto color=[&](StyleColor v){return range(v.red,0,1)&&range(v.green,0,1)&&range(v.blue,0,1)&&range(v.alpha,0,1);};
        const auto& p=c.palette;
        return Policy::valid_text(c.text)&&Policy::valid_text(c.accessibility_name)&&!c.value_type.empty()&&
            c.value_type.size()<=128&&Policy::valid_text(c.value_type)&&range(c.width,1,8192)&&range(c.height,1,8192)&&
            range(c.padding,0,8192)&&range(c.font_size,1,256)&&range(c.current_border_width,0,8192)&&
            color(p.background)&&color(p.divider)&&color(p.header_text)&&color(p.cell_text)&&color(p.selection)&&color(p.current);
    }
    bool set_configuration(TableCellItemConfiguration value){if(!validate(value))return false;configuration_=std::move(value);return true;}
    [[nodiscard]] const TableCellItemConfiguration& configuration()const noexcept{return configuration_;}
    [[nodiscard]] std::optional<TableCellItemGeometry> geometry(double width,double height)const noexcept{
        if(!std::isfinite(width)||!std::isfinite(height)||width<0||height<0||width>8192||height>8192)return std::nullopt;
        TableCellItemGeometry result;result.surface={0,0,width,height};const auto padding=(std::min)(configuration_.padding,width/2);
        result.text={padding,0,(std::max)(0.,width-2*padding),height};
        if(configuration_.show_divider&&width>0&&height>0){const auto edge=(std::min)(1.,width);result.divider={0,0,edge,height};}
        return result;
    }
    [[nodiscard]] TableCellItemVisual visual()const noexcept{
        const auto& c=configuration_;return {c.selected,c.current,c.palette.cell_text,c.palette.selection,c.palette.current,c.palette.divider};
    }
    [[nodiscard]] Result accept(std::string_view text,std::shared_ptr<const TableCellValueValidator> validator={})const{
        const auto reject=[](std::string message)->Result{return Policy::reject(std::move(message));};
        if(!Policy::valid_text(text))return reject("Invalid table cell text");
        if(validator)try{return validator->validate(text,configuration_.value_type);}catch(...){return reject("Table cell validator failed");}
        const auto& type=configuration_.value_type;
        if(type=="text"||type=="string")return {};
        if(type=="bool"||type=="boolean")return text=="true"||text=="false"?Result{}:reject("Expected true or false");
        if(type=="int"||type=="integer"){
            std::int64_t value=0;const auto [end,error]=std::from_chars(text.data(),text.data()+text.size(),value);
            return !text.empty()&&error==std::errc{}&&end==text.data()+text.size()?Result{}:reject("Expected a signed integer");
        }
        if(type=="number"||type=="double"||type=="float"){
            std::istringstream stream{std::string(text)};stream.imbue(std::locale::classic());double value=0;
            stream >> std::noskipws >> value;
            const bool complete=bool(stream)&&stream.rdbuf()->sgetc()==std::char_traits<char>::eof();
            return !text.empty()&&complete&&std::isfinite(value)?Result{}:reject("Expected a finite number");
        }
        return reject("A custom table cell type requires a validator");
    }
private:
    TableCellItemConfiguration configuration_;
};

} // namespace ii::gui
