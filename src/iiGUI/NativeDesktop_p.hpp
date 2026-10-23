#pragma once
#include "Desktop.h"
#include "Host.h"
namespace ii::gui::detail {
struct NativeDialog final {
    std::shared_ptr<void> owner;
    std::function<void()> cancel;
    ~NativeDialog(){if(cancel)cancel();}
};
core::Result<std::shared_ptr<NativeDialog>> native_file_dialog(void* parent,FileDialogOptions,UiExecutor,FileDialogCompletion);
#if defined(__linux__)
void bind_desktop(void* display,std::uintptr_t window);
void unbind_desktop(void* display);
bool desktop_event(void* event);
core::Result<ClipboardContent> read_x11_selection(void* display,std::uintptr_t window,unsigned long selection);
struct NativeMenu final {std::shared_ptr<void> owner;std::function<bool(const void*)> key;};
core::Result<NativeMenu> native_app_menu(void* display,std::uintptr_t window,std::vector<AppMenuItem>,UiExecutor);
#endif
}
