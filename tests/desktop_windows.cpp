#include <iiGUI/Desktop.h>
#include <iostream>
#include <stdexcept>

namespace {
void rejected(const ii::gui::ClipboardContent& content) {
    const auto result = ii::gui::write_clipboard(content);
    if (result || result.error().code != ii::core::ErrorCode::invalid_argument)
        throw std::runtime_error("Invalid clipboard input must fail before native publication");
}
}

int main() {
    try {
        ii::gui::ClipboardContent content;
        content.files = {""};
        rejected(content);
        content.files = {std::string{char(0xc0), char(0xaf)}};
        rejected(content);
        content.files.assign(257, "C:/example.txt");
        rejected(content);
        content = {};
        content.image = {1};
        content.image_mime = "application/octet-stream";
        rejected(content);
        const auto url = ii::gui::open_external_url("file:///C:/example.txt");
        if (url || url.error().code != ii::core::ErrorCode::invalid_argument)
            throw std::runtime_error("Unsupported URL must fail before native activation");
        std::cout << "Windows desktop input validation passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
