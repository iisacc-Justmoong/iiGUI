#include <iiGUI/WidgetState.h>

#include <memory>
#include <stdexcept>

namespace {
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
template<class T>T take(ii::core::Result<T> value) {
    if (!value) throw std::runtime_error(value.error().message);
    return std::move(*value);
}
void good(ii::core::Result<void> value) { if (!value) throw std::runtime_error(value.error().message); }
struct Lifetime final {
    int& retired;
    explicit Lifetime(int& value) : retired(value) {}
    ~Lifetime() { ++retired; }
};
}

int main() {
    namespace gui = ii::gui;
    auto tree = std::make_shared<gui::detail::WidgetState>([](ii::runtime::Task task) { task(); });
    auto host = take(tree->content_host({}, "host", {600, 400}));
    gui::Widget popup, anchor;
    int made = 0, retired = 0;
    good(host.set_content([&](gui::WidgetBuilder& builder) -> ii::core::Result<void> {
        auto root = builder.vstack({}, "root"); if (!root) return std::unexpected(root.error());
        auto candidate = builder.popover(*root, "popup", {.width = 300, .placement = gui::PopoverPlacement::centered});
        if (!candidate) return std::unexpected(candidate.error()); popup = *candidate;
        auto chrome = builder.vstack(popup, "chrome"); if (!chrome) return std::unexpected(chrome.error());
        auto body = builder.vstack(*chrome, "body"); if (!body) return std::unexpected(body.error()); anchor = *body;
        auto attached = popup.set_popup_content_at(anchor, [&](gui::WidgetBuilder& child) -> ii::core::Result<void> {
            ++made;
            if (auto retained = child.retain(std::make_shared<Lifetime>(retired)); !retained) return retained;
            auto label = child.label({}, "child", {.text = "Body"});
            if (!label) return std::unexpected(label.error());
            return {};
        });
        if (!attached) return attached;
        require(!popup.open_at(0, 0), "An unpublished popup opened during construction");
        return {};
    }));
    good(popup.open_at(0, 0));
    auto first = take(popup.popup_content("child"));
    const auto first_snapshot = take(first.snapshot());
    require(made == 1 && retired == 0 && first_snapshot.text == "Body", "First popup activation failed");

    auto rejected = host.set_content([&](gui::WidgetBuilder& builder) -> ii::core::Result<void> {
        auto root = builder.vstack({}, "replacement"); if (!root) return std::unexpected(root.error());
        return ii::core::fail(ii::core::ErrorCode::failed, "Reject candidate");
    });
    require(!rejected && bool(first.snapshot()) && take(popup.snapshot()).opened,
            "A rejected replacement retired the previously published popup");
    good(popup.close_popup());
    require(retired == 1 && !first.snapshot(), "Closing did not retire the popup body");

    rejected = host.set_content([&](gui::WidgetBuilder& builder) -> ii::core::Result<void> {
        auto root = builder.vstack({}, "replacement"); if (!root) return std::unexpected(root.error());
        auto changed = popup.set_popup_content_at(anchor, {});
        require(!changed && changed.error().code == ii::core::ErrorCode::busy,
                "A staged candidate modified a live popup factory");
        return ii::core::fail(ii::core::ErrorCode::failed, "Reject candidate");
    });
    require(!rejected, "The second candidate unexpectedly committed");
    good(popup.open_at(0, 0));
    auto second = take(popup.popup_content("child"));
    require(made == 2 && retired == 1 && take(second.snapshot()).id != first_snapshot.id,
            "Reopening did not create a fresh popup body");
    good(popup.close_popup()); require(retired == 2, "Second body did not retire");
    tree->close();
}
