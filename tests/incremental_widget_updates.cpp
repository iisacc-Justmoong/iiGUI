#include "../src/Widgets/WidgetState.h"
#include "../src/Widgets/NativeColorSurfaceView.h"
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
namespace gui = ii::gui;
namespace core = ii::core;
void check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
void good(core::Result<void> value) { if (!value) throw std::runtime_error(value.error().message); }
template<class T> T take(core::Result<T> value) {
    if (!value) throw std::runtime_error(value.error().message);
    return std::move(*value);
}
auto state(gui::detail::WidgetFrameHooks hooks = {}) {
    return std::make_shared<gui::detail::WidgetState>([](ii::runtime::Task task) { task(); }, gui::WidgetAppearance{}, std::move(hooks));
}

void options() {
    unsigned changes = 0;
    gui::detail::WidgetFrameHooks hooks;
    hooks.accessibility_changed = [&] { ++changes; };
    auto tree = state(std::move(hooks));
    auto host = take(tree->content_host({}, "host", {200, 100}));
    gui::Widget action;
    good(host.set_content([&](gui::WidgetBuilder& builder) -> core::Result<void> {
        action = take(builder.action_surface({}, "action", {.text="Action", .accessibility_name="Action"}));
        return {};
    }));
    auto original = take(action.action_options());
    changes = 0;
    good(action.set_action_options(original));
    check(changes == 0, "Unchanged action options published an accessibility update");
    auto visual = original;
    visual.text_color = gui::WidgetColor{.2, .3, .4, 1};
    good(action.set_action_options(visual));
    check(changes == 0 && take(action.snapshot()).foreground_color == visual.text_color,
          "Visual-only action update changed accessibility or failed to update the view");
    visual.accessibility_description = "Updated description";
    good(action.set_action_options(visual));
    check(changes == 1 && take(action.snapshot()).accessibility_description == "Updated description",
          "Changed action semantics were not delivered once");
    good(action.set_action_options(visual));
    check(changes == 1, "Repeated action semantics were republished");
    tree->close();
}

void non_action() {
    auto tree = state();
    auto root = take(tree->absolute_stack({}, "root", {200, 100}));
    int downs = 0, ups = 0;
    good(root.set_input_handler([&](const gui::WidgetInput& input) -> core::Result<bool> {
        if (input.kind == gui::WidgetInputKind::pointer_down) ++downs;
        if (input.kind == gui::WidgetInputKind::pointer_up) ++ups;
        return false;
    }));
    (void)take(tree->drawing_snapshot({200, 100}));
    good(tree->input({.kind=gui::WidgetInputKind::pointer_down, .x=20, .y=20, .buttons=1}));
    good(tree->input({.kind=gui::WidgetInputKind::pointer_up, .x=20, .y=20}));
    check(downs == 1 && ups == 1, "Non-action input handler lost a pointer event");
    tree->close();
}

void focused_editor() {
    auto tree = state();
    auto host = take(tree->content_host({}, "host", {240, 100}));
    gui::Widget first, second, plain;
    unsigned first_reads=0, second_reads=0;
    const auto provider = [](unsigned& reads) {
        auto result = std::make_shared<gui::TextInputProvider>();
        result->state = [&reads]() -> core::Result<gui::TextInputState> {
            ++reads; return gui::TextInputState{.length=0, .caret={1,1,1,12}};
        };
        result->read = [](gui::TextRange range) -> core::Result<gui::TextContent> {
            return gui::TextContent{range, {}};
        };
        return result;
    };
    good(host.set_content([&](gui::WidgetBuilder& builder) -> core::Result<void> {
        auto root=take(builder.absolute_stack({}, "root", {240,100}));
        first=take(builder.absolute_stack(root, "first", {80,30}));
        second=take(builder.absolute_stack(root, "second", {80,30}));
        plain=take(builder.button(root, "plain", {.text="Button"}));
        good(first.set_frame({0,0,80,30})); good(second.set_frame({80,0,80,30}));
        good(plain.set_frame({160,0,80,30}));
        good(first.set_text_input_provider(provider(first_reads)));
        good(second.set_text_input_provider(provider(second_reads)));
        return {};
    }));
    (void)take(tree->drawing_snapshot({240,100}));
    first_reads=second_reads=0;
    check(!take(tree->focused_text_editor()) && first_reads==0 && second_reads==0,
          "Unfocused editor lookup read unrelated providers");
    good(first.focus()); first_reads=second_reads=0;
    auto current=take(tree->focused_text_editor());
    check(current && current->id.ends_with("first") && current->focused && current->text_input &&
          first_reads==1 && second_reads==0, "Focused editor lookup traversed unrelated fields or lost nested focus");
    good(second.focus()); first_reads=second_reads=0;
    current=take(tree->focused_text_editor());
    check(current && current->id.ends_with("second") && first_reads==0 && second_reads==1,
          "Focused editor lookup returned a stale field");
    good(plain.focus()); first_reads=second_reads=0;
    check(!take(tree->focused_text_editor()) && first_reads==0 && second_reads==0,
          "A non-editor focus caused unrelated text queries");
    good(host.clear_content());
    check(!take(tree->focused_text_editor()), "Retired content retained an editor");
    tree->close();
    check(!tree->focused_text_editor(), "Closed widget tree accepted an editor query");

    auto popup_tree=state();
    auto popup_host=take(popup_tree->content_host({},"popup-host",{200,100}));
    gui::Widget popup,field;
    good(popup_host.set_content([&](gui::WidgetBuilder& builder)->core::Result<void>{
        auto root=take(builder.absolute_stack({},"popup-root",{200,100}));
        popup=take(builder.popover(root,"popup",{.width=120,.height=80}));return {};
    }));
    good(popup.set_popup_content([&](gui::WidgetBuilder& builder)->core::Result<void>{
        auto root=take(builder.vstack({},"body"));
        field=take(builder.input_field(root,"popup-field",{.text="Value"}));return {};
    }));
    good(popup.open_at(20,10));(void)take(popup_tree->drawing_snapshot({200,100}));good(field.focus());
    current=take(popup_tree->focused_text_editor());
    check(current&&current->id.ends_with("popup-field")&&current->text=="Value",
          "Focused lookup lost the active popup editor");
    good(popup.close_popup());
    check(!take(popup_tree->focused_text_editor()),"Closed popup retained a focused editor");
    popup_tree->close();

    auto large=state();auto root=take(large->absolute_stack({},"large",{320,240}));
    std::vector<gui::Widget> labels;
    for(int i=0;i<100;++i){
        auto label=take(large->add(gui::WidgetKind::label,root,"label-"+std::to_string(i),{},
                                   {.text="item-"+std::to_string(i)}, {},{}));
        good(label.set_frame({float((i%10)*30),float((i/10)*20),30,20}));labels.push_back(label);
    }
    (void)take(large->drawing_snapshot({320,240}));
    good(labels[31].set_text("before-boundary"));good(labels[32].set_text("after-boundary"));
    auto all=take(large->snapshots());check(all.size()==101,"Chunked snapshot omitted a node");
    for(std::size_t i=0;i<labels.size();++i){
        const auto expected=take(labels[i].snapshot());const auto& actual=all[i+1];
        check(actual.id==expected.id&&actual.text==expected.text&&actual.bounds==expected.bounds&&
              actual.visible==expected.visible,"Chunked snapshot differed from direct node state");
    }
    large->close();
}

void paint_chunks() {
    auto tree=state();
    auto root=take(tree->absolute_stack({}, "root", {200,100}));
    auto left=take(tree->absolute_stack(root, "left", {90,100}));
    auto right=take(tree->absolute_stack(root, "right", {90,100}));
    good(left.set_frame({0,0,90,100}));good(right.set_frame({100,0,90,100}));
    unsigned left_paints=0,right_paints=0;
    const auto painter=[](unsigned& calls) {
        return [&calls](const gui::WidgetPaintState&) -> core::Result<std::shared_ptr<const gui::WidgetDrawing>> {
            ++calls;return std::make_shared<const gui::WidgetDrawing>();
        };
    };
    good(left.set_paint_handler(painter(left_paints)));
    good(right.set_paint_handler(painter(right_paints)));
    (void)take(tree->drawing_snapshot({200,100}));
    check(left_paints==1&&right_paints==1,"Initial drawing did not build both chunks");
    (void)take(tree->drawing_snapshot({200,100}));
    check(left_paints==1&&right_paints==1,"Unchanged view regenerated paint chunks");
    good(left.request_repaint());(void)take(tree->drawing_snapshot({200,100}));
    check(left_paints==2&&right_paints==1,"Repaint regenerated an unrelated chunk");
    good(right.set_enabled(false));(void)take(tree->drawing_snapshot({200,100}));
    check(left_paints==2&&right_paints==2,"Changed interaction did not update exactly its chunk");
    good(root.request_repaint());(void)take(tree->drawing_snapshot({200,100}));
    check(left_paints==3&&right_paints==3,"Parent repaint did not refresh dependent descendants");
    bool fail=true;unsigned attempts=0;
    good(left.set_paint_handler([&](const gui::WidgetPaintState&)->core::Result<std::shared_ptr<const gui::WidgetDrawing>>{
        ++attempts;if(fail)return core::fail(core::ErrorCode::failed,"temporary paint failure");
        return std::make_shared<const gui::WidgetDrawing>();
    }));
    check(!tree->drawing_snapshot({200,100})&&attempts==1,"Failed paint was reported as successful");
    fail=false;(void)take(tree->drawing_snapshot({200,100}));
    check(attempts==2&&right_paints==3,"Paint retry was cached or regenerated an unrelated chunk");
    tree->close();
}

struct ColorSource final : gui::ColorSurfaceSource {
    std::uint64_t version = 0;
    std::uint8_t pixel = 127;
    std::uint64_t revision() const noexcept override { return version; }
    gui::ColorSurfaceResult<gui::ColorSurfaceFrame> frame(gui::ColorSurfaceKind, std::uint32_t width, std::uint32_t height) const override {
        gui::ColorSurfaceFrame value;
        value.width=width; value.height=height; value.revision=version;
        value.marker={version ? .75 : .25, .5}; value.color={.2, .3, .4, 1};
        value.rgba.assign(std::size_t(width)*height*4, pixel);
        return value;
    }
    gui::ColorSurfaceResult<std::uint64_t> edit(gui::ColorSurfaceKind, gui::ColorSurfacePoint, std::uint64_t) override { return ++version; }
};
std::shared_ptr<const gui::RasterImage> color_image(const gui::CanvasFrame& frame) {
    auto match = [](const auto& images) -> std::shared_ptr<const gui::RasterImage> {
        for (const auto& image : images) if (image.image && image.image->width() == 64) return image.image;
        return {};
    };
    if (auto image=match(frame.images)) return image;
    for (const auto& layer : frame.overlays) if (auto image=match(layer.images)) return image;
    return {};
}
float marker_x(const gui::CanvasFrame& frame) {
    for(const auto& layer:frame.overlays)
        for(const auto& image:layer.images)
            if(image.image&&image.image->width()==32)return image.bounds.x;
    throw std::runtime_error("Color marker was not drawn");
}
void color() {
    auto tree = state();
    auto host = take(tree->content_host({}, "host", {100, 100}));
    auto source = std::make_shared<ColorSource>();
    gui::ColorSurfaceInput input;
    input.source=source; input.configuration.width=64; input.configuration.height=64;
    std::optional<gui::NativeColorSurface> surface;
    good(host.set_content([&](gui::WidgetBuilder& builder) -> core::Result<void> {
        surface = take(gui::NativeColorSurface::create(builder, {}, "color", input));
        return {};
    }));
    const auto initial_frame=take(tree->drawing_snapshot({100, 100}));
    auto first = color_image(initial_frame);const auto first_marker=marker_x(initial_frame);
    check(bool(first), "Initial color image was not drawn");
    ++source->version; input.source_revision=static_cast<std::int64_t>(source->version);
    good(surface->set_options(input));
    const auto updated_frame=take(tree->drawing_snapshot({100, 100}));
    auto same = color_image(updated_frame);
    check(same == first && marker_x(updated_frame)!=first_marker,
          "A marker-only change replaced the image or did not move the marker");
    ++source->version; ++source->pixel; input.source_revision=static_cast<std::int64_t>(source->version);
    good(surface->set_options(input));
    auto changed = color_image(take(tree->drawing_snapshot({100, 100})));
    check(changed && changed != same && changed->rgba()[0] == source->pixel,
          "A changed color image reused stale pixels");
    tree->close();
}
}
int main(int argc, char** argv) {
    try {
        const std::string name = argc > 1 ? argv[1] : "options";
        if (name == "options") options();
        else if (name == "non_action") non_action();
        else if (name == "color") color();
        else if (name == "focused_editor") focused_editor();
        else if (name == "paint_chunks") paint_chunks();
        else throw std::runtime_error("Unknown case");
        std::cout << "INCREMENTAL_WIDGET_UPDATES_OK " << name << '\n';
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
