#pragma once
#include <cstdint>
#include <stdexcept>
#include <utility>

namespace ii::gui::detail {
// Retain only dimensions successfully committed by the platform. A failed
// resize remains eligible for retry; unchanged frames do no buffer work.
class CompositionResize final {
public:
    CompositionResize(std::uint32_t width,std::uint32_t height):width_(width),height_(height) {
        if(!width || !height)throw std::invalid_argument("Composition dimensions must be nonzero");
    }
    template<class Resize> bool update(std::uint32_t width,std::uint32_t height,Resize&& resize) {
        if(!width || !height)throw std::invalid_argument("Composition dimensions must be nonzero");
        if(width==width_ && height==height_)return false;
        std::forward<Resize>(resize)(width,height);
        width_=width;height_=height;
        return true;
    }
private:
    std::uint32_t width_,height_;
};
} // namespace ii::gui::detail
