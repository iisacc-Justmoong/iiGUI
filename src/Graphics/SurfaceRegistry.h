#pragma once
#include <cstdint>
#include <map>
#include <memory>
namespace ii::gui::detail {
// Owner-thread weak registry for hosts with one OS presentation surface.
// Transactional widget generations can overlap without creating two swapchains
// for the same native window. The registry never extends GPU resource lifetime.
template<class Surface>class SurfaceRegistry final {
    using Key=std::pair<std::uintptr_t,std::uintptr_t>;
    std::map<Key,std::weak_ptr<Surface>> surfaces_;
public:
    template<class Factory>std::shared_ptr<Surface> acquire(void* parent,void* display,Factory&& factory){
        std::erase_if(surfaces_,[](const auto& entry){return entry.second.expired();});
        const Key key{reinterpret_cast<std::uintptr_t>(parent),reinterpret_cast<std::uintptr_t>(display)};
        if(auto found=surfaces_.find(key);found!=surfaces_.end())if(auto value=found->second.lock())return value;
        auto value=factory();surfaces_[key]=value;return value;
    }
};
}
