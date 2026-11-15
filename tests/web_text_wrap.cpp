#include "../src/Text/WebTextWrap.h"
#include <iiGUI/TextBoundary.h>
#include <cmath>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
double graphemes(std::string_view text){
    auto index=ii::gui::TextIndex::create(std::string(text));
    if(!index)throw std::runtime_error("Invalid test input");
    double count=0;
    for(std::size_t at=0;at<index->utf16_size();at=index->next_grapheme(at))++count;
    return count;
}
void words_and_explicit_breaks(){
    const std::string text="Alpha beta gamma";
    auto lines=ii::gui::detail::web_text_wrap(text,5,true,graphemes);
    check(lines==std::vector<std::string_view>({"Alpha ","beta ","gamma"}),"Word wrapping lost or moved spaces");
    const std::string paragraphs="a\r\n\r\nb\n";
    lines=ii::gui::detail::web_text_wrap(paragraphs,20,true,graphemes);
    check(lines==std::vector<std::string_view>({"a","","b",""}),"Explicit empty paragraphs changed");
    lines=ii::gui::detail::web_text_wrap(text,2,false,graphemes);
    check(lines==std::vector<std::string_view>({"Alpha beta gamma"}),"Ellipsis mode added soft wraps");
}
void grapheme_clusters_remain_intact(){
    const std::string text="A👩🏽‍💻e\u0301가";
    auto lines=ii::gui::detail::web_text_wrap(text,1,true,graphemes);
    check(lines==std::vector<std::string_view>({"A","👩🏽‍💻","e\u0301","가"}),"Wrapping split a grapheme");
}
void long_unbroken_text_is_bounded(){
    const std::string text(65536,'a');std::size_t calls=0;
    const auto lines=ii::gui::detail::web_text_wrap(text,20,true,[&](std::string_view part){++calls;return double(part.size());});
    check(lines.size()==3277 && lines.back().size()==16,"Long word wrapping is incorrect");
    check(calls<50000,"Long word wrapping performs excessive width measurements");
}
void text_is_not_lost_when_wrapping(){
    for(const std::string text:{"  a b  c ","가나다라마바사", "hello👩🏽‍💻e\u0301 world", "a\tb"}){
        for(double width:{0.5,1.0,2.0,8.0,100.0}){
            const auto lines=ii::gui::detail::web_text_wrap(text,width,true,graphemes);
            std::string joined;for(auto line:lines)joined.append(line);
            check(joined==text,"Soft wrapping lost source text");
        }
    }
    const std::string text="invisible";
    check(ii::gui::detail::web_text_wrap(text,0,true,graphemes)==std::vector<std::string_view>{text},
          "A zero-width surface performed unnecessary wrapping");
}
}
int main(){words_and_explicit_breaks();grapheme_clusters_remain_intact();long_unbroken_text_is_bounded();text_is_not_lost_when_wrapping();}
