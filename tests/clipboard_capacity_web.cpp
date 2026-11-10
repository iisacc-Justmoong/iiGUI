#include <iiGUI/Desktop.h>
#include <iostream>
#include <stdexcept>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

int main() {
    int status = 0;
    try {
        ii::gui::ClipboardContent content;
        content.text.assign(2 * 1024 * 1024, 'w');
        content.text += " 한글 😀";
        const auto written = ii::gui::write_clipboard(content);
        if (!written) throw std::runtime_error(written.error().message);
        const auto read = ii::gui::read_clipboard();
        if (!read || read->text != content.text) throw std::runtime_error("Web clipboard truncated its report");
        content.text.assign(ii::gui::maximum_clipboard_text_bytes + 1, 'x');
        if (ii::gui::write_clipboard(content)) throw std::runtime_error("Web clipboard accepted an oversized report");
        const auto retained = ii::gui::read_clipboard();
        if (!retained || retained->text != read->text) throw std::runtime_error("Rejected web write changed the clipboard");
    } catch (const std::exception &error) { std::cerr << error.what() << '\n'; status = 1; }
#ifdef __EMSCRIPTEN__
    emscripten_force_exit(status);
#endif
    return status;
}
