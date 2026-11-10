#include <iiGUI/WidgetState.h>

#include <memory>
#include <stdexcept>
#include <utility>

namespace {
template<class T> T take(ii::core::Result<T> value) {
    if (!value) throw std::runtime_error(value.error().message);
    return std::move(*value);
}
void good(ii::core::Result<void> value) {
    if (!value) throw std::runtime_error(value.error().message);
}
void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
void hover_after_scroll() {
    using namespace ii::gui;
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{200,160}));Widget scroll,first,second,popup,cover,sibling;
    good(host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto root=take(builder.absolute_stack({},"root",{200,160}));
        scroll=take(builder.scroll_view(root,"scroll",{.width=100,.height=60}));good(scroll.set_frame({10,10,100,60}));
        auto body=take(builder.vstack(scroll,"body",{.spacing=0,.alignment=StackAlignment::leading}));
        first=take(builder.action_surface(body,"first",{.width=80,.height=40}));
        second=take(builder.action_surface(body,"second",{.width=80,.height=40}));
        take(builder.action_surface(body,"third",{.width=80,.height=40}));
        sibling=take(builder.action_surface(root,"sibling",{.width=60,.height=30}));good(sibling.set_frame({130,10,60,30}));
        PopoverOptions options;options.width=100;options.height=80;options.padding=0;options.margins=0;
        options.focus_on_open=false;options.dismiss_on_escape=false;options.dismiss_on_outside_press=false;options.preserve_input=true;
        options.bounds=Rect{90,60,100,80};options.input_bounds=Rect{0,0,100,70};
        popup=take(builder.popover(root,"popup",options));take(builder.label(popup,"popup-label",{.text="help"}));
        cover=take(builder.popover(root,"cover",options));take(builder.label(cover,"cover-label",{.text="cover"}));
        return {};
    }));
    (void)take(tree->drawing_snapshot({200,160}));
    good(tree->input({.kind=WidgetInputKind::pointer_move,.x=20,.y=20}));
    require(take(first.interaction()).hovered&&take(scroll.interaction()).hovered,"Pointer did not reach the first scroll row");
    good(tree->input({.kind=WidgetInputKind::scroll,.x=20,.y=20,.scroll_y=40}));
    require(!take(first.interaction()).hovered&&take(second.interaction()).hovered&&take(scroll.interaction()).hovered,
        "Scrolling did not retarget hover to the row now under the pointer");
    good(popup.open_at(0,0));
    good(tree->input({.kind=WidgetInputKind::pointer_move,.x=170,.y=100}));
    require(take(popup.interaction()).hovered,"A text-only popup did not report hover in its body");
    good(cover.open_at(0,0));
    require(take(cover.interaction()).hovered&&!take(popup.interaction()).hovered,
        "A covered popup retained passive hover through a higher popup");
    good(cover.close_popup());
    require(take(popup.interaction()).hovered,"Closing a covering popup did not restore passive hover");
    good(tree->input({.kind=WidgetInputKind::pointer_move,.x=170,.y=135}));
    require(!take(popup.interaction()).hovered,"Popup hover extended outside its input bounds");
    good(tree->input({.kind=WidgetInputKind::pointer_leave}));
    require(!take(popup.interaction()).hovered,"Pointer leave retained popup hover");
    good(scroll.set_visible(false));good(tree->input({.kind=WidgetInputKind::pointer_move,.x=140,.y=20}));
    require(take(sibling.interaction()).hovered,"Pointer did not hover the sibling control");
    good(scroll.set_scroll_offset(0));
    require(take(sibling.interaction()).hovered,"A hidden viewport cleared an unrelated control's hover");
    good(host.clear_content());tree->close();
}
}

int main() {
    using namespace ii::gui;
    auto tree = std::make_shared<detail::WidgetState>([](ii::runtime::Task task) { task(); });
    auto host = take(tree->content_host({}, "host", {240, 180}));
    Widget outer, inner, content, embedded;
    good(host.set_content([&](WidgetBuilder& builder) -> ii::core::Result<void> {
        auto root = take(builder.absolute_stack({}, "root", {240, 180}));
        outer = take(builder.scroll_view(root, "outer", {.width=200, .height=150, .virtual_content_height=600}));
        good(outer.set_frame({0, 0, 200, 150}));
        auto body = take(builder.absolute_stack(outer, "body", {200, 150}));
        inner = take(builder.scroll_view(body, "inner", {.width=100, .height=80, .virtual_content_height=300}));
        good(inner.set_frame({20, 20, 100, 80}));
        content = take(builder.content_host(inner, "inner-body", {100, 80}));
        return {};
    }));
    good(content.set_content([&](WidgetBuilder& builder) -> ii::core::Result<void> {
        embedded = take(builder.scroll_view({}, "embedded-scroll", {.width=100, .height=80,
            .virtual_content_height=80, .virtual_content_width=300}));
        take(builder.absolute_stack(embedded, "embedded-body", {100, 80}));
        return {};
    }));
    (void)take(tree->drawing_snapshot({240, 180}));
    const auto wheel = [&] { good(tree->input({.kind=WidgetInputKind::scroll, .x=40, .y=40, .scroll_y=20})); };
    wheel();
    require(take(inner.snapshot()).scroll_offset == 20 && take(outer.snapshot()).scroll_offset == 0,
            "A user scroll did not reach the inner viewport");
    good(inner.set_scroll_options({.width=100, .height=80, .virtual_content_height=300, .user_scroll_enabled=false}));
    require(take(inner.snapshot()).scroll_offset == 20 && take(inner.snapshot()).content_height == 300,
            "Disabling user scroll discarded the offset or logical content");
    wheel();
    require(take(inner.snapshot()).scroll_offset == 20 && take(outer.snapshot()).scroll_offset == 20,
            "A disabled inner user scroll did not reach its parent");
    good(inner.set_scroll_offset(60));
    require(take(inner.snapshot()).scroll_offset == 60, "Programmatic scrolling was disabled");
    good(inner.set_scroll_options({.width=100, .height=80, .virtual_content_height=300, .user_scroll_enabled=true}));
    wheel();
    require(take(inner.snapshot()).scroll_offset == 80 && take(outer.snapshot()).scroll_offset == 20,
            "Re-enabling user scrolling did not restore inner input");
    good(inner.set_scroll_offset(0));good(outer.set_scroll_offset(0));
    good(tree->input({.kind=WidgetInputKind::scroll,.x=40,.y=40,.scroll_y=-17,.scroll_x=-13,
                      .precise_scroll=true,.device=PointerDevice::touch}));
    require(take(embedded.snapshot()).horizontal_scroll_offset==13 && take(inner.snapshot()).scroll_offset==17 &&
            take(outer.snapshot()).scroll_offset==0, "Touch content did not follow the finger on both axes");
    good(tree->input({.kind=WidgetInputKind::scroll,.x=40,.y=40,.scroll_y=7,.scroll_x=5,
                      .precise_scroll=true,.device=PointerDevice::touch}));
    require(take(embedded.snapshot()).horizontal_scroll_offset==8 && take(inner.snapshot()).scroll_offset==10,
            "Reverse touch pan did not move content back with the finger");
    good(embedded.set_horizontal_scroll_offset(0));
    good(inner.set_scroll_offset(0));good(outer.set_scroll_offset(0));
    good(tree->input({.kind=WidgetInputKind::scroll, .x=40, .y=40, .scroll_y=17, .scroll_x=13}));
    require(take(embedded.snapshot()).horizontal_scroll_offset==13 && take(inner.snapshot()).scroll_offset==17 &&
            take(outer.snapshot()).scroll_offset==0, "Embedded scrolling lost or duplicated a diagonal axis");
    good(tree->input({.kind=WidgetInputKind::scroll,.x=40,.y=40,.shift=true,.scroll_y=7,.scroll_x=9}));
    require(take(embedded.snapshot()).horizontal_scroll_offset==22&&take(inner.snapshot()).scroll_offset==24,
            "Shift converted an already-normalized remaining axis twice");
    int ancestor_calls=0;bool original_shift_input=false;
    good(inner.set_input_handler([&](const WidgetInput& event) -> ii::core::Result<bool> {
        if(event.kind==WidgetInputKind::scroll){++ancestor_calls;original_shift_input=event.shift&&event.scroll_x==0&&event.scroll_y==12;}
        return false;
    }));
    good(tree->input({.kind=WidgetInputKind::scroll,.x=40,.y=40,.shift=true,.scroll_y=12}));
    require(take(embedded.snapshot()).horizontal_scroll_offset==34&&take(inner.snapshot()).scroll_offset==24&&ancestor_calls==1&&original_shift_input,
            "Shift-wheel lost its axis or notified an ancestor twice");
    good(embedded.set_input_handler([](const WidgetInput& event) -> ii::core::Result<bool> {
        return event.kind==WidgetInputKind::scroll;
    }));
    wheel();
    require(take(inner.snapshot()).scroll_offset==24, "A handled embedded scroll reached an ancestor");
    good(host.clear_content());
    tree->close();
    hover_after_scroll();
}
