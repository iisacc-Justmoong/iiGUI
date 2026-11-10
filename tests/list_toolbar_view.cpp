#include <iiGUI/NativeListToolbarView.h>
#include <iiGUI/WidgetState.h>

#include <memory>
#include <functional>
#include <optional>
#include <stdexcept>
#include <thread>
#include <utility>
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
struct Source final : ii::gui::ButtonIconSource {
    std::shared_ptr<const ii::gui::RasterImage> image;mutable unsigned calls=0;std::uint64_t version=0;
    mutable std::function<void()> on_resolve;
    std::uint64_t revision()const noexcept override{return version;}
    ii::core::Result<std::shared_ptr<const ii::gui::RasterImage>> resolve(std::string_view name,double size)const override {
        ++calls;if(auto callback=std::exchange(on_resolve,{}))callback();
        if(size!=18 || (name!="home" && name!="generalchevronRight"))
            return ii::core::fail(ii::core::ErrorCode::not_found,"Missing icon");return image;
    }
};
void reentrant_current_input(){
    using namespace ii::gui;
    auto source=std::make_shared<Source>();source->image=take(rgba_image(2,2,std::vector<std::uint8_t>(16,255)));
    ListBarInput input;input.configuration.icons={"home","",""};input.icon_source=source;
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"current-host",{200,100}));std::optional<ListToolbarView> toolbar;
    good(host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto value=ListToolbarView::create(builder,{},"toolbar",input);if(!value)return std::unexpected(value.error());toolbar=*value;return {};
    }));
    auto older=input;++source->version;++older.icon_revision;
    source->on_resolve=[&]{good(toolbar->set_options(input));};
    good(toolbar->set_options(older));
    require(take(toolbar->options()).icon_revision==0,"An older list-toolbar preparation overrode a reentrant request for the current input");
    tree->close();
}
void retire_during_resolution(bool destroy_handle){
    using namespace ii::gui;
    auto source=std::make_shared<Source>();source->image=take(rgba_image(2,2,std::vector<std::uint8_t>(16,255)));
    ListBarInput input;input.configuration.icons={"home","","generalchevronRight"};input.icon_source=source;
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"retire-host",{200,100}));std::optional<ListToolbarView> toolbar;
    good(host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto value=ListToolbarView::create(builder,{},"toolbar",input);if(!value)return std::unexpected(value.error());toolbar=*value;return {};
    }));
    auto next=input;++source->version;++next.icon_revision;
    const auto calls=source->calls;source->on_resolve=[&]{good(host.clear_content());if(destroy_handle)toolbar.reset();};
    good(toolbar->set_options(next));
    require(source->calls==calls+1,"Retired list toolbar continued resolving remaining icons");
    std::weak_ptr<Source> weak=source;input.icon_source.reset();next.icon_source.reset();source.reset();
    require(weak.expired(),"Retired list toolbar retained its icon source");
    if(!destroy_handle)require(!toolbar->options(),"Retired list toolbar remained active");tree->close();
}
void reentrant_layout(){
    using namespace ii::gui;
    auto source=std::make_shared<Source>();source->image=take(rgba_image(2,2,std::vector<std::uint8_t>(16,255)));
    ListBarInput initial;initial.configuration.icons={"home","","generalchevronRight"};initial.icon_source=source;
    auto newest=initial;newest.configuration.width=120;
    std::optional<ListToolbarView> toolbar;unsigned until_reentry=0;
    detail::WidgetFrameHooks hooks;hooks.accessibility_changed=[&]{
        if(until_reentry&&--until_reentry==0)good(toolbar->set_options(newest));
    };
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();},WidgetAppearance{},hooks);
    auto host=take(tree->content_host({},"reentrant-host",{200,100}));
    good(host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto value=ListToolbarView::create(builder,{},"toolbar",initial);if(!value)return std::unexpected(value.error());toolbar=*value;return {};
    }));
    (void)take(tree->drawing_snapshot({200,100}));
    auto older=initial;older.configuration.width=20;until_reentry=4;good(toolbar->set_options(older));
    require(!until_reentry&&take(toolbar->options()).configuration.width==120&&take(take(toolbar->slot(2)).interaction()).visible&&
            take(take(toolbar->slot(2)).snapshot()).size.width>0,
            "An older list-toolbar layout overwrote a newer reentrant update");
    tree->close();
}
}
int main() {
    reentrant_current_input();retire_during_resolution(false);retire_during_resolution(true);
    reentrant_layout();
    using namespace ii::gui;
    ii::gui::ListBarInput input; input.configuration.icons = {"home", "", "generalchevronRight"};
    auto source=std::make_shared<Source>();source->image=take(rgba_image(2,2,std::vector<std::uint8_t>(16,255)));input.icon_source=source;
    auto tree = std::make_shared<detail::WidgetState>([](ii::runtime::Task task) { task(); });
    auto host = take(tree->content_host({}, "host", {200, 100}));
    std::optional<ii::gui::ListToolbarView> toolbar; std::vector<std::string> events; bool remove = false;
    good(host.set_content([&](WidgetBuilder& builder) -> ii::core::Result<void> {
        auto column = builder.vstack({}, "column", {.spacing=0, .alignment=StackAlignment::leading});
        if (!column) return std::unexpected(column.error());
        auto value = ii::gui::ListToolbarView::create(builder, *column, "toolbar", input,
            {.clicked = [&](std::size_t index, std::string name) { events.push_back(std::to_string(index) + ":" + name); if (remove) good(host.clear_content()); }});
        if (!value) return std::unexpected(value.error()); toolbar = *value; return {};
    }));
    const auto cached=source->calls;good(toolbar->set_options(input));
    require(source->calls==cached,"Unchanged list toolbar re-resolved icons");
    ++source->version;++input.icon_revision;good(toolbar->set_options(input));
    require(source->calls==cached+2,"List toolbar did not refresh the published icon revision");
    const auto root = take(toolbar->root().snapshot()); auto first = take(toolbar->slot(0)); auto blank = take(toolbar->slot(1));
    require(root.size.width == 90 && root.size.height == 30 && !take(blank.interaction()).visible &&
            take(first.action_options()).accessibility_name == "home", "List toolbar lost its geometry, labels or empty slot");
    auto drawing = take(tree->drawing_snapshot({200, 100})); std::size_t images = drawing.images.size();
    for (const auto& layer : drawing.overlays) images += layer.images.size();
    require(images == 2, "List toolbar did not draw its available icons");
    good(first.activate()); good(take(toolbar->slot(2)).activate());
    require(events == std::vector<std::string>{"0:home", "2:generalchevronRight"}, "List toolbar sent an incorrect slot action");
    auto next = input; next.configuration.icons[0] = "missing-icon";
    require(!toolbar->set_options(next) && take(toolbar->options()) == input,
            "An invalid toolbar icon replaced the committed configuration");
    next = input; next.configuration.interactive = false; good(toolbar->set_options(next)); good(first.activate());
    require(events.size() == 2, "A noninteractive toolbar still activated its slot");
    next.configuration.interactive = true; next.configuration.width = 30; good(toolbar->set_options(next));
    auto narrowed = take(first.snapshot());
    require(narrowed.size.width <= 22 && narrowed.bounds.x >= 0, "Narrow toolbar slot exceeded its available extent");
    next.configuration.width = 20; good(toolbar->set_options(next));
    require(!take(first.interaction()).visible, "A zero-sized slot remained accessible");
    next.configuration.width = 90; good(toolbar->set_options(next));
    require(take(first.interaction()).visible, "A slot did not reappear when space returned");
    bool rejected = false;
    std::thread worker([&] { auto result = toolbar->set_options(input); rejected = !result && result.error().code == ii::core::ErrorCode::wrong_executor; });
    worker.join(); require(rejected, "List toolbar accepted a foreign-thread update");
    remove = true; good(first.activate()); require(!toolbar->set_options(input), "Toolbar survived removal inside its own callback");
    tree->close();
}
