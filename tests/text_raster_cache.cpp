#include <iiGUI/TextRasterCache.h>
#include <algorithm>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <thread>
using namespace ii;
namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(core::Result<T> r){if(!r)throw std::runtime_error(r.error().message);return std::move(*r);}
void good(core::Result<void> r){if(!r)throw std::runtime_error(r.error().message);}
gui::TextLine line(std::string text="Native 😀",double size=13){return take(gui::TextLine::create(take(gui::TextIndex::create(std::move(text))),{.font_family="Monospace",.font_size=size,.line_height=20}));}
bool same(const gui::TextBitmap& a,const gui::RasterImage& b){return a.width==b.width()&&a.height==b.height()&&std::ranges::equal(a.rgba,b.rgba());}
void run(const std::string& name){
    auto cache=take(gui::TextRasterCache::create());auto text=line();const gui::Color blue{.02f,.2f,.7f,1};
    if(name=="native_rgb"){auto expected=take(text.render(0,240,blue,2));auto actual=take(cache->paint(text,0,240,blue,2));check(same(expected,*actual.image),"Text cache replaced native glyph RGB with a white/tinted image");}
    else if(name=="alpha"){auto full=take(cache->paint(text,0,240,blue));auto faded=take(cache->paint(text,0,240,{blue.red,blue.green,blue.blue,.25f}));check(full.image==faded.image&&faded.opacity==.25f&&take(cache->stats()).rasterizations==1,"Opacity change rerasterized native glyphs");}
    else if(name=="palette"){auto a=take(cache->paint(text,0,240,{.8f,.01f,.02f,1}));auto b=take(cache->paint(text,0,240,{.01f,.8f,.02f,1}));auto again=take(cache->paint(text,0,240,{.8f,.01f,.02f,.7f}));check(a.image==again.image&&a.image!=b.image&&take(cache->stats()).rasterizations==2,"Body/selection palette was not retained separately");check(same(take(text.render(0,240,{.01f,.8f,.02f,1})),*b.image),"Selection paint recolored a native color glyph");}
    else if(name=="identity"){auto a=take(cache->paint(text,0,240,blue));auto copy=text;auto b=take(cache->paint(copy,0,240,blue));auto other=line("Native 😀",24);auto c=take(cache->paint(other,0,240,blue));check(a.image==b.image&&a.image!=c.image,"Shaped-line identity lost sharing or aliased a different font layout");}
    else if(name=="region"){auto a=take(cache->paint(text,0,80,blue));auto b=take(cache->paint(text,30,80,blue));auto c=take(cache->paint(text,30,80,blue,2));check(a.image!=b.image&&b.image!=c.image&&c.image->width()==160&&c.image->height()==40,"Crop/scale was omitted from the native text cache key");}
    else if(name=="lru"){good(cache->set_limits({.maximum_bytes=8000,.maximum_entries=8}));auto a=take(cache->paint(text,0,48,{1,0,0,1}));auto b=take(cache->paint(text,0,48,{0,1,0,1}));check(take(cache->paint(text,0,48,{1,0,0,1})).image==a.image,"Recent native text paint was not reused");take(cache->paint(text,0,48,{0,0,1,1}));check(take(cache->stats()).entries==2&&take(cache->stats()).bytes==7680&&take(cache->stats()).evictions==1,"Raster byte budget did not evict the LRU entry");check(take(cache->paint(text,0,48,{0,1,0,1})).image!=b.image,"Evicted palette entry still addressed its old bitmap");}
    else if(name=="entry_limit"){good(cache->set_limits({.maximum_bytes=1024*1024,.maximum_entries=1}));take(cache->paint(text,0,48,blue));take(cache->paint(text,1,48,blue));check(take(cache->stats()).entries==1&&take(cache->stats()).evictions==1,"Native text entry limit was ignored");}
    else if(name=="budget"){good(cache->set_limits({.maximum_bytes=128,.maximum_entries=4}));auto first=take(cache->paint(text,0,1,blue));auto before=take(cache->stats());auto rejected=cache->paint(text,0,48,blue);auto after=take(cache->stats());check(!rejected&&rejected.error().code==core::ErrorCode::full&&before.entries==after.entries&&before.bytes==after.bytes&&take(cache->paint(text,0,1,blue)).image==first.image,"Oversized raster request mutated the retained cache");}
    else if(name=="limits"){take(cache->paint(text,0,48,blue));good(cache->set_limits({.maximum_bytes=1,.maximum_entries=1}));check(take(cache->stats()).entries==0,"Shrinking raster budget retained oversized entries");check(!cache->set_limits({.maximum_bytes=0}),"Zero raster budget was admitted");}
    else if(name=="clear"){take(cache->paint(text,0,48,blue));good(cache->clear());auto s=take(cache->stats());check(s.bytes==0&&s.entries==0&&s.rasterizations==1,"Cache clear reset lifetime counters or retained images");}
    else if(name=="invalid"){const auto nan=std::numeric_limits<double>::quiet_NaN();check(!cache->paint(text,nan,20,blue)&&!cache->paint(text,0,20,{1,1,1,2})&&!cache->paint(text,0,20,blue,0)&&!cache->paint({},0,20,blue),"Invalid input entered the native text cache key");check(take(cache->stats()).entries==0,"Rejected input changed the raster cache");}
    else if(name=="owner"){bool rejected=false;std::thread worker([&]{auto r=cache->paint(text,0,20,blue);rejected=!r&&r.error().code==core::ErrorCode::wrong_executor;});worker.join();check(rejected,"Text raster cache crossed its UI owner thread");}
    else throw std::runtime_error("Unknown text raster cache case");
}
}
int main(int argc,char** argv){try{run(argc>1?argv[1]:"native_rgb");std::cout<<"TEXT_RASTER_CACHE_OK\n";}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
