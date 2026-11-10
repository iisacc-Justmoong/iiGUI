#include "../src/Text/AndroidTextLineModel.h"
#include <cmath>
#include <memory>
#include <stdexcept>
#include <vector>
using namespace ii;
using namespace ii::gui;
using namespace ii::gui::detail;
namespace {
void check(bool value,const char* why){if(!value)throw std::runtime_error(why);}
bool near(double a,double b){return std::abs(a-b)<.001;}
struct FakeLayout final:AndroidParagraphLayout {
    bool wrapped=false;
    std::vector<AndroidParagraphLine> rows;
    explicit FakeLayout(bool value):wrapped(value){
        rows=wrapped?std::vector<AndroidParagraphLine>{{0,4,0,40,0,20,15},{4,7,80,30,20,42,36},{7,11,0,40,42,62,57}}
                    :std::vector<AndroidParagraphLine>{{0,11,0,110,0,20,15}};
    }
    const std::vector<AndroidParagraphLine>& lines()const noexcept override{return rows;}
    double primary(std::size_t position)const override{
        if(wrapped){if(position==4)return 110;if(position==5)return 100;if(position==6)return 90;if(position==7)return 0;}
        return double(position)*10;
    }
    std::size_t hit(std::size_t row,double x)const override{
        if(wrapped&&row==1)return x>105?4:x>95?5:x>85?6:7;
        return std::min<std::size_t>(rows[row].last,std::size_t(std::max(0.,std::round(x/10))));
    }
    bool rtl(std::size_t position)const override{return position>=4&&position<7;}
    std::vector<TextLineSpan> selection(std::size_t row,std::size_t first,std::size_t last)const override{
        if(first==last)return {};
        if(wrapped&&row==1&&first==6&&last==7)return {{80,10}};
        if(wrapped&&row==1)return {{80,30}};
        if(!wrapped&&first==2&&last==5)return {{20,10},{50,10}};
        return {{double(first)*10,double(last-first)*10}};
    }
    TextBitmap render(std::size_t row,double x,double width,double height,Color,double scale)const override{
        check(row<rows.size()&&near(x,7)&&near(width,12)&&near(height,30)&&near(scale,2),"Viewport parameters changed");
        return {24,60,std::vector<std::uint8_t>(24*60*4,127)};
    }
};
struct FakeShaper final:AndroidParagraphShaper {
    std::shared_ptr<AndroidParagraphLayout> shape(std::optional<double> width,std::size_t)const override{
        if(width)check(near(*width,35),"Wrap width was not passed to the native shaper");
        return std::make_shared<FakeLayout>(width.has_value());
    }
};
}
int main(){
    auto index=TextIndex::create("abc אבג xyz");check(bool(index)&&index->utf16_size()==11,"Unicode fixture changed");
    auto shaper=std::make_shared<FakeShaper>();std::weak_ptr<FakeShaper> lifetime=shaper;
    auto line=android_text_line_model(*index,{.font_size=13,.line_height=30},shaper);
    auto metrics=line->metrics();check(near(metrics.width,110)&&near(metrics.height,30)&&near(metrics.baseline,20),"Natural metrics or fixed line height changed");
    auto spans=line->selection(2,5);check(spans.size()==2&&near(spans[0].x,20)&&near(spans[1].x,50),"Bidi visual selection was merged");
    check(line->hit(30)==3&&near(line->caret(3),30),"Unwrapped caret and hit diverged");
    bool limited=false;try{(void)line->wrap(35,2);}catch(const std::length_error&){limited=true;}
    check(limited,"Wrapped line budget was ignored");
    auto fragments=line->wrap(35,3);check(fragments.size()==3&&fragments[1].first==4&&fragments[1].last==7,"Paragraph ranges changed");
    auto& rtl=*fragments[1].line;check(near(rtl.caret(0),30)&&near(rtl.caret(3),0),"Wrapped bidi line-end caret used the next line");
    auto selected=rtl.selection(0,3);check(selected.size()==1&&near(selected[0].x,0)&&near(selected[0].width,30),"Wrapped selection was not normalized to its line");
    auto bitmap=rtl.render(7,12,{1,1,1,1},2);check(bitmap.width==24&&bitmap.height==60,"Rendered viewport allocated the wrong size");
    shaper.reset();line.reset();check(!lifetime.expired()&&near(rtl.caret(0),30),"Wrapped fragment did not retain paragraph ownership");
    fragments.clear();check(lifetime.expired(),"Wrapped paragraph lifetime leaked");
}
