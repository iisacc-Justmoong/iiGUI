#include <iiGUI/TextLine.h>
#include <iiGUI/Text.h>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <thread>
using namespace ii;
namespace {
void check(bool b,const char* m){if(!b)throw std::runtime_error(m);}
template<class T>T take(core::Result<T> r){if(!r)throw std::runtime_error(r.error().message);return std::move(*r);}
gui::TextLine line(std::string s,gui::TextLineStyle style={.font_family="Monospace",.line_height=20}){return take(gui::TextLine::create(take(gui::TextIndex::create(std::move(s))),std::move(style)));}
void run(const std::string& test){
    if(test=="empty"){auto l=line("");auto m=take(l.metrics());check(m.height==20&&m.width==0&&take(l.hit_test(500))==0,"Empty shaped paragraph contract changed");}
    else if(test=="ascii"){auto l=line("hello world");auto p=take(l.caret(5));check(p>0&&take(l.hit_test(p))==5&&take(l.selection(0,5)).size()==1,"ASCII caret/hit/selection disagree");}
    else if(test=="utf16"){auto l=line("A한😀Z");check(!l.caret(3)&&take(l.hit_test(take(l.caret(4))))==4,"Native UTF-16 surrogate boundary changed");}
    else if(test=="graphemes"){auto l=line("e\xcc\x81👩‍👩‍👧‍👦Z");for(double x=0;x<take(l.metrics()).width;x+=.25){auto p=take(l.hit_test(x));check(p==0||p==2||p==13||p==14,"Native hit split an extended grapheme");}}
    else if(test=="hangul"){
        const gui::TextLineStyle style{.font_family="Helvetica",.font_size=13,.line_height=20};
        const auto composed=line("A안녕Z",style),decomposed=line("A안녕Z",style);
        const auto m=take(decomposed.metrics());
        check(std::abs(take(composed.metrics()).width-m.width)<.1,"Hangul jamo did not shape to equivalent syllables");
        for(double x=0;x<m.width;x+=.25){const auto p=take(decomposed.hit_test(x));check(p==0||p==1||p==4||p==7||p==8,"Hit testing split a Hangul syllable");}
        auto verify=[](const gui::TextBitmap& pixels){
            unsigned solid=0,edges=0,clear=0;
            for(std::size_t i=0;i<pixels.rgba.size();i+=4){
                const auto a=pixels.rgba[i+3];solid+=a>180;edges+=a>16&&a<220;clear+=a==0;
                if(a>16)check(pixels.rgba[i]>240&&pixels.rgba[i+1]>240&&pixels.rgba[i+2]>240,"White Hangul ink gained color fringes");
            }
            check(solid&&edges&&clear,"Hangul raster lost stroke contrast, antialiasing, or transparent margins");
        };
        const auto paragraph=line("안녕! 한 문장으로 인사해 줘. A",style);
        for(double scale:{1.,1.25,2.,3.}){
            const auto raster=take(paragraph.render(0,180,{1,1,1,1},scale));
            check(raster.width==std::ceil(180*scale)&&raster.height==std::ceil(20*scale),"Hangul editing raster did not match drawable scale");
            verify(raster);
            const auto label=take(gui::render_text({.bounds={0,0,180,32},.text="안녕! 한 문장으로 인사해 줘. A",
                .color={1,1,1,1},.font_size=13,.font_family="Helvetica",.line_height=20},scale));
            check(label.width==std::ceil(180*scale)&&label.height==std::ceil(32*scale),"Hangul label raster did not match drawable scale");
            verify(label);
        }
    }
    else if(test=="tabs"){auto l=line("a\tb",{.font_family="Monospace",.font_size=13,.line_height=20,.tab_stop_distance=100});check(std::abs(take(l.caret(2))-100)<2,"Explicit tab stop was ignored");}
    else if(test=="bidi"){auto l=line("abc אבג xyz");auto spans=take(l.selection(2,5));double width=0;for(auto s:spans){check(s.x>=0&&s.width>=0,"Bidi span geometry is invalid");width+=s.width;}check(spans.size()>=2&&width>0,"Bidi selection collapsed to one logical rectangle");}
    else if(test=="crop"){auto l=line(std::string(20000,'M'));check(take(l.metrics()).width>8192,"Long paragraph wrapped into the canvas");auto pixels=take(l.render(50000,120,{1,1,1,1},2));check(pixels.width==240&&pixels.height==40&&pixels.rgba.size()==240*40*4,"Long paragraph allocated its full width");check(std::ranges::any_of(pixels.rgba,[](auto b){return b!=0;}),"Clipped native glyph viewport is empty");}
    else if(test=="alpha"||test=="color_font_alpha"){
        auto l=line(test=="alpha"?"Native RGB":"A 😀 한");auto full=take(l.render(0,160,{.2f,.5f,.8f,1}));auto part=take(l.render(0,160,{.2f,.5f,.8f,.37f}));auto empty=take(l.render(0,160,{.2f,.5f,.8f,0}));
        check(full.rgba.size()==part.rgba.size()&&std::ranges::any_of(full.rgba,[](auto b){return b!=0;}),"Native text opacity fixture is empty");
        for(std::size_t i=0;i<full.rgba.size();i+=4){const auto alpha=std::uint8_t(std::lround(full.rgba[i+3]*.37f));check(part.rgba[i+3]==alpha,"Native palette glyph ignored the uniform opacity contract");for(unsigned c=0;c<3;++c)check(part.rgba[i+c]==(alpha?full.rgba[i+c]:0),"Uniform opacity altered straight-alpha native RGB");}
        check(std::ranges::all_of(empty.rgba,[](auto b){return b==0;}),"Zero opacity retained a color-font pixel");
    }
    else if(test=="nul"){auto l=line(std::string("a\0b",3));check(l.index().utf16_size()==3&&take(l.caret(3))>take(l.caret(2)),"Control-picture shaping changed source positions");}
    else if(test=="limits"){check(!gui::TextLine::create(take(gui::TextIndex::create("a\nb"))),"Multiline text admitted as a paragraph");check(!gui::TextLine::create(take(gui::TextIndex::create(std::string(gui::TextLine::maximum_text_bytes+1,'x')))),"Native paragraph admission bound ignored");auto l=line("x");check(!l.render(0,9000,{1,1,1,1})&&!l.selection(1,0),"Invalid viewport or reversed selection accepted");}
    else if(test=="owner"){auto l=line("text");bool rejected=false;std::thread t([&]{auto r=l.metrics();rejected=!r&&r.error().code==core::ErrorCode::wrong_executor;});t.join();check(rejected,"Native shaped line crossed its owner thread");}
    else throw std::runtime_error("Unknown text line case");
}
}
int main(int argc,char** argv){try{run(argc>1?argv[1]:"empty");std::cout<<"TEXT_LINE_OK\n";}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
