#include <iiGUI/NativeListFooterView.h>
#include <iiGUI/WidgetState.h>

#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
template<class T> T take(ii::core::Result<T> value) {
    if (!value) { if (value.error().cause) std::rethrow_exception(value.error().cause); throw std::runtime_error(value.error().message); }
    return std::move(*value);
}
void good(ii::core::Result<void> value) {
    if (!value) { if (value.error().cause) std::rethrow_exception(value.error().cause); throw std::runtime_error(value.error().message); }
}
class Command final : public ii::gui::FooterCommand {
public:
    std::function<void()> action;
    void invoke() const override { if (action) action(); }
};
struct Source final : ii::gui::ButtonIconSource {
    std::shared_ptr<const ii::gui::RasterImage> image;
    mutable unsigned calls=0;std::uint64_t version=0;
    std::uint64_t revision() const noexcept override {return version;}
    ii::core::Result<std::shared_ptr<const ii::gui::RasterImage>> resolve(std::string_view name,double size) const override {
        ++calls;
        if(size!=18 || (name!="addFile" && name!="generaldelete" && name!="settings"))
            return ii::core::fail(ii::core::ErrorCode::not_found,"Missing icon");
        return image;
    }
};
}
int main() {
    using namespace ii::gui;
    ii::gui::FooterInput input;
    input.configuration.slots[0].icon_name="addFile";input.configuration.slots[1].icon_name="generaldelete";
    input.configuration.slots[2].icon_name="settings";input.configuration.slots[2].menu_indicator=true;
    auto source=std::make_shared<Source>();source->image=take(rgba_image(2,2,std::vector<std::uint8_t>(16,255)));input.icon_source=source;
    auto command = std::make_shared<Command>(); input.configuration.slots[0].command = command;
    auto tree = std::make_shared<detail::WidgetState>([](ii::runtime::Task task) { task(); });
    auto host = take(tree->content_host({}, "host", {200, 80}));
    std::optional<ii::gui::ListFooterView> footer; std::vector<std::string> events; bool retire = false, reconfigure = false;
    command->action = [&] { events.push_back("command"); if (reconfigure) { auto next = input; next.configuration.slots[0].text = "Changed"; good(footer->set_options(next)); }
                            if (retire) good(host.clear_content()); };
    good(host.set_content([&](WidgetBuilder& builder) -> ii::core::Result<void> {
        auto column = builder.vstack({}, "column", {.spacing=0, .alignment=StackAlignment::leading}); if (!column) return std::unexpected(column.error());
        auto value = ii::gui::ListFooterView::create(builder, *column, "footer", input,
            {.clicked = [&](std::size_t index, const auto& slot) { events.push_back(std::to_string(index) + ":" + slot.icon_name); }});
        if (!value) return std::unexpected(value.error()); footer = *value; return {};
    }));
    const auto cached=source->calls;good(footer->set_options(input));
    require(source->calls==cached,"Unchanged footer re-resolved its icons");
    ++source->version;++input.icon_revision;good(footer->set_options(input));
    require(source->calls==cached+3,"Footer did not refresh icons after an explicitly published source revision");
    auto root = take(footer->root().snapshot()); auto first = take(footer->slot(0)); auto second = take(footer->slot(1)); auto menu = take(footer->slot(2));
    require(root.size.width == 88 && root.size.height == 26 && take(menu.interaction()).visible && !footer->slot(3),
            "List footer lost its default geometry or menu slot");
    auto customized=input;customized.configuration.menu_indicator_text="More";customized.configuration.menu_indicator_width=30;
    good(footer->set_options(customized));
    require(take(menu.action_options()).text=="More" && take(footer->root().snapshot()).size.width==100,
            "Footer did not apply its injected menu indicator and layout");
    good(footer->set_options(input));
    auto drawing = take(tree->drawing_snapshot({200, 80})); std::size_t images = drawing.images.size();
    for (const auto& layer : drawing.overlays) images += layer.images.size();
    require(images == 3, "List footer did not draw all default icons");
    good(first.activate()); good(second.activate());
    require(events == std::vector<std::string>{"command", "0:addFile", "1:generaldelete"},
            "List footer did not deliver its slot command and callback in order");
    auto invalid = input; invalid.configuration.slots[1].icon_name = "missing-icon";
    require(!footer->set_options(invalid) && take(footer->options()) == input, "An invalid icon replaced the working footer");
    auto next = input; next.configuration.interactive = false; good(footer->set_options(next)); good(first.activate());
    require(events.size() == 3, "A noninteractive footer delivered a slot action");
    next.configuration.interactive = true; next.configuration.slots[1].visible = false; good(footer->set_options(next));
    require(!take(second.interaction()).visible && take(footer->root().snapshot()).size.width == 66,
            "A hidden footer slot remained visible or occupied layout space");
    next.configuration.width = 4; good(footer->set_options(next));
    require(!take(first.interaction()).visible && !take(menu.interaction()).visible, "Zero-sized footer slots remained visible");
    next.configuration.width = 0; good(footer->set_options(next));
    require(take(first.interaction()).visible && take(menu.interaction()).visible, "Footer slots did not recover after narrowing");
    good(footer->set_options(input)); bool wrong_thread = false;
    std::thread worker([&] { auto result = footer->set_options(input); wrong_thread = !result && result.error().code == ii::core::ErrorCode::wrong_executor; });
    worker.join(); require(wrong_thread, "List footer accepted a foreign-thread update");
    reconfigure = true; good(first.activate()); reconfigure = false;
    require(events.size() == 4 && events.back() == "command", "A reconfigured footer delivered a stale slot callback");
    good(footer->set_options(input)); retire = true; good(first.activate());
    require(events.size() == 5 && events.back() == "command" && !footer->set_options(input),
            "A retiring footer delivered a stale callback or accepted updates");
    tree->close();
}
