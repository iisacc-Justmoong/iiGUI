#include <iiGUI/Host.h>
#include <iiGUI/NativeWidgets.h>
#include <android/log.h>
#include <array>
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace {
template<class T>T take(ii::core::Result<T> value){if(!value)throw std::runtime_error(value.error().message);return std::move(*value);}
void good(ii::core::Result<void> value){if(!value)throw std::runtime_error(value.error().message);}
std::vector<std::uint8_t> crop(const ii::gui::PixelBuffer& image,ii::gui::Rect bounds,double scale){
    std::vector<std::uint8_t> result;
    const auto left=std::clamp(int(std::floor(bounds.x*scale)),0,int(image.width));
    const auto top=std::clamp(int(std::floor(bounds.y*scale)),0,int(image.height));
    const auto right=std::clamp(int(std::ceil((bounds.x+bounds.width)*scale)),left,int(image.width));
    const auto bottom=std::clamp(int(std::ceil((bounds.y+bounds.height)*scale)),top,int(image.height));
    for(int y=top;y<bottom;++y)result.insert(result.end(),image.rgba.begin()+(std::size_t(y)*image.width+left)*4,image.rgba.begin()+(std::size_t(y)*image.width+right)*4);
    return result;
}
}
// One mounted window; repeated GPU frames must retain static sibling pixels.
void verify_android_retained_labels(ii::gui::Host& host,const ii::gui::WidgetMount& mount){
    auto status=take(mount.find("status"));const auto previous=take(status.snapshot()).text;
    const auto scale=take(host.snapshot()).backing_scale;
    const std::array regions{take(take(mount.find("greeting")).snapshot()).bounds,take(take(mount.find("description")).snapshot()).bounds};
    auto baseline=take(host.capture_canvas());
    const std::array expected{crop(baseline,regions[0],scale),crop(baseline,regions[1],scale)};
    for(const auto& pixels:expected){bool ink=false;for(std::size_t i=0;i<pixels.size();++i)if(i%4!=3&&pixels[i]>40)ink=true;if(!ink)throw std::runtime_error("Static label has no initial ink");}
    for(int tick:{1,2,9,10,99,100,135,999}){
        good(status.set_text("Native C++23 - "+std::to_string(tick)+" s"));
        auto frame=take(host.capture_canvas());
        for(std::size_t i=0;i<regions.size();++i)if(crop(frame,regions[i],scale)!=expected[i])
            throw std::runtime_error("Static label GPU pixels changed at tick "+std::to_string(tick));
    }
    good(status.set_text(previous));
    __android_log_write(ANDROID_LOG_INFO,"iiSDK-LVUI","ANDROID_RETAINED_LABELS_OK gpu_pixels=true digit_transitions=true");
}
