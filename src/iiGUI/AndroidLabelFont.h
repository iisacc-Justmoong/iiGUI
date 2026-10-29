#pragma once
#include <cstdint>
#include <memory>
#include <string_view>
#include <vector>

namespace ii::gui::detail {
struct LabelGlyph final {
    int width=0,height=0,x=0,y=0;
    float advance=0;
    std::vector<std::uint8_t> coverage;
};
// Private, owned, pinned APK font data. This is not an untrusted font-file API.
class AndroidLabelFont final {
public:
    explicit AndroidLabelFont(std::vector<std::uint8_t>,unsigned collection_index=0);
    ~AndroidLabelFont();
    AndroidLabelFont(const AndroidLabelFont&)=delete;
    AndroidLabelFont& operator=(const AndroidLabelFont&)=delete;
    float width(float pixels,std::string_view)const;
    float ascent(float pixels)const;
    float height(float pixels)const;
    float kerning(float pixels,unsigned first,unsigned second)const;
    LabelGlyph glyph(float pixels,unsigned codepoint)const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
