#pragma once
#include <iiGUI/TextBoundary.h>
#include <iiGUI/NativeWidgets.h>
#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace ii::gui::detail {
struct AtSpiTextTarget final {
    std::string id;
    std::size_t length=0;
    TextRange selection;
    bool reversed=false,focused=false,enabled=false,read_only=false;
    TextInputRevision revision;
    friend bool operator==(const AtSpiTextTarget&,const AtSpiTextTarget&)=default;
};
[[nodiscard]] inline std::optional<AtSpiTextTarget> atspi_text_target(const WidgetSnapshot& value){
    if(!value.text_input||value.password||!value.text_revision)return std::nullopt;
    return AtSpiTextTarget{value.id,value.text_length,value.text_selection,value.text_selection_reversed,
        value.focused,value.enabled,value.read_only,*value.text_revision};
}
// AT-SPI offsets count Unicode scalar values; iiGUI editor ranges use UTF-16.
// Sparse checkpoints keep ASCII-heavy documents bounded without a word-sized
// entry per character. TextIndex owns and validates the UTF-8 content.
class AtSpiTextIndex final {
    struct Mark final {std::size_t byte=0,units=0,scalar=0;};
    static constexpr std::size_t stride=256;
    TextIndex text_;
    std::vector<Mark> marks_;
    std::size_t count_=0;
    TextRange selection_{};
    bool reversed_=false;

    [[nodiscard]] static std::size_t width(unsigned char lead)noexcept{
        return lead<0x80?1:lead<0xe0?2:lead<0xf0?3:4;
    }
    static void advance(Mark& mark,std::string_view bytes)noexcept{
        const auto length=width(static_cast<unsigned char>(bytes[mark.byte]));mark.byte+=length;
        mark.units+=length==4?2:1;++mark.scalar;
    }
    [[nodiscard]] std::optional<Mark> locate(std::size_t scalar)const noexcept{
        if(scalar>count_)return std::nullopt;
        auto position=marks_[scalar/stride];auto bytes=text_.text();
        while(position.scalar<scalar)advance(position,bytes);
        return position;
    }
public:
    [[nodiscard]] static std::optional<AtSpiTextIndex> create(std::string text,TextRange selection,bool reversed){
        if(text.find('\0')!=std::string::npos||(reversed&&!selection.length))return std::nullopt;
        auto checked=TextIndex::create(std::move(text));if(!checked)return std::nullopt;
        if(selection.location>checked->utf16_size()||selection.length>checked->utf16_size()-selection.location||
           !checked->byte_for_utf16(selection.location)||!checked->byte_for_utf16(selection.location+selection.length))return std::nullopt;
        AtSpiTextIndex result;result.text_=std::move(*checked);result.selection_=selection;result.reversed_=reversed;
        Mark position{};const auto bytes=result.text_.text();result.marks_.push_back(position);
        while(position.byte<bytes.size()){
            advance(position,bytes);
            if(position.scalar%stride==0)result.marks_.push_back(position);
        }
        result.count_=position.scalar;return result;
    }
    [[nodiscard]] std::size_t characters()const noexcept{return count_;}
    [[nodiscard]] std::size_t utf16_size()const noexcept{return text_.utf16_size();}
    [[nodiscard]] std::string_view text()const noexcept{return text_.text();}
    [[nodiscard]] std::size_t storage_bytes()const noexcept{return text_.storage_bytes()+marks_.capacity()*sizeof(Mark);}
    [[nodiscard]] std::optional<std::size_t> utf16_at(std::size_t scalar)const noexcept{
        auto position=locate(scalar);return position?std::optional{position->units}:std::nullopt;
    }
    [[nodiscard]] std::optional<std::size_t> scalar_at_utf16(std::size_t offset)const noexcept{
        if(offset>text_.utf16_size())return std::nullopt;
        auto found=std::upper_bound(marks_.begin(),marks_.end(),offset,[](std::size_t value,const Mark& mark){return value<mark.units;});
        auto position=*std::prev(found);auto bytes=text_.text();
        while(position.units<offset&&position.byte<bytes.size())advance(position,bytes);
        return position.units==offset?std::optional{position.scalar}:std::nullopt;
    }
    [[nodiscard]] TextRange selection()const noexcept{
        const auto first=*scalar_at_utf16(selection_.location),last=*scalar_at_utf16(selection_.location+selection_.length);
        return {first,last-first};
    }
    [[nodiscard]] std::size_t caret()const noexcept{const auto range=selection();return reversed_?range.location:range.location+range.length;}
    [[nodiscard]] std::optional<std::string_view> slice(std::size_t start,std::size_t end)const noexcept{
        if(start>end)return std::nullopt;const auto first=locate(start),last=locate(end);if(!first||!last)return std::nullopt;
        return text_.text().substr(first->byte,last->byte-first->byte);
    }
    [[nodiscard]] std::optional<std::uint32_t> character(std::size_t scalar)const noexcept{
        if(scalar>=count_)return std::nullopt;const auto position=*locate(scalar);const auto bytes=text_.text();
        const auto lead=static_cast<unsigned char>(bytes[position.byte]);const auto size=width(lead);
        std::uint32_t value=size==1?lead:lead&((1u<<(7-size))-1u);
        for(std::size_t i=1;i<size;++i)value=(value<<6)|(static_cast<unsigned char>(bytes[position.byte+i])&0x3fu);
        return value;
    }
};
struct AtSpiTextData final {AtSpiTextTarget target;AtSpiTextIndex index;};
} // namespace ii::gui::detail
