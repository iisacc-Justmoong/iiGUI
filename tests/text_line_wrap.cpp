#include <iiGUI/TextLine.h>
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>
#include <thread>

namespace {
namespace gui=ii::gui;
namespace core=ii::core;
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(core::Result<T> value){if(!value)throw std::runtime_error(value.error().message);return std::move(*value);}
gui::TextLine line(std::string text){return take(gui::TextLine::create(take(gui::TextIndex::create(std::move(text))),
    {.font_family="Monospace",.font_size=16,.line_height=22}));}
bool edge(const gui::TextIndex& index,std::size_t position){
    return position==0||position==index.utf16_size()||index.next_grapheme(index.previous_grapheme(position))==position;
}
void verify(const gui::TextLine& source,const std::vector<gui::TextLineSlice>& slices){
    require(!slices.empty(),"Wrapped paragraph lost all lines");
    std::size_t expected=0;std::string text;
    for(const auto& slice:slices){
        require(slice.first==expected&&slice.last>=slice.first&&edge(source.index(),slice.first)&&edge(source.index(),slice.last),
                "Wrap produced noncontiguous or split grapheme boundaries");
        require(slice.line.index().utf16_size()==slice.last-slice.first,"Wrapped line index differs from its source range");
        text.append(slice.line.index().text());expected=slice.last;
    }
    require(expected==source.index().utf16_size()&&text==source.index().text(),"Wrap changed the original text");
}
void ascii(){
    auto source=line("alpha beta gamma delta");
    const auto width=(take(source.caret(6))+take(source.caret(11)))/2;
    auto slices=take(source.wrap(width));verify(source,slices);
    require(slices.size()>=3&&slices.front().line.index().text()=="alpha ","Native word wrapping did not preserve a word boundary");
    auto& second=slices[1].line;
    require(take(second.metrics()).height==22&&take(second.caret(1))>take(second.caret(0)),"Wrapped line lost its font metrics or caret");
    const auto hit=take(second.hit_test(take(second.caret(1))));
    require(hit==1,"Wrapped line hit testing is not local to its text");
    auto spans=take(second.selection(0,2));require(!spans.empty()&&spans.front().width>0,"Wrapped selection lost its geometry");
    const auto bitmap=take(second.render(0,width,{.2f,.4f,.6f,1},2));
    require(bitmap.height==44&&std::ranges::any_of(bitmap.rgba,[](auto b){return b!=0;}),"Wrapped glyph raster is empty");
}
void unicode(){
    auto source=line("한글 😀 e\xcc\x81 👩‍👩‍👧‍👦 나머지");
    auto slices=take(source.wrap(24));verify(source,slices);
    require(slices.size()>3,"Unicode text did not wrap");
    for(const auto& slice:slices){
        const auto size=slice.line.index().utf16_size();
        require(bool(slice.line.caret(0))&&bool(slice.line.caret(size)),"Unicode wrapped caret is invalid");
    }
    auto narrow=take(source.wrap(.25));verify(source,narrow);
    require(narrow.size()>slices.size(),"Oversized graphemes did not make progress at a narrow width");
}
void bidi_context(){
    // RLO spans the soft break: reshaping the second fragment by itself would
    // incorrectly turn the following Latin letters left-to-right.
    auto source=line("\xe2\x80\xae" "abcdefghijklmnop" "\xe2\x80\xac");
    auto slices=take(source.wrap(55));verify(source,slices);
    require(slices.size()>=3,"Bidirectional context fixture did not wrap");
    const auto& middle=slices[1].line;const auto end=middle.index().utf16_size();
    require(end>1&&take(middle.caret(0))>take(middle.caret(end)),"Soft-wrapped line lost its paragraph bidi context");
    auto spans=take(middle.selection(0,end));
    require(!spans.empty()&&std::ranges::any_of(spans,[](auto span){return span.width>0;}),"Wrapped bidi selection disappeared");
}
void boundaries(){
    auto empty=line("");auto slices=take(empty.wrap(12));verify(empty,slices);
    require(slices.size()==1&&slices[0].first==0&&slices[0].last==0,"Empty paragraph wrap changed");
    auto source=line("a long line for the budget");
    require(!source.wrap(0)&&!source.wrap(std::numeric_limits<double>::infinity())&&!source.wrap(1,0),
            "Invalid wrap width or line budget was accepted");
    auto limited=source.wrap(1,2);require(!limited&&limited.error().code==core::ErrorCode::full,"Wrap ignored its line budget");
    auto normal=take(source.wrap(35));require(!normal.front().line.wrap(20),"A fragment silently lost its paragraph context when rewrapped");
    bool rejected=false;std::thread worker([&]{auto result=source.wrap(35);rejected=!result&&result.error().code==core::ErrorCode::wrong_executor;});worker.join();
    require(rejected,"Wrapped shaping crossed the owner thread");
    auto retained=[] {auto paragraph=line("retain paragraph shaping");return take(paragraph.wrap(45));}();
    require(retained.size()>1&&take(retained[1].line.metrics()).height==22&&
            !take(retained[1].line.render(0,45,{1,1,1,1})).rgba.empty(),
            "Wrapped fragments did not retain the native paragraph lifetime");
}
}
int main(){ascii();unicode();bidi_context();boundaries();}
