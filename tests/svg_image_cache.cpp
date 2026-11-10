#include <iiGUI/SvgImageCache.h>
#include <algorithm>
#include <array>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(ii::core::Result<T> value){if(!value)throw std::runtime_error(value.error().message);return std::move(*value);}
constexpr std::string_view svg="<svg xmlns='http://www.w3.org/2000/svg' width='4' height='4'><rect width='4' height='4' fill='#f00'/></svg>";
}
int main(){
    using namespace ii::gui;
    SvgImageCache cache({.maximum_entries=2,.maximum_bytes=1024});
    auto a=std::make_shared<const int>(1),b=std::make_shared<const int>(2),c=std::make_shared<const int>(3);
    auto first=take(cache.resolve(a,svg,4));auto second=take(cache.resolve(b,svg,4));
    require(first->width()==4&&first->height()==4&&cache.stats().entries==2,"SVG images were not decoded and cached");
    require(take(cache.resolve(a,svg,4))==first,"Repeated SVG identity did not reuse its immutable image");
    std::weak_ptr<const RasterImage> evicted=second;second.reset();(void)take(cache.resolve(c,svg,4));
    require(evicted.expired()&&cache.stats().entries==2,"SVG image cache did not evict the least recently used image");
    require(!cache.resolve({},svg,4)&&!cache.resolve(a,svg,0)&&!cache.resolve(a,"invalid svg",5),"Invalid SVG request was accepted");
    require(cache.stats().entries==2&&take(cache.resolve(a,svg,4))==first,"A failed SVG request disturbed cached images");
    cache.clear();require(cache.stats().entries==0&&cache.stats().bytes==0,"SVG cache clear retained entries");
    SvgImageCache disabled({.maximum_entries=0,.maximum_bytes=0});
    auto one=take(disabled.resolve(a,svg,4));auto two=take(disabled.resolve(a,svg,4));
    require(one!=two&&disabled.stats().entries==0,"Disabled SVG caching retained an image");
    SvgImageCache wrong_size({},[](std::string_view,std::uint32_t){return rgba_image(1,1,std::vector<std::uint8_t>(4,255));});
    require(!wrong_size.resolve(a,svg,4)&&wrong_size.stats().entries==0,
            "A decoder returning the wrong raster extent polluted the cache");
    SvgImageCache concurrent;auto key=std::make_shared<const int>(9);
    std::array<std::shared_ptr<const RasterImage>,8> images;std::array<std::thread,8> threads;
    for(std::size_t i=0;i<threads.size();++i)threads[i]=std::thread([&,i]{images[i]=take(concurrent.resolve(key,svg,8));});
    for(auto& thread:threads)thread.join();
    require(std::ranges::all_of(images,[&](const auto& image){return image==images[0];})&&concurrent.stats().entries==1,
            "Concurrent SVG cache requests did not converge on one image");
    std::mutex mutex;std::condition_variable condition;bool started=false,release=false;
    SvgImageCache clearing({},[&](std::string_view,std::uint32_t)->ii::core::Result<std::shared_ptr<const RasterImage>>{
        {std::unique_lock lock(mutex);started=true;condition.notify_one();condition.wait(lock,[&]{return release;});}
        return rgba_image(2,2,std::vector<std::uint8_t>(16,255));
    });
    std::thread inflight([&]{(void)take(clearing.resolve(key,svg,2));});
    {std::unique_lock lock(mutex);condition.wait(lock,[&]{return started;});}
    clearing.clear();
    {std::lock_guard lock(mutex);release=true;}condition.notify_one();inflight.join();
    require(clearing.stats().entries==0,"A request admitted before clear repopulated the SVG cache");
    (void)take(clearing.resolve(key,svg,2));require(clearing.stats().entries==1,"SVG cache did not accept requests after clear");
}
