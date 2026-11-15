#include <iiGUI/NativeSheetView.h>
#include <iiGUI/WidgetState.h>

#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
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
}

int main() {
    using namespace ii::gui;
    auto tree = std::make_shared<detail::WidgetState>([](ii::runtime::Task task) { task(); });
    auto host = take(tree->content_host({}, "host", {800, 600}));
    std::optional<ii::gui::NativeSheet> sheet;
    ii::gui::SheetInput input;
    input.mobile_platform = false;
    input.configuration.title = "Export";
    input.configuration.description = "Save your file";
    input.configuration.motion_enabled = false;
    int made = 0, opened = 0, closed = 0, canceled = 0, close_made = 0;
    bool reopen_on_close = false;
    good(host.set_content([&](WidgetBuilder& builder) -> ii::core::Result<void> {
        auto root = builder.vstack({}, "root"); if (!root) return std::unexpected(root.error());
        auto created = ii::gui::NativeSheet::create(builder, *root, "sheet", input,
            {.opened = [&] { ++opened; }, .closed = [&] {
                ++closed;
                if (reopen_on_close) { reopen_on_close = false; good(sheet->root().open_at(0, 0)); }
            }, .canceled = [&] { ++canceled; }},
            [&](WidgetBuilder& child, Widget parent) -> ii::core::Result<void> {
                ++made;
                for (int index = 0; index < 30; ++index) {
                    auto item = child.label(parent, "row" + std::to_string(index), {.text = "Retained row"});
                    if (!item) return std::unexpected(item.error());
                }
                auto focusable = child.button(parent, "focusable", {.text = "Focusable"});
                if (!focusable) return std::unexpected(focusable.error());
                return {};
            }, [&](WidgetBuilder& child, Widget parent, std::string_view id, ii::runtime::Task action) {
                ++close_made;
                return child.button(parent, id, {.text = "Close", .tone = ButtonTone::borderless}, std::move(action));
            });
        if (!created) return std::unexpected(created.error()); sheet = *created; return {};
    }));
    auto root = sheet->root(), viewport = sheet->viewport();
    require(close_made == 1, "Sheet ignored its supplied close control");
    (void)take(tree->drawing_snapshot({800, 600})); good(root.open_at(0, 0));
    const auto desktop = take(root.snapshot());
    require(desktop.modal && desktop.bounds.width == 560 && desktop.bounds.height == 420 && made == 1 && opened == 1,
            "Independent desktop Sheet did not open at its preferred geometry");
    auto row = take(root.popup_content("row0")); const auto row_id = take(row.snapshot()).id;
    require(take(viewport.snapshot()).content_height > take(viewport.snapshot()).bounds.height,
            "Sheet did not expose its scrolling content");
    good(viewport.set_scroll_offset(80)); require(take(viewport.snapshot()).scroll_offset > 0, "Sheet did not scroll");
    good(root.close_popup()); require(closed == 1 && canceled == 0, "Programmatic close canceled the Sheet");
    good(root.open_at(0, 0));
    require(made == 1 && take(take(root.popup_content("row0")).snapshot()).id == row_id &&
            take(viewport.snapshot()).scroll_offset == 0, "Sheet did not retain its body or reset scrolling");
    auto mobile = input; mobile.configuration.presentation = ii::gui::SheetPresentation::mobile;
    mobile.configuration.detent = ii::gui::SheetDetent::medium; mobile.configuration.bottom_safe_inset = 34;
    good(sheet->set_options(mobile));
    auto mobile_frame = take(root.snapshot()).bounds;
    require(mobile_frame.width == 800 && mobile_frame.height == 342 && mobile_frame.y == 258 && made == 1,
            "Sheet did not adopt mobile bottom-aligned geometry while retaining its body");
    auto grabber = take(root.popup_content("sheet.grabber"));
    auto grab = take(grabber.snapshot()).bounds;
    require(take(grabber.snapshot()).visible && grab.height > 0, "Mobile Sheet did not expose its grabber");
    auto x = grab.x + grab.width / 2, y = grab.y + grab.height / 2;
    good(tree->input({.kind = WidgetInputKind::pointer_down, .x = x, .y = y}));
    good(tree->input({.kind = WidgetInputKind::pointer_move, .x = x, .y = y + 20}));
    require(take(root.snapshot()).bounds.y == mobile_frame.y + 20, "Sheet did not follow its captured drag");
    good(sheet->set_options(mobile));
    good(tree->input({.kind = WidgetInputKind::pointer_move, .x = x, .y = y + 25}));
    require(take(root.snapshot()).bounds.y == mobile_frame.y + 25, "An unchanged Sheet update interrupted its active drag");
    good(tree->input({.kind = WidgetInputKind::pointer_up, .x = x, .y = y + 20}));
    require(take(root.snapshot()).opened && take(root.snapshot()).bounds.y == mobile_frame.y,
            "Short Sheet drag did not restore the surface");
    grab = take(grabber.snapshot()).bounds; x = grab.x + grab.width / 2; y = grab.y + grab.height / 2;
    good(tree->input({.kind = WidgetInputKind::pointer_down, .x = x, .y = y}));
    good(tree->input({.kind = WidgetInputKind::pointer_move, .x = x, .y = y + 100}));
    good(tree->input({.kind = WidgetInputKind::pointer_up, .x = x, .y = y + 100}));
    require(!take(root.snapshot()).opened && closed == 2 && canceled == 1, "Long Sheet drag did not cancel exactly once");
    auto invalid = mobile; invalid.configuration.preferred_width = -1;
    require(!sheet->set_options(invalid) && take(sheet->options()).configuration == mobile.configuration,
            "Invalid Sheet options replaced its retained state");
    good(root.open_at(0, 0));
    const auto close = take(root.popup_content("sheet.close"));
    const auto bounds = take(close.snapshot()).bounds;
    good(tree->input({.kind = WidgetInputKind::pointer_down, .x = bounds.x + bounds.width / 2, .y = bounds.y + bounds.height / 2}));
    good(tree->input({.kind = WidgetInputKind::pointer_up, .x = bounds.x + bounds.width / 2, .y = bounds.y + bounds.height / 2}));
    require(!take(root.snapshot()).opened && closed == 3 && canceled == 2, "Injected close control did not cancel the sheet");
    good(root.open_at(0, 0));
    good(tree->input({.kind = WidgetInputKind::key_down, .key = WidgetKey::escape}));
    require(!take(root.snapshot()).opened && closed == 4 && canceled == 3, "Escape did not cancel the Sheet exactly once");
    good(root.open_at(0, 0));
    good(tree->input({.kind = WidgetInputKind::pointer_down, .x = 1, .y = 1}));
    good(tree->input({.kind = WidgetInputKind::pointer_up, .x = 1, .y = 1}));
    require(!take(root.snapshot()).opened && closed == 5 && canceled == 4, "Outside press did not cancel the Sheet");
    good(root.open_at(0, 0)); reopen_on_close = true; good(root.cancel_popup());
    require(take(root.snapshot()).opened && closed == 6 && canceled == 4, "Retired cancellation reached a reopened Sheet");
    good(root.close_popup());
    auto without_focus = mobile; without_focus.configuration.show_close_button = false;
    good(sheet->set_options(without_focus)); good(root.open_at(0, 0));
    auto focusable = take(root.popup_content("focusable"));
    good(focusable.focus()); good(focusable.set_visible(false));
    good(tree->input({.kind = WidgetInputKind::key_down, .key = WidgetKey::tab}));
    require(take(root.snapshot()).opened, "Tab closed an empty modal Sheet");
    good(tree->input({.kind = WidgetInputKind::key_down, .key = WidgetKey::escape}));
    require(!take(root.snapshot()).opened && closed == 8 && canceled == 5,
            "Escape did not cancel a modal Sheet without a focused child");
    good(host.clear_content()); require(!row.snapshot() && !sheet->options(), "Sheet retirement retained its body");
    tree->close();

    auto directTree = std::make_shared<detail::WidgetState>([](ii::runtime::Task task) { task(); });
    auto directHost = take(directTree->content_host({}, "host", {800, 700}));
    std::optional<ii::gui::NativeSheet> direct;
    Widget rows;
    good(directHost.set_content([&](WidgetBuilder& builder) -> ii::core::Result<void> {
        auto made=ii::gui::NativeSheet::create(builder, {}, "direct", input, {},
            [&](WidgetBuilder& child, Widget parent) -> ii::core::Result<void> {
                auto host=child.content_host(parent,"rows",{});
                if(!host)return std::unexpected(host.error());rows=*host;
                return {};
            });
        if(!made)return std::unexpected(made.error());direct=*made;return {};
    }));
    good(direct->root().open_at(0,0));
    (void)take(directTree->drawing_snapshot({800,700}));
    std::vector<KeyedContentItem> items;
    for(int index=0;index<4;++index)items.push_back({std::to_string(index),[index](WidgetBuilder& child)->ii::core::Result<void>{
        auto column=child.vstack({},"column",{.alignment=StackAlignment::stretch});if(!column)return std::unexpected(column.error());
        for(int item=0;item<8;++item){auto label=child.label(*column,"item"+std::to_string(index)+"-"+std::to_string(item),{.text="Long retained content"});
            if(!label)return std::unexpected(label.error());}
        return {};
    }});
    good(rows.set_keyed_content(std::move(items),{.layout={.alignment=StackAlignment::stretch}}));
    good(directHost.set_extent({360,260}));
    (void)take(directTree->drawing_snapshot({360,260}));
    auto frame=take(direct->root().snapshot()).bounds;
    auto scroll=take(direct->viewport().snapshot());
    require(frame.x>=0 && frame.y>=0 && frame.x+frame.width<=360 && frame.y+frame.height<=260,
            "A root popup did not follow its embedded host's window resize");
    require(scroll.bounds.height>0 && scroll.content_height>scroll.bounds.height,
            "A resized root Sheet lost its scrollable content");
    good(directTree->input({.kind=WidgetInputKind::scroll,.x=scroll.bounds.x+10,
                            .y=scroll.bounds.y+10,.scroll_y=60}));
    require(take(direct->viewport().snapshot()).scroll_offset>0,
            "A resized root Sheet did not receive user scrolling");
    good(directHost.clear_content());direct.reset();directTree->close();
}
