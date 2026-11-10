#include <iiGUI/DesktopCommon.h>
#include <iostream>
#include <stdexcept>

namespace {
void check(bool value, const char *message) { if (!value) throw std::runtime_error(message); }
void exercise() {
    ii::gui::ClipboardContent content;
    content.text.assign(2 * 1024 * 1024, 'a');
    content.text += " 한글 😀";
    check(ii::gui::detail::valid_clipboard(content), "Clipboard still rejects text larger than 1 MiB");
    content.text.assign(64 * 1024 * 1024, 'a');
    check(ii::gui::detail::valid_clipboard(content), "Clipboard rejected the supported maximum");
    content.text.push_back('a');
    check(!ii::gui::detail::valid_clipboard(content), "Clipboard accepted text larger than the supported maximum");
    content.text.assign("a\0b", 3);
    check(!ii::gui::detail::valid_clipboard(content), "Clipboard accepted embedded NUL");
    content.text.assign("\xff", 1);
    check(!ii::gui::detail::valid_clipboard(content), "Clipboard accepted invalid UTF-8");
}
}
int main() {
    try { exercise(); }
    catch (const std::exception &error) { std::cerr << error.what() << '\n'; return 1; }
    return 0;
}
