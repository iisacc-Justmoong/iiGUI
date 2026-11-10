#pragma once
#include <iiGUI/NativeWidgets.h>
#include <string>
#include <string_view>

namespace ii::gui::detail {
// The host owns the returned handle only for the lifetime of its mounted tree.
inline core::Result<void> build_legacy_text(WidgetBuilder& builder, std::string_view text, Widget& label) {
    auto root = builder.vstack({}, "iiGUI.legacy.root", {.padding=28});
    if (!root) return std::unexpected(root.error());
    auto created = builder.label(*root, "iiGUI.legacy.text", {.text=std::string(text), .font_size=17});
    if (!created) return std::unexpected(created.error());
    label = *created;
    return {};
}
}
