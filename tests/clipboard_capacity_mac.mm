#import <AppKit/AppKit.h>
#include <iiGUI/DesktopCommon.h>
#include <iostream>
#include <stdexcept>

namespace {
void check(bool value, const char *message) { if (!value) throw std::runtime_error(message); }
void exercise() {
    @autoreleasepool {
        NSPasteboard *board = [NSPasteboard pasteboardWithUniqueName];
        struct Release { NSPasteboard *board; ~Release() { [board releaseGlobally]; } } release{board};
        ii::gui::ClipboardContent content;
        content.text.assign(2 * 1024 * 1024, 'x');
        content.text += " 한글 😀";
        const auto written = ii::gui::detail::write_pasteboard((__bridge void *)board, content);
        check(bool(written), "The isolated native pasteboard rejected a large report");
        const auto read = ii::gui::detail::read_pasteboard((__bridge void *)board);
        check(read && read->text == content.text, "The isolated native pasteboard truncated a large report");
        content.text.assign("a\0b", 3);
        check(!ii::gui::detail::write_pasteboard((__bridge void *)board, content), "Invalid text was accepted");
        const auto preserved = ii::gui::detail::read_pasteboard((__bridge void *)board);
        check(preserved && preserved->text == read->text, "Failed clipboard write replaced existing content");
    }
}
}
int main() {
    try { exercise(); }
    catch (const std::exception &error) { std::cerr << error.what() << '\n'; return 1; }
    return 0;
}
