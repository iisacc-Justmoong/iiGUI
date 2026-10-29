#pragma once
#include <iiSDKCore/Core/Result/Result.h>
#include <filesystem>
#include <string>
namespace ii::gui::detail {
struct FontMetadata final {std::string family,style;int weight=400;};
// Bounded SFNT/TTC name and OS/2 metadata. Does not rasterize or load glyphs.
[[nodiscard]] core::Result<FontMetadata> inspect_font_metadata(const std::filesystem::path&,unsigned face=0);
}
