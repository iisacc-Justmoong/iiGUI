#pragma once
#include <iiGUI/AndroidLabelFont.h>
#include <expected>
#include <memory>
#include <string>
#include <string_view>
namespace ii::gui {
struct FakeFontError { std::string message; };
class FontDatabase final {
public:
    static std::shared_ptr<FontDatabase> application() { return std::make_shared<FontDatabase>(); }
    std::expected<int, FakeFontError> load_bundled_fonts() { return 1; }
    std::string resolve_family(std::string_view family) { return std::string(family); }
};
namespace detail { std::shared_ptr<AndroidLabelFont> native_android_font(std::string_view, int); }
}
