#include <iiGUI/NativeTextLayout.h>
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace {
void require(bool value,const char* message) { if (!value) throw std::runtime_error(message); }
template<class T> T take(ii::core::Result<T> value) {
    if (!value) throw std::runtime_error(value.error().message);
    return std::move(*value);
}
}
int main() {
    using namespace ii::gui;
    require(!NativeTextLayout::create(std::string("a\0b",3))&&!NativeTextLayout::create(std::string("\xc0\x80",2)),
            "Invalid Unicode or embedded NUL was accepted");
    auto layout=take(NativeTextLayout::create("abc\n한😀\ne\xcc\x81Z"));
    require(layout.lines().size()==3 && layout.length()==11,"Paragraph boundaries or UTF-16 length are wrong");
    const auto first=take(layout.caret(0)), korean=take(layout.caret(4));
    require(first.y==0 && korean.y>first.y && korean.height>0,"Caret is not in the correct paragraph");
    const auto emoji_start=take(layout.caret(5)), emoji_end=take(layout.caret(7));
    require(take(layout.hit_test(emoji_start.x,emoji_start.y+1))==5 &&
            take(layout.hit_test(emoji_end.x,emoji_end.y+1))==7,"Shaped hit testing did not preserve emoji boundaries");
    require(!layout.caret(6),"Caret split a surrogate pair");
    struct ForeignRange{std::size_t location,length;};
    require(take(layout.selection(ForeignRange{2,7})).size()>=3,"Foreign range contract failed");
    const auto spans=take(layout.selection({2,7}));
    require(spans.size()>=3,"Selection did not cover the newline and following paragraphs");
    auto masked=take(NativeTextLayout::create("A😀e\xcc\x81Z",{},true));
    require(masked.display_text(0)=="••••" && masked.length()==6,"Password display did not mask whole graphemes");
    const auto before=take(masked.caret(1)),after=take(masked.caret(3));
    const auto hit=take(masked.hit_test((before.x+after.x)*.5,before.y+1));
    require(hit==1 || hit==3,"Password hit testing split an emoji");
    require(!masked.selection({2,1}),"Password selection accepted a split surrogate");
    auto empty=take(NativeTextLayout::create("\n"));
    require(empty.lines().size()==2 && take(empty.caret(1)).y>0,"Trailing empty paragraph disappeared");
    require(!NativeTextLayout::create(std::string(4097,'\n')),"Excessive paragraph count was accepted");
    require(!layout.hit_test(std::numeric_limits<double>::infinity(),0),"Non-finite hit position was accepted");

    const ii::gui::TextLineStyle mono{.font_family="Monospace",.font_size=16,.line_height=22};
    auto wrapped=take(NativeTextLayout::create("alpha beta gamma\n끝",mono,false,55));
    require(wrapped.lines().size()>=4&&wrapped.lines()[0].next==wrapped.lines()[0].last&&
            wrapped.lines()[0].last==wrapped.lines()[1].first,"Soft wrapping lost its source boundaries");
    const auto boundary=wrapped.lines()[0].last;
    const auto upper=take(wrapped.caret(boundary,NativeTextAffinity::upstream));
    const auto lower=take(wrapped.caret(boundary,NativeTextAffinity::downstream));
    require(upper.y==0&&lower.y>upper.y,"A soft break did not preserve both caret affinities");
    const auto first_hit=take(wrapped.hit_test_position(upper.x+2,upper.y+upper.height/2));
    const auto second_hit=take(wrapped.hit_test_position(lower.x,lower.y+lower.height/2));
    require(first_hit.position==boundary&&first_hit.affinity==NativeTextAffinity::upstream&&
            second_hit.position==boundary&&second_hit.affinity==NativeTextAffinity::downstream,
            "Pointer hits on either side of a soft break did not retain visual affinity");
    auto selections=take(wrapped.selection({0,boundary}));
    require(!selections.empty()&&std::ranges::all_of(selections,[&](auto span){return span.y==upper.y;}),
            "A soft break added a phantom newline selection");
    auto wrapped_mask=take(NativeTextLayout::create("A😀e\xcc\x81Z",mono,true,1));
    require(wrapped_mask.lines().size()==4&&wrapped_mask.lines()[1].first==1&&wrapped_mask.lines()[1].last==3&&
            wrapped_mask.lines()[2].first==3&&wrapped_mask.lines()[2].last==5,
            "Masked wrapping split an original emoji or combining grapheme");
    const auto combining=take(wrapped_mask.caret(4));
    const auto before_combining=take(wrapped_mask.caret(3)),after_combining=take(wrapped_mask.caret(5,NativeTextAffinity::upstream));
    require((combining.x==before_combining.x&&combining.y==before_combining.y)||
            (combining.x==after_combining.x&&combining.y==after_combining.y),
            "Masked caret exposed an interior position of a grapheme");
    require(!NativeTextLayout::create("text",mono,false,0)&&
            !NativeTextLayout::create("text",mono,false,std::numeric_limits<double>::quiet_NaN()),
            "Invalid automatic wrapping width was accepted");
    auto at_capacity=NativeTextLayout::create(std::string(NativeTextLayout::maximum_lines,'a')+"\n",mono,false,1);
    require(!at_capacity&&at_capacity.error().code==ii::core::ErrorCode::full,
            "A paragraph after the visual-line budget did not request a safe wrapping fallback");
}
