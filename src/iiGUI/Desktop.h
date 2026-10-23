#pragma once
#include <iiGUI/Export.h>
#include <iiSDKCore/Core/Result/Result.h>
#include <iiSDKCore/Runtime/Thread.h>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace ii::gui {
// Bytes are encoded PNG/TIFF/JPEG data. The image codec owns pixel decoding;
// FileProvider owns all file access after the user's OS selection/drop.
struct ClipboardContent final {
    std::string text,url,image_mime;
    std::vector<std::string> files;
    std::vector<std::uint8_t> image;
};
[[nodiscard]] IIGUI_EXPORT core::Result<ClipboardContent> read_clipboard();
[[nodiscard]] IIGUI_EXPORT core::Result<void> write_clipboard(const ClipboardContent&);
[[nodiscard]] IIGUI_EXPORT core::Result<void> open_external_url(std::string_view);

enum class FileDialogMode { open,save,directory };
struct FileFilter final {std::string title;std::vector<std::string> extensions;};
struct FileDialogOptions final {
    FileDialogMode mode=FileDialogMode::open;
    std::string title,filename,initial_directory;
    std::vector<FileFilter> filters;
    bool multiple=false;
};
struct FileDialogSelection final {bool canceled=false;std::vector<std::string> paths;};
using FileDialogCompletion=std::function<void(core::Result<FileDialogSelection>)>;
enum class WindowAction { restore,minimize,maximize,toggle_fullscreen,raise };
struct AppShortcut final {std::string key;bool control=false,alt=false,shift=false,meta=true;};
struct AppMenuItem final {
    std::string id,title;
    bool enabled=true,checked=false,separator=false;
    std::optional<AppShortcut> shortcut;
    runtime::Task triggered;
    std::vector<AppMenuItem> children;
};
}
