#pragma once
#include "AtSpiText.h"
#include "WidgetState.h"
#include <algorithm>
#include <optional>
#include <string>

namespace ii::gui::detail {
// Read on the widget owner thread. The provider may return bounded fragments;
// a failed endpoint may be a UTF-16 surrogate boundary, so retry one unit
// earlier. Never publish content if any observable state/revision changed.
[[nodiscard]] inline std::optional<AtSpiTextIndex> read_atspi_text(WidgetState& state,const AtSpiTextTarget& target){
    try{
        if(target.length>TextIndex::maximum_text_bytes)return std::nullopt;
        auto widget=state.find(target.id);if(!widget)return std::nullopt;
        auto before=widget->snapshot();if(!before||atspi_text_target(*before)!=target)return std::nullopt;
        std::string text;std::size_t offset=0;
        while(offset<target.length){
            auto amount=std::min<std::size_t>(65536,target.length-offset);
            auto part=widget->text_content({offset,amount});
            if(!part&&amount>1){--amount;part=widget->text_content({offset,amount});}
            if(!part||part->range.location!=offset||!part->range.length||part->range.length>amount||
               part->text.size()>TextIndex::maximum_text_bytes-text.size())return std::nullopt;
            text+=part->text;offset+=part->range.length;
        }
        auto after=widget->snapshot();if(!after||atspi_text_target(*after)!=target)return std::nullopt;
        auto index=AtSpiTextIndex::create(std::move(text),target.selection,target.reversed);
        if(!index||index->utf16_size()!=target.length)return std::nullopt;return index;
    }catch(...){return std::nullopt;}
}
} // namespace ii::gui::detail
