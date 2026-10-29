#include <iiGUI/Image.h>
#include <iostream>
#include <stdexcept>
using namespace ii;
namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
constexpr auto source=R"(<svg xmlns="http://www.w3.org/2000/svg" width="1" height="1"><rect width="1" height="1" fill="red"/></svg>)";
void limits(){
    check(!gui::rgba_image(1,1,{255,0,0,255},gui::ImageBudget{0}),"Zero pixel budget accepted");
    check(!gui::rgba_image(1,1,{255,0,0,255},gui::ImageBudget{64ull*1024*1024+1}),"Unbounded pixel budget accepted");
    check(!gui::rgba_image(2,1,std::vector<std::uint8_t>(8),gui::ImageBudget{1}),"Caller pixel budget ignored");
    check(!gui::rgba_image(8193,1,{},gui::ImageBudget{64ull*1024*1024}),"Hard dimension limit bypassed");
    auto image=gui::rgba_image(8192,1,std::vector<std::uint8_t>(8192*4,127),gui::ImageBudget{64ull*1024*1024});
    check(image&&(*image)->width()==8192&&(*image)->rgba().back()==127,"Explicit image budget lost immutable pixels");
}
void svg(){
    auto image=gui::svg_image(source,8192,1,gui::ImageBudget{64ull*1024*1024});
    check(image&&(*image)->width()==8192&&(*image)->height()==1,"SVG explicit raster budget was not forwarded");
    check(!gui::svg_image(source,16,16,gui::ImageBudget{255}),"SVG raster exceeded caller budget");
    check(!gui::svg_image(source,1,1,gui::ImageBudget{0}),"SVG zero budget accepted");
    check(!gui::svg_image(source,8193,1,gui::ImageBudget{64ull*1024*1024}),"SVG hard dimension bound bypassed");
    check(gui::svg_image(source,1,1).has_value(),"Existing SVG overload regressed");
}
}
int main(int argc,char** argv){try{const std::string name=argc>1?argv[1]:"limits";if(name=="limits")limits();else if(name=="svg")svg();else throw std::runtime_error("Unknown image budget case");std::cout<<"Image budget "<<name<<" passed\n";}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
