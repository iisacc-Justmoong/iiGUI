#pragma once
#include <string>
namespace ii::gui::detail {
struct AndroidFontSource final {
    enum class Kind { none, asset, file };
    Kind kind = Kind::asset;
    std::string path = "fonts/Pretendard-Regular.ttf";
};
class AndroidLabelFont final {
public:
    AndroidFontSource data;
    unsigned index = 0;
    const AndroidFontSource& source() const noexcept { return data; }
    unsigned collection_index() const noexcept { return index; }
};
}
