#pragma once
#include <iiGUI/NativeCanvas.h>
#include <algorithm>

namespace ii::gui::detail {
// Private, concrete two-backend ownership. The template allows deterministic
// failure tests without environment overrides or a production fault-injection API.
template<class Primary,class Vulkan,bool HasPrimary=true> class RendererChain final {
public:
    RendererChain(void* parent,void* display,Acceleration acceleration,Renderer renderer)
        : parent_(parent),display_(display),acceleration_(acceleration),renderer_(renderer) {
        if (renderer==Renderer::vulkan) vulkan_=std::make_unique<Vulkan>(parent,display,acceleration);
        else if constexpr (!HasPrimary) {
            if (renderer==Renderer::native) throw std::runtime_error("No platform-specific accelerator; use Vulkan on this platform");
            vulkan_=std::make_unique<Vulkan>(parent,display,acceleration);
        } else {
            try { primary_=std::make_unique<Primary>(parent,display,acceleration); }
            catch (const std::exception& error) {
                if (renderer!=Renderer::automatic) throw;
                fallback("Native accelerator initialization failed: "+std::string(error.what()),nullptr,0,0);
            }
        }
    }
    void draw(const CanvasFrame& frame,double scale,double height) {
        if (vulkan_) { vulkan_->draw(frame,scale,height); return; }
        try { primary_->draw(frame,scale,height); }
        catch (const std::exception& error) {
            if (renderer_!=Renderer::automatic) throw;
            fallback("Native accelerator rendering failed: "+std::string(error.what()),&frame,scale,height);
        }
    }
    PixelBuffer capture() { return primary_?primary_->capture():vulkan_->capture(); }
    GraphicsInfo info() const {
        auto result=primary_?primary_->info():vulkan_->info();
        result.fallback=!reason_.empty(); result.fallback_reason=reason_;
        return result;
    }
    bool native_event(const void* event) { return primary_?primary_->native_event(event):vulkan_->native_event(event); }
private:
    void fallback(std::string reason,const CanvasFrame* frame,double scale,double height) {
        try {
            auto replacement=std::make_unique<Vulkan>(parent_,display_,acceleration_);
            if(frame&&std::ranges::any_of(frame->overlays,[](const auto& l){return bool(l.backdrop);})&&!replacement->info().backdrop_supported)throw std::runtime_error("Vulkan backdrop materials are not implemented");
            if(frame&&std::ranges::any_of(frame->overlays,[](const auto& l){return bool(l.shadow);})&&!replacement->info().shadow_supported)throw std::runtime_error("Vulkan box shadows are not implemented");
            if(frame&&std::ranges::any_of(frame->overlays,[](const auto& l){return !l.content_transform.identity();})&&!replacement->info().content_transform_supported)throw std::runtime_error("Vulkan content transforms are not implemented");
            if (frame) replacement->draw(*frame,scale,height);
            primary_.reset(); vulkan_=std::move(replacement); reason_=std::move(reason);
        } catch (const std::exception& error) {
            throw std::runtime_error(reason+"; Vulkan fallback failed: "+error.what());
        }
    }
    void* parent_;
    void* display_;
    Acceleration acceleration_;
    Renderer renderer_;
    std::unique_ptr<Primary> primary_;
    std::unique_ptr<Vulkan> vulkan_;
    std::string reason_;
};
} // namespace ii::gui::detail
