#include "WaylandConnection.h"
#include "DesktopCommon.h"
#include <gio/gio.h>
namespace ii::gui {
core::Result<ClipboardContent> read_clipboard(){auto* host=detail::wayland_connection();if(!host)return core::fail(core::ErrorCode::wrong_executor,"Clipboard requires the active Wayland host owner");try{return host->clipboard();}catch(...){return core::fail(core::ErrorCode::failed,"Wayland clipboard read failed",std::current_exception());}}
core::Result<void> write_clipboard(const ClipboardContent& value){auto* host=detail::wayland_connection();if(!host)return core::fail(core::ErrorCode::wrong_executor,"Clipboard requires the active Wayland host owner");try{return host->clipboard(value);}catch(...){return core::fail(core::ErrorCode::failed,"Wayland clipboard write failed",std::current_exception());}}
core::Result<void> open_external_url(std::string_view value){if(!detail::valid_external_url(value))return core::fail(core::ErrorCode::invalid_argument,"Invalid external URL");GError* error=nullptr;std::string url(value);if(!g_app_info_launch_default_for_uri(url.c_str(),nullptr,&error)){std::string message=error?error->message:"No desktop URL handler";if(error)g_error_free(error);return core::fail(core::ErrorCode::failed,std::move(message));}return {};}
}
