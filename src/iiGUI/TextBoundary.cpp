#include "TextBoundary.h"
#include <iiSDKCore/Core/Text/Utf8.h>
#include "../third_party/utf8proc/utf8proc.h"
#include <algorithm>
#include <cstdint>
#include <vector>

namespace ii::gui {
struct TextIndex::Impl final {
    struct Scalar { std::uint32_t byte, unit; std::uint8_t bytes, units; };
    struct Cluster { std::uint32_t first, last; };
    std::string value;
    std::vector<Scalar> non_ascii;
    std::vector<Cluster> joined;
    std::size_t units = 0;
    [[nodiscard]] std::size_t allocated() const noexcept {
        return sizeof(Impl) + value.capacity() + non_ascii.capacity() * sizeof(Scalar) + joined.capacity() * sizeof(Cluster);
    }
    [[nodiscard]] bool word(std::size_t position) const noexcept {
        const auto at = std::upper_bound(non_ascii.begin(), non_ascii.end(), position,
            [](std::size_t p, const Scalar& s) { return p < s.unit; });
        std::size_t byte = position;
        if (at != non_ascii.begin()) {
            const auto& s = *std::prev(at);
            byte = position < std::size_t(s.unit) + s.units ? s.byte :
                std::size_t(s.byte) + s.bytes + position - (std::size_t(s.unit) + s.units);
        }
        if (byte >= value.size()) return false;
        const auto first = static_cast<unsigned char>(value[byte]);
        if (first < 128) return (first >= 'a' && first <= 'z') || (first >= 'A' && first <= 'Z') || (first >= '0' && first <= '9') || first == '_';
        utf8proc_int32_t codepoint = 0;
        if (utf8proc_iterate(reinterpret_cast<const utf8proc_uint8_t*>(value.data() + byte), utf8proc_ssize_t(value.size() - byte), &codepoint) < 1) return false;
        const auto category = utf8proc_category(codepoint);
        return (category >= UTF8PROC_CATEGORY_LU && category <= UTF8PROC_CATEGORY_LO) ||
            (category >= UTF8PROC_CATEGORY_ND && category <= UTF8PROC_CATEGORY_NO);
    }
};

core::Result<TextIndex> TextIndex::create(std::string value) {
    if (value.size() > maximum_text_bytes) return core::fail(core::ErrorCode::out_of_range, "Text index input exceeds its byte budget");
    if (!core::valid_utf8(value)) return core::fail(core::ErrorCode::invalid_argument, "Text index requires valid UTF-8");
    try {
        auto p = std::make_shared<Impl>();
        p->value = std::move(value);
        if (p->allocated() > maximum_index_bytes) return core::fail(core::ErrorCode::out_of_range, "Text index storage exceeds its byte budget");
        std::size_t byte = 0, unit = 0, cluster = 0;
        utf8proc_int32_t previous = -1, state = 0;
        while (byte < p->value.size()) {
            utf8proc_int32_t current = static_cast<unsigned char>(p->value[byte]);
            const auto bytes = current < 128 ? 1 : int(utf8proc_iterate(
                reinterpret_cast<const utf8proc_uint8_t*>(p->value.data() + byte), utf8proc_ssize_t(p->value.size() - byte), &current));
            const auto units = current > 0xffff ? 2u : 1u;
            bool boundary = previous < 0;
            if (previous >= 0) {
                if (previous < 128 && current < 128) { boundary = previous != '\r' || current != '\n'; state = 0; }
                else boundary = utf8proc_grapheme_break_stateful(previous, current, &state);
            }
            if (boundary) {
                if (unit - cluster > 1) p->joined.push_back({std::uint32_t(cluster), std::uint32_t(unit)});
                cluster = unit;
            }
            if (current >= 128) p->non_ascii.push_back({std::uint32_t(byte), std::uint32_t(unit), std::uint8_t(bytes), std::uint8_t(units)});
            byte += std::size_t(bytes); unit += units; previous = current;
            if (p->allocated() > maximum_index_bytes) return core::fail(core::ErrorCode::out_of_range, "Text index metadata exceeds its byte budget");
        }
        if (unit - cluster > 1) p->joined.push_back({std::uint32_t(cluster), std::uint32_t(unit)});
        p->units = unit;
        if (p->allocated() > maximum_index_bytes) return core::fail(core::ErrorCode::out_of_range, "Text index metadata exceeds its byte budget");
        TextIndex result; result.impl_ = std::move(p); return result;
    } catch (...) { return core::fail(core::ErrorCode::failed, "Text index allocation failed", std::current_exception()); }
}
std::string_view TextIndex::text() const noexcept { return impl_ ? std::string_view(impl_->value) : std::string_view{}; }
std::size_t TextIndex::utf16_size() const noexcept { return impl_ ? impl_->units : 0; }
std::size_t TextIndex::storage_bytes() const noexcept { return impl_ ? impl_->allocated() : 0; }
core::Result<std::size_t> TextIndex::byte_for_utf16(std::size_t position) const {
    if (position > utf16_size()) return core::fail(core::ErrorCode::out_of_range, "UTF-16 position exceeds text");
    if (!impl_) return std::size_t(0);
    const auto at = std::upper_bound(impl_->non_ascii.begin(), impl_->non_ascii.end(), position,
        [](std::size_t p, const Impl::Scalar& s) { return p < s.unit; });
    if (at == impl_->non_ascii.begin()) return position;
    const auto& s = *std::prev(at);
    if (position == s.unit) return std::size_t(s.byte);
    if (position < std::size_t(s.unit) + s.units) return core::fail(core::ErrorCode::out_of_range, "UTF-16 position splits a surrogate pair");
    return std::size_t(s.byte) + s.bytes + position - (std::size_t(s.unit) + s.units);
}
core::Result<std::size_t> TextIndex::utf16_for_byte(std::size_t position) const {
    if (position > text().size()) return core::fail(core::ErrorCode::out_of_range, "Byte position exceeds text");
    if (!impl_) return std::size_t(0);
    const auto at = std::upper_bound(impl_->non_ascii.begin(), impl_->non_ascii.end(), position,
        [](std::size_t p, const Impl::Scalar& s) { return p < s.byte; });
    if (at == impl_->non_ascii.begin()) return position;
    const auto& s = *std::prev(at);
    if (position == s.byte) return std::size_t(s.unit);
    if (position < std::size_t(s.byte) + s.bytes) return core::fail(core::ErrorCode::out_of_range, "Byte position splits a UTF-8 scalar");
    return std::size_t(s.unit) + s.units + position - (std::size_t(s.byte) + s.bytes);
}
std::size_t TextIndex::previous_grapheme(std::size_t position) const noexcept {
    position = std::min(position, utf16_size()); if (!position || !impl_) return 0;
    const auto at = std::lower_bound(impl_->joined.begin(), impl_->joined.end(), position,
        [](const Impl::Cluster& c, std::size_t p) { return c.first < p; });
    if (at != impl_->joined.begin()) { const auto& c = *std::prev(at); if (position <= c.last) return c.first; }
    return position - 1;
}
std::size_t TextIndex::next_grapheme(std::size_t position) const noexcept {
    position = std::min(position, utf16_size()); if (position == utf16_size() || !impl_) return position;
    const auto at = std::upper_bound(impl_->joined.begin(), impl_->joined.end(), position,
        [](std::size_t p, const Impl::Cluster& c) { return p < c.first; });
    if (at != impl_->joined.begin()) { const auto& c = *std::prev(at); if (position < c.last) return c.last; }
    return position + 1;
}
std::size_t TextIndex::previous_word(std::size_t position) const noexcept {
    position = std::min(position, utf16_size()); if (!impl_) return 0;
    while (position && !impl_->word(previous_grapheme(position))) position = previous_grapheme(position);
    while (position && impl_->word(previous_grapheme(position))) position = previous_grapheme(position);
    return position;
}
std::size_t TextIndex::next_word(std::size_t position) const noexcept {
    position = std::min(position, utf16_size()); if (!impl_) return 0;
    // A caller may supply a scalar boundary inside a combining cluster.
    if (position && next_grapheme(previous_grapheme(position)) > position) position = previous_grapheme(position);
    while (position < utf16_size() && !impl_->word(position)) position = next_grapheme(position);
    while (position < utf16_size() && impl_->word(position)) position = next_grapheme(position);
    return position;
}
bool TextIndex::word_at(std::size_t position) const noexcept {
    if (!impl_ || position >= utf16_size()) return false;
    if (position && next_grapheme(previous_grapheme(position)) > position) position = previous_grapheme(position);
    return impl_->word(position);
}
} // namespace ii::gui
