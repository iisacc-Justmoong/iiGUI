#pragma once
#include <iiSDKCore/Core/Text/Utf8.h>
#include <iiGUI/TextBoundary.h>
#include <iiGUI/TextLine.h>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <exception>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace ii::gui {
struct NativeTextRange final { std::size_t location=0,length=0; };
struct NativeTextCaret final { double x=0,y=0,height=0; };
struct NativeTextSelectionRect final { double x=0,y=0,width=0,height=0; };
enum class NativeTextAffinity { upstream,downstream };
struct NativeTextHit final {std::size_t position=0;NativeTextAffinity affinity=NativeTextAffinity::downstream;};
struct NativeTextLayoutLine final {
    std::size_t first=0,last=0,next=0;
    double y=0,width=0,height=0;
};

// The graphics framework owns paragraph layout and selection geometry. Its generic line
// shaper supplies glyph metrics, bidirectional spans, hit testing and raster.
class NativeTextLayout final {
    ii::gui::TextIndex source_;
    std::vector<NativeTextLayoutLine> lines_;
    std::vector<ii::gui::TextLine> shaped_;
    // Empty for ordinary text; masked lines map each displayed bullet boundary
    // back to an original UTF-16 grapheme boundary.
    std::vector<std::vector<std::size_t>> masked_boundaries_;
    double width_=0,height_=0;
    [[nodiscard]] std::size_t line_for(std::size_t unit,NativeTextAffinity affinity=NativeTextAffinity::downstream) const noexcept {
        const auto it=std::upper_bound(lines_.begin(),lines_.end(),unit,
            [](std::size_t value,const auto& line){return value<line.first;});
        auto index=it==lines_.begin()?std::size_t(0):std::size_t(std::prev(it)-lines_.begin());
        if(affinity==NativeTextAffinity::upstream&&index>0&&lines_[index].first==unit&&
           lines_[index-1].last==unit&&lines_[index-1].next==unit)--index;
        return index;
    }
    [[nodiscard]] std::size_t display_position(std::size_t line,std::size_t unit,bool ceil=false)const noexcept {
        const auto& map=masked_boundaries_[line];
        if(map.empty())return unit-lines_[line].first;
        const auto it=std::lower_bound(map.begin(),map.end(),unit);
        if(it==map.end())return map.size()-1;
        const auto next=std::size_t(it-map.begin());
        if(*it==unit||next==0)return next;
        return ceil?next:next-1;
    }
public:
    static constexpr std::size_t maximum_bytes=64*1024;
    static constexpr std::size_t maximum_lines=4096;
    [[nodiscard]] static ii::core::Result<NativeTextLayout> create(std::string text,
            ii::gui::TextLineStyle style={},bool password=false,std::optional<double> wrap_width=std::nullopt) {
        if(text.size()>maximum_bytes||text.find('\0')!=std::string::npos||!ii::core::valid_utf8(text))
            return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Text layout requires bounded NUL-free UTF-8");
        if(wrap_width&&(!std::isfinite(*wrap_width)||*wrap_width<=0||*wrap_width>8192))
            return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid automatic wrapping width");
        std::size_t paragraphs=1;
        for(std::size_t i=0;i<text.size();++i)if(text[i]=='\r'||text[i]=='\n'){
            if(++paragraphs>maximum_lines)return ii::core::fail(ii::core::ErrorCode::out_of_range,"Text layout exceeds 4096 paragraphs");
            if(text[i]=='\r'&&i+1<text.size()&&text[i+1]=='\n')++i;
        }
        try {
            auto source=ii::gui::TextIndex::create(std::move(text));if(!source)return std::unexpected(source.error());
            NativeTextLayout result;result.source_=std::move(*source);
            const auto input=result.source_.text();
            std::size_t start_byte=0;
            while(true){
                if(result.lines_.size()==maximum_lines){
                    if(wrap_width)return ii::core::fail(ii::core::ErrorCode::full,"Text layout exceeds 4096 visual lines");
                    return ii::core::fail(ii::core::ErrorCode::out_of_range,"Text layout exceeds 4096 paragraphs");
                }
                const auto found=input.find_first_of("\r\n",start_byte);
                const auto end_byte=found==input.npos?input.size():found;
                auto next_byte=end_byte;
                if(found!=input.npos){++next_byte;if(input[found]=='\r'&&next_byte<input.size()&&input[next_byte]=='\n')++next_byte;}
                const auto first=result.source_.utf16_for_byte(start_byte),last=result.source_.utf16_for_byte(end_byte),next=result.source_.utf16_for_byte(next_byte);
                if(!first||!last||!next)return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Paragraph splits a Unicode scalar");
                std::string display;std::vector<std::size_t> mask;
                if(password){
                    display.reserve((*last-*first)*3);mask.push_back(*first);
                    for(auto i=*first;i<*last;){
                        const auto after=result.source_.next_grapheme(i);
                        if(after<=i||after>*last)return ii::core::fail(ii::core::ErrorCode::failed,"Password grapheme boundary is invalid");
                        display.append("\xe2\x80\xa2");mask.push_back(after);i=after;
                    }
                }else display.assign(input.substr(start_byte,end_byte-start_byte));
                auto line_index=ii::gui::TextIndex::create(std::move(display));if(!line_index)return std::unexpected(line_index.error());
                const auto display_length=line_index->utf16_size();
                auto shaped=ii::gui::TextLine::create(std::move(*line_index),style);if(!shaped)return std::unexpected(shaped.error());
                std::vector<ii::gui::TextLineSlice> slices;
                if(wrap_width){
                    auto wrapped=shaped->wrap(*wrap_width,maximum_lines-result.lines_.size());
                    if(!wrapped)return std::unexpected(wrapped.error());slices=std::move(*wrapped);
                }else slices.push_back({0,display_length,std::move(*shaped)});
                for(auto& slice:slices){
                    if(result.lines_.size()==maximum_lines)
                        return ii::core::fail(ii::core::ErrorCode::full,"Text layout exceeds 4096 visual lines");
                    const auto begin=password?mask.at(slice.first):*first+slice.first;
                    const auto end=password?mask.at(slice.last):*first+slice.last;
                    auto metrics=slice.line.metrics();if(!metrics)return std::unexpected(metrics.error());
                    result.lines_.push_back({begin,end,slice.last==display_length?*next:end,result.height_,metrics->width,metrics->height});
                    result.width_=(std::max)(result.width_,metrics->width);result.height_+=metrics->height;
                    result.shaped_.push_back(std::move(slice.line));
                    if(password)result.masked_boundaries_.emplace_back(mask.begin()+slice.first,mask.begin()+slice.last+1);
                    else result.masked_boundaries_.emplace_back();
                }
                if(found==input.npos)break;
                start_byte=next_byte;
            }
            return result;
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Text layout failed",std::current_exception());}
    }
    [[nodiscard]] std::size_t length()const noexcept{return source_.utf16_size();}
    [[nodiscard]] double width()const noexcept{return width_;}
    [[nodiscard]] double height()const noexcept{return height_;}
    [[nodiscard]] const std::vector<NativeTextLayoutLine>& lines()const noexcept{return lines_;}
    [[nodiscard]] std::string_view display_text(std::size_t line)const noexcept {
        return line<shaped_.size()?shaped_[line].index().text():std::string_view{};
    }
    [[nodiscard]] ii::core::Result<std::size_t> line_at(std::size_t unit,NativeTextAffinity affinity=NativeTextAffinity::downstream)const {
        if(!source_.byte_for_utf16(unit))return ii::core::fail(ii::core::ErrorCode::out_of_range,"Caret splits a Unicode scalar");
        return line_for(unit,affinity);
    }
    [[nodiscard]] bool soft_boundary(std::size_t unit)const noexcept {
        if(unit>length())return false;
        return line_for(unit,NativeTextAffinity::upstream)!=line_for(unit,NativeTextAffinity::downstream);
    }
    [[nodiscard]] ii::core::Result<NativeTextCaret> caret(std::size_t unit,NativeTextAffinity affinity=NativeTextAffinity::downstream)const {
        if(!source_.byte_for_utf16(unit))return ii::core::fail(ii::core::ErrorCode::out_of_range,"Caret splits a Unicode scalar");
        const auto i=line_for(unit,affinity);const auto& line=lines_[i];
        auto x=shaped_[i].caret(display_position(i,(std::min)(unit,line.last)));if(!x)return std::unexpected(x.error());
        return NativeTextCaret{*x,line.y,line.height};
    }
    [[nodiscard]] ii::core::Result<NativeTextHit> hit_test_position(double x,double y)const {
        if(!std::isfinite(x)||!std::isfinite(y))return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Text hit position must be finite");
        const auto it=std::upper_bound(lines_.begin(),lines_.end(),y,
            [](double value,const auto& line){return value<line.y;});
        const auto i=it==lines_.begin()?std::size_t(0):std::size_t(std::prev(it)-lines_.begin());
        const auto& line=lines_[i];auto hit=shaped_[i].hit_test(x);if(!hit)return std::unexpected(hit.error());
        const auto& map=masked_boundaries_[i];
        const auto unit=map.empty()?line.first+(std::min)(*hit,line.last-line.first):map[(std::min)(*hit,map.size()-1)];
        const bool upstream=unit==line.last&&i+1<lines_.size()&&line.next==line.last&&lines_[i+1].first==unit;
        return NativeTextHit{unit,upstream?NativeTextAffinity::upstream:NativeTextAffinity::downstream};
    }
    [[nodiscard]] ii::core::Result<std::size_t> hit_test(double x,double y)const {
        auto result=hit_test_position(x,y);if(!result)return std::unexpected(result.error());return result->position;
    }
    [[nodiscard]] ii::core::Result<std::vector<NativeTextSelectionRect>> selection(NativeTextRange range)const {
        if(range.location>length()||range.length>length()-range.location||
           !source_.byte_for_utf16(range.location)||!source_.byte_for_utf16(range.location+range.length))
            return ii::core::fail(ii::core::ErrorCode::out_of_range,"Selection splits a Unicode scalar");
        try {
            std::vector<NativeTextSelectionRect> result;const auto end=range.location+range.length;
            for(std::size_t i=0;i<lines_.size();++i){
                const auto& line=lines_[i];const auto first=(std::max)(range.location,line.first),last=(std::min)(end,line.last);
                if(first<last){auto spans=shaped_[i].selection(display_position(i,first),display_position(i,last,true));if(!spans)return std::unexpected(spans.error());
                    for(auto span:*spans)if(span.width>0)result.push_back({span.x,line.y,span.width,line.height});}
                if(line.next>line.last&&range.location<line.next&&end>line.last){
                    auto edge=shaped_[i].caret(display_position(i,line.last));if(!edge)return std::unexpected(edge.error());
                    result.push_back({*edge,line.y,3,line.height});
                }
            }
            return result;
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Text selection layout failed",std::current_exception());}
    }
    template<class Range> requires requires(const Range& value) { value.location; value.length; }
    [[nodiscard]] ii::core::Result<std::vector<NativeTextSelectionRect>> selection(const Range& range)const {
        return selection(NativeTextRange{range.location,range.length});
    }
    [[nodiscard]] ii::core::Result<ii::gui::TextBitmap> render_line(std::size_t line,double offset,double width,
            ii::gui::Color color,double scale=1)const {
        if(line>=shaped_.size())return ii::core::fail(ii::core::ErrorCode::out_of_range,"Text paragraph does not exist");
        return shaped_[line].render(offset,width,color,scale);
    }
};
} // namespace ii::gui
