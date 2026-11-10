#pragma once
#include <cstddef>

namespace ii::gui::detail {
// Owner-thread demand for a display source. Different widget trees can hold
// independent motion requests while a game owns a continuous frame loop.
class FrameDemand final {
public:
    void foreground(bool value) noexcept { foreground_ = value; }
    void continuous(bool value) noexcept { continuous_ = value; }
    void motion(bool active) noexcept {
        if (active) ++motions_;
        else if (motions_) --motions_;
    }
    [[nodiscard]] bool needed() const noexcept { return foreground_ && (continuous_ || motions_ != 0); }
private:
    std::size_t motions_ = 0;
    bool foreground_ = false, continuous_ = false;
};
}
