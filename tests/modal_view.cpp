#include <iiGUI/NativeModalView.h>
#include <iiGUI/WidgetState.h>

#include <memory>
#include <limits>
#include <optional>
#include <stdexcept>
#include <thread>
#include <vector>

namespace {
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
template<class T>T take(ii::core::Result<T> value) {
    if (!value) { if (value.error().cause) std::rethrow_exception(value.error().cause); throw std::runtime_error(value.error().message); }
    return std::move(*value);
}
void good(ii::core::Result<void> value) {
    if (!value) { if (value.error().cause) std::rethrow_exception(value.error().cause); throw std::runtime_error(value.error().message); }
}
struct Lifetime final {
    int& count;
    explicit Lifetime(int& value) : count(value) {}
    ~Lifetime() { ++count; }
};
}

int main() {
    using namespace ii::gui;
    std::optional<ii::gui::NativeModal> modal;
    bool reenter = false, reopen = false;
    ii::gui::ModalInput input;
    input.configuration.motion_enabled = false;
    input.configuration.title = "Initial";
    input.configuration.message = "Fallback";
    input.icon = take(rgba_image(2, 2, std::vector<std::uint8_t>(16, 255)));
    input.style.title.font_size = 22;
    input.style.title.line_height = 22;
    input.style.title.font_weight = 700;
    input.style.description.font_size = 13;
    input.style.text_spacing = 8;
    input.style.icon_padding = 8;
    input.style.icon_border_width = 1;
    input.configuration.action_count = 3;
    input.configuration.action_text = {"Apply", "Cancel", "More"};
    auto latest = input;
    latest.configuration.title = "Newest";
    latest.configuration.action_text[0] = "Save";
    detail::WidgetFrameHooks hooks;
    hooks.accessibility_changed = [&] { if (reenter) { reenter = false; good(modal->set_options(latest)); } };
    auto tree = std::make_shared<detail::WidgetState>([](ii::runtime::Task task) { task(); }, WidgetAppearance{}, hooks);
    auto host = take(tree->content_host({}, "host", {800, 600}));
    int made = 0, retired = 0, closed = 0, canceled = 0;
    std::vector<int> actions;
    good(host.set_content([&](auto& builder) -> ii::core::Result<void> {
        auto stack = builder.vstack({}, "root"); if (!stack) return std::unexpected(stack.error());
        ii::gui::NativeModalCallbacks callbacks;
        for (int index = 0; index < 3; ++index) callbacks.actions[unsigned(index)] = [&, index] { actions.push_back(index); };
        callbacks.closed = [&] { ++closed; if (reopen) { reopen = false; good(modal->root().open_at(0, 0)); } };
        callbacks.canceled = [&] { ++canceled; };
        auto created = ii::gui::NativeModal::create(builder, *stack, "dialog", input, std::move(callbacks),
            [&](auto& child, auto parent) -> ii::core::Result<void> {
                ++made;
                if (auto retained = child.retain(std::make_shared<Lifetime>(retired)); !retained) return retained;
                auto label = child.label(parent, "editor", {.text = "Body"});
                if (!label) return std::unexpected(label.error()); return {};
            });
        if (!created) return std::unexpected(created.error()); modal = *created; return {};
    }));
    auto root = modal->root();
    (void)take(tree->drawing_snapshot({800, 600}));
    good(root.open_at(0, 0));
    auto snapshot = take(root.snapshot());
    if (!snapshot.modal || !snapshot.dim || snapshot.bounds.width != 752 || snapshot.bounds.height < 210) {
        throw std::runtime_error("Modal popup geometry: modal=" + std::to_string(snapshot.modal) +
            " dim=" + std::to_string(snapshot.dim) + " width=" + std::to_string(snapshot.bounds.width) +
            " height=" + std::to_string(snapshot.bounds.height));
    }
    auto body = take(root.popup_content("editor"));
    const auto body_id = take(body.snapshot()).id;
    const auto drawing = take(tree->drawing_snapshot({800, 600}));
    bool has_icon = false;
    const auto check_images = [&](const auto& images) {
        for (const auto& image : images) has_icon |= image.image && image.image->width() == 2;
    };
    check_images(drawing.images); for (const auto& layer : drawing.overlays) check_images(layer.images);
    require(has_icon, "Modal did not render its application-supplied icon");
    auto primary = take(root.popup_content("dialog.action-0"));
    auto secondary = take(root.popup_content("dialog.action-1"));
    require(take(take(root.popup_content("dialog.title")).snapshot()).font_size == 22,
            "Modal title did not use its inherited subtitle size");
    require(take(primary.snapshot()).visible && take(secondary.snapshot()).visible && made == 1,
            "Modal did not publish its action chrome and body");
    good(primary.activate()); require(actions == std::vector<int>{0} && take(root.snapshot()).opened,
            "Modal action was not delivered or closed the popup automatically");

    auto updated = input;
    updated.configuration.description = "Updated";
    updated.configuration.action_enabled[1] = false;
    updated.configuration.preferred_width = 600;
    good(modal->set_options(updated));
    good(secondary.activate());
    require(actions.size() == 1 && take(root.snapshot()).bounds.width == 600 &&
            take(root.popup_content("dialog.description")).snapshot()->text == "Updated" &&
            take(body.snapshot()).id == body_id && made == 1,
            "Modal update lost the body or ignored its changed options");
    require(!modal->set_content({}), "An open modal accepted a content factory replacement");
    auto invalid = updated; invalid.configuration.minimum_width = 900; invalid.configuration.maximum_width = 500;
    require(!modal->set_options(invalid) && take(modal->options()).configuration == updated.configuration && bool(body.snapshot()),
            "An invalid modal update changed the live state");
    invalid = updated; invalid.style.text_spacing = std::numeric_limits<double>::quiet_NaN();
    require(!modal->set_options(invalid) && take(modal->options()).style == updated.style,
            "Invalid modal style replaced the live state");
    invalid = updated; invalid.icon.reset(); invalid.configuration.icon_name = "missing-modal-icon";
    require(!modal->set_options(invalid) && take(modal->options()).configuration == updated.configuration,
            "An unresolved modal icon replaced the live configuration");
    auto older = updated; older.configuration.title = "Older"; reenter = true;
    good(modal->set_options(older));
    require(!reenter && take(modal->options()).configuration.title == "Newest" &&
            take(root.popup_content("dialog.title")).snapshot()->text == "Newest",
            "An older modal update overwrote a reentrant update");

    reopen = true; good(root.cancel_popup());
    require(closed == 1 && canceled == 0 && take(root.snapshot()).opened && made == 2 && retired == 1 && !body.snapshot(),
            "A stale cancel reached a reopened modal or kept its old body");
    auto next_body = take(root.popup_content("editor"));
    require(take(next_body.snapshot()).id != body_id, "Modal reused its old content activation");
    good(root.cancel_popup());
    require(closed == 2 && canceled == 1 && retired == 2, "Modal cancellation did not retire and notify once");
    good(modal->set_content([](auto& child, auto parent) -> ii::core::Result<void> {
        auto label = child.label(parent, "replacement", {.text = "New body"});
        if (!label) return std::unexpected(label.error());
        return {};
    }));
    good(root.open_at(0, 0));
    require(take(take(root.popup_content("replacement")).snapshot()).text == "New body" &&
            !root.popup_content("editor"), "A closed modal did not adopt its replacement content factory");
    good(root.close_popup());
    bool rejected = false;
    std::thread worker([&] { auto result = modal->set_options(input); rejected = !result && result.error().code == ii::core::ErrorCode::wrong_executor; });
    worker.join(); require(rejected, "Modal accepted a foreign-thread update");
    good(host.clear_content()); require(!modal->options(), "Retired modal accepted access"); tree->close();
}
