#pragma once
#include <iiGUI/TextLineNative.h>
#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

namespace ii::gui::detail {
// Physical extents and UTF-16 ranges belong to the native paragraph, not to a
// substring; a wrapped line retains the same bidi and shaping context.
struct AndroidParagraphLine final {
    std::size_t first=0,last=0;
    double left=0,width=0,top=0,bottom=0,baseline=0;
};
class AndroidParagraphLayout {
public:
    virtual ~AndroidParagraphLayout()=default;
    virtual const std::vector<AndroidParagraphLine>& lines()const noexcept=0;
    virtual double primary(std::size_t position)const=0;
    virtual std::size_t hit(std::size_t line,double x)const=0;
    virtual bool rtl(std::size_t position)const=0;
    // Native coordinates, clipped to the requested visual line.
    virtual std::vector<TextLineSpan> selection(std::size_t line,std::size_t first,std::size_t last)const=0;
    virtual TextBitmap render(std::size_t line,double x,double width,double height,Color,double scale)const=0;
};
class AndroidParagraphShaper {
public:
    virtual ~AndroidParagraphShaper()=default;
    // nullopt selects one unwrapped line. The platform admits at most the
    // requested line budget and signals overflow rather than truncating.
    virtual std::shared_ptr<AndroidParagraphLayout> shape(std::optional<double> width,std::size_t maximum_lines)const=0;
};
namespace android_text_line_detail {
class Line final:public NativeTextLine {
    TextIndex display_;
    TextLineStyle style_;
    std::shared_ptr<AndroidParagraphShaper> shaper_;
    std::shared_ptr<AndroidParagraphLayout> layout_;
    std::size_t row_=0;
    bool fragment_=false;
    AndroidParagraphLine geometry_;
    TextLineMetrics metrics_;
    static void validate(const AndroidParagraphLine& g,std::size_t size){
        if(g.first>g.last||g.last>size||!std::isfinite(g.left)||!std::isfinite(g.width)||g.width<0||
           !std::isfinite(g.top)||!std::isfinite(g.bottom)||g.top<0||g.bottom<g.top||!std::isfinite(g.baseline))
            throw std::runtime_error("Android returned invalid shaped line geometry");
    }
    double normalized(double value)const{
        if(!std::isfinite(value))throw std::runtime_error("Android returned an invalid caret coordinate");
        return std::clamp(value-geometry_.left,0.,geometry_.width);
    }
    Line(TextIndex display,TextLineStyle style,std::shared_ptr<AndroidParagraphShaper> shaper,
         std::shared_ptr<AndroidParagraphLayout> layout,std::size_t row,bool fragment)
        :display_(std::move(display)),style_(std::move(style)),shaper_(std::move(shaper)),layout_(std::move(layout)),row_(row),fragment_(fragment){
        if(!shaper_||!layout_||row_>=layout_->lines().size())throw std::runtime_error("Android text line is unavailable");
        geometry_=layout_->lines()[row_];validate(geometry_,display_.utf16_size());
        const auto natural=geometry_.bottom-geometry_.top;
        const auto height=style_.line_height>0?style_.line_height:std::max(1.,natural);
        metrics_={geometry_.width,height,(height-natural)/2+geometry_.baseline-geometry_.top};
    }
public:
    static std::unique_ptr<NativeTextLine> make(TextIndex display,TextLineStyle style,std::shared_ptr<AndroidParagraphShaper> shaper){
        if(!shaper)throw std::invalid_argument("Android text shaper is unavailable");
        auto layout=shaper->shape(std::nullopt,1);
        if(!layout||layout->lines().size()!=1||layout->lines()[0].first!=0||layout->lines()[0].last!=display.utf16_size())
            throw std::runtime_error("Android unwrapped paragraph was truncated");
        return std::unique_ptr<NativeTextLine>(new Line(std::move(display),std::move(style),std::move(shaper),std::move(layout),0,false));
    }
    TextLineMetrics metrics()const override{return metrics_;}
    double caret(std::size_t position)const override{
        if(position>geometry_.last-geometry_.first)throw std::out_of_range("Android caret exceeds its line");
        const auto absolute=geometry_.first+position;
        // At a soft break Android attributes the shared offset to the next
        // row. Select the visual trailing edge of this row's final grapheme.
        if(absolute==geometry_.last&&absolute<display_.utf16_size()&&absolute>geometry_.first){
            const auto previous=std::max(geometry_.first,display_.previous_grapheme(absolute));
            auto spans=layout_->selection(row_,previous,absolute);
            if(!spans.empty()){
                const bool right_to_left=layout_->rtl(absolute-1);
                double edge=right_to_left?std::numeric_limits<double>::infinity():-std::numeric_limits<double>::infinity();
                for(const auto& span:spans){
                    if(!std::isfinite(span.x)||!std::isfinite(span.width)||span.width<0)
                        throw std::runtime_error("Android returned invalid selection geometry");
                    edge=right_to_left?std::min(edge,span.x):std::max(edge,span.x+span.width);
                }
                return normalized(edge);
            }
            return normalized(layout_->primary(previous));
        }
        return normalized(layout_->primary(absolute));
    }
    std::size_t hit(double x)const override{
        const auto absolute=layout_->hit(row_,geometry_.left+x);
        return std::clamp(absolute,geometry_.first,geometry_.last)-geometry_.first;
    }
    std::vector<TextLineSpan> selection(std::size_t first,std::size_t last)const override{
        if(first>last||last>geometry_.last-geometry_.first)throw std::out_of_range("Android selection exceeds its line");
        if(first==last)return {};
        auto spans=layout_->selection(row_,geometry_.first+first,geometry_.first+last);
        std::vector<TextLineSpan> result;result.reserve(spans.size());
        for(const auto& span:spans){
            if(!std::isfinite(span.x)||!std::isfinite(span.width)||span.width<0||!std::isfinite(span.x+span.width))
                throw std::runtime_error("Android returned invalid selection geometry");
            const auto a=std::clamp(span.x-geometry_.left,0.,geometry_.width);
            const auto b=std::clamp(span.x+span.width-geometry_.left,0.,geometry_.width);
            if(b>a)result.push_back({a,b-a});
        }
        return result;
    }
    TextBitmap render(double x,double width,Color color,double scale)const override{
        return layout_->render(row_,x,width,metrics_.height,color,scale);
    }
    std::vector<NativeLineSlice> wrap(double width,std::size_t maximum_lines)const override{
        if(fragment_)throw std::logic_error("A wrapped Android fragment cannot be rewrapped");
        auto wrapped=shaper_->shape(width,maximum_lines);
        if(!wrapped||wrapped->lines().empty())throw std::runtime_error("Android returned no wrapped lines");
        if(wrapped->lines().size()>maximum_lines)throw std::length_error("Wrapped Android paragraph exceeds its line budget");
        std::vector<NativeLineSlice> result;result.reserve(wrapped->lines().size());
        for(std::size_t i=0;i<wrapped->lines().size();++i){
            const auto& geometry=wrapped->lines()[i];validate(geometry,display_.utf16_size());
            result.push_back({geometry.first,geometry.last,std::unique_ptr<NativeTextLine>(new Line(display_,style_,shaper_,wrapped,i,true))});
        }
        return result;
    }
};
}
inline std::unique_ptr<NativeTextLine> android_text_line_model(TextIndex display,TextLineStyle style,
                                                               std::shared_ptr<AndroidParagraphShaper> shaper){
    return android_text_line_detail::Line::make(std::move(display),std::move(style),std::move(shaper));
}
} // namespace ii::gui::detail
