#pragma once
#include <cstdint>
#include <memory>

namespace ii::gui::detail {
// Windows-only, owner-thread surface composed above a borrowed HWND. The
// window remains owned by the embedding application.
class DirectCompositionSurface final {
public:
    explicit DirectCompositionSurface(void* parent);
    ~DirectCompositionSurface();
    DirectCompositionSurface(const DirectCompositionSurface&)=delete;
    DirectCompositionSurface& operator=(const DirectCompositionSurface&)=delete;
    void resize(std::uint32_t width,std::uint32_t height);
    [[nodiscard]] void* swapchain()const noexcept;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace ii::gui::detail
