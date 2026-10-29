#pragma once
#include <iiGUI/Export.h>
#include <iiSDKCore/Core/Result/Result.h>
#include <cstddef>
#include <memory>
#include <string>
#include <string_view>

namespace ii::gui {
// Immutable UTF-8 text indexed in native UTF-16 units. No window, font,
// renderer or owner thread is required. ASCII runs need no per-scalar table.
class IIGUI_EXPORT TextIndex final {
public:
    TextIndex() = default;
    [[nodiscard]] static core::Result<TextIndex> create(std::string);
    [[nodiscard]] std::string_view text() const noexcept;
    [[nodiscard]] std::size_t utf16_size() const noexcept;
    [[nodiscard]] std::size_t storage_bytes() const noexcept;
    // Invalid offsets, including the middle of a surrogate pair, are errors.
    [[nodiscard]] core::Result<std::size_t> byte_for_utf16(std::size_t) const;
    [[nodiscard]] core::Result<std::size_t> utf16_for_byte(std::size_t) const;
    [[nodiscard]] std::size_t previous_grapheme(std::size_t) const noexcept;
    [[nodiscard]] std::size_t next_grapheme(std::size_t) const noexcept;
    // LVRS word movement: skip separators, then letters/numbers/underscore.
    // A movement never ends inside an extended grapheme cluster.
    [[nodiscard]] std::size_t previous_word(std::size_t) const noexcept;
    [[nodiscard]] std::size_t next_word(std::size_t) const noexcept;
    [[nodiscard]] bool word_at(std::size_t) const noexcept;
    static constexpr std::size_t maximum_text_bytes = 64 * 1024 * 1024;
    static constexpr std::size_t maximum_index_bytes = 128 * 1024 * 1024;
private:
    struct Impl;
    std::shared_ptr<const Impl> impl_;
};
} // namespace ii::gui
