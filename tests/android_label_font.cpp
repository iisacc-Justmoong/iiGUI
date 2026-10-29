#include "iiGUI/AndroidLabelFont.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <stdexcept>

using ii::gui::detail::AndroidLabelFont;
void require(bool value,const char* why){if(!value)throw std::runtime_error(why);}
int main(int argc,char** argv){
    try{
        require(argc==2,"Expected the pinned Pretendard font");std::ifstream file(argv[1],std::ios::binary);
        std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(file)),{});require(!bytes.empty(),"Font fixture missing");
        bool invalid_face=false;try{AndroidLabelFont invalid(bytes,1);}catch(const std::invalid_argument&){invalid_face=true;}require(invalid_face,"A standalone font accepted a collection index");
        AndroidLabelFont font(std::move(bytes));
        const auto width=font.width(32,"Hello World");require(width>50&&width<500,"Invalid native label advance");
        require(std::abs(font.width(64,"Hello World")-2*width)<.001,"Font scale must preserve logical geometry");
        require(font.height(32)>0&&font.ascent(32)>0,"Font line metrics missing");
        const auto glyph=font.glyph(112,'H');require(glyph.width>0&&glyph.height>0,"Native glyph is empty");
        require(std::ranges::any_of(glyph.coverage,[](auto v){return v>0;}),"Native glyph has no ink");
        require(font.width(32,"")==0,"Empty label has nonzero width");
        for(float size:{0.f,-1.f,2049.f,std::numeric_limits<float>::quiet_NaN()}){
            bool rejected=false;try{(void)font.width(size,"Hello");}catch(const std::invalid_argument&){rejected=true;}require(rejected,"Invalid font size was admitted");
        }
        bool rejected=false;try{(void)font.width(32,"\xCE\xA9");}catch(const std::invalid_argument&){rejected=true;}require(rejected,"Unsupported shaping was accepted");
        std::cout<<"ANDROID_LABEL_FONT_OK advance="<<width<<" glyph="<<glyph.width<<'x'<<glyph.height<<" scale=2 invalid_sizes=rejected\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
