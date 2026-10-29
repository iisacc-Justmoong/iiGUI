#pragma once
#include "FontDatabase.h"
#include <filesystem>
#if defined(_WIN32)
#include <dwrite.h>
#include <wrl/client.h>
#endif
namespace ii::gui::detail {
std::shared_ptr<FontProvider> native_font_provider(std::string resource_directory);
std::filesystem::path font_resource_directory(std::string_view configured);
#if defined(_WIN32)
Microsoft::WRL::ComPtr<IDWriteFontCollection> native_font_collection();
#endif
#if defined(__ANDROID__)
class AndroidLabelFont;
std::shared_ptr<AndroidLabelFont> native_android_font(std::string_view family,int weight);
#endif
}
