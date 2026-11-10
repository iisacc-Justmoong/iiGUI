#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace ii::gui::detail {
struct AndroidFontSource final {
    enum class Kind { none, asset, file };
    Kind kind = Kind::none;
    std::string path;
};
struct LabelGlyph final {
    int width=0,height=0,x=0,y=0;
    float advance=0;
    std::vector<std::uint8_t> coverage;
};
// Private, owned, pinned APK font data. This is not an untrusted font-file API.
class AndroidLabelFont final {
public:
    explicit AndroidLabelFont(std::vector<std::uint8_t>, unsigned collection_index=0,
                              AndroidFontSource source={});
    ~AndroidLabelFont();
    AndroidLabelFont(const AndroidLabelFont&)=delete;
    AndroidLabelFont& operator=(const AndroidLabelFont&)=delete;
    float width(float pixels,std::string_view)const;
    float ascent(float pixels)const;
    float height(float pixels)const;
    float kerning(float pixels,unsigned first,unsigned second)const;
    LabelGlyph glyph(float pixels,unsigned codepoint)const;
    [[nodiscard]] const AndroidFontSource& source() const noexcept;
    [[nodiscard]] unsigned collection_index() const noexcept;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
