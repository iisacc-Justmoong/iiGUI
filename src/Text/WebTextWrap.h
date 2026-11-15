#pragma once

#include <iiGUI/TextBoundary.h>
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace ii::gui::detail {
// Shared by browser measurement and drawing. Returned views borrow `text`.
// Break at whitespace where possible and otherwise at a Unicode grapheme;
// explicit CR, LF and CRLF delimit paragraphs, including empty paragraphs.
template<class Measure>
std::vector<std::string_view> web_text_wrap(std::string_view text,double width,bool soft_wrap,Measure&& measure){
    if(!std::isfinite(width))throw std::invalid_argument("Text width must be finite");
    auto index=TextIndex::create(std::string(text));
    if(!index)throw std::invalid_argument("Text wrapping requires valid bounded UTF-8");
    std::vector<std::string_view> result;
    const auto byte=[&](std::size_t unit){
        auto value=index->byte_for_utf16(unit);
        if(!value)throw std::logic_error("Text wrapping split a Unicode scalar");
        return *value;
    };
    const auto paragraph=[&](std::size_t first,std::size_t last){
        if(first==last){result.emplace_back(text.substr(byte(first),0));return;}
        if(!soft_wrap || width<=0){result.emplace_back(text.substr(byte(first),byte(last)-byte(first)));return;}
        std::vector<std::size_t> boundaries;
        for(auto unit=first;unit<last;unit=index->next_grapheme(unit))boundaries.push_back(byte(unit));
        boundaries.push_back(byte(last));
        const auto end=boundaries.size()-1;
        const auto whitespace=[&](std::size_t at){return at<end && boundaries[at+1]==boundaries[at]+1 &&
            (text[boundaries[at]]==' '||text[boundaries[at]]=='\t');};
        for(std::size_t begin=0;begin<end;){
            const auto remaining=end-begin;
            const auto fits=[&](std::size_t count){
                const auto part=text.substr(boundaries[begin],boundaries[begin+count]-boundaries[begin]);
                const double measured=measure(part);
                if(!std::isfinite(measured)||measured<0)throw std::runtime_error("Invalid browser text measurement");
                return measured<=width;
            };
            std::size_t fit=1;
            if(fits(1) && remaining>1){
                std::size_t probe=2;
                while(probe<remaining && fits(probe)){fit=probe;probe=std::min(remaining,probe*2);}
                if(probe==remaining && fits(probe))fit=remaining;
                else {
                    auto low=fit,high=probe;
                    while(low+1<high){const auto mid=low+(high-low)/2;if(fits(mid))low=mid;else high=mid;}
                    fit=low;
                }
            }
            auto finish=begin+fit;
            if(finish<end){
                // Trailing whitespace need not force an otherwise fitting word
                // onto the next line; retain it in the source slice.
                if(whitespace(finish)){while(finish<end&&whitespace(finish))++finish;}
                else for(auto candidate=finish;candidate>begin;--candidate)
                    if(whitespace(candidate-1)){finish=candidate;break;}
            }
            result.emplace_back(text.substr(boundaries[begin],boundaries[finish]-boundaries[begin]));
            begin=finish;
        }
    };
    std::size_t first=0,unit=0;
    while(unit<index->utf16_size()){
        const auto at=byte(unit);
        const auto next=index->next_grapheme(unit);
        if(text[at]=='\r'||text[at]=='\n'){paragraph(first,unit);first=next;}
        unit=next;
    }
    paragraph(first,unit);
    return result;
}
} // namespace ii::gui::detail
