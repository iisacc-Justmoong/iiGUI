# Self-rendered menus

`WidgetBuilder::menu`, `menu_item` and `menu_divider` construct C++ widget nodes.
No OS menu, popup control, native item view or source interpreter renders them.
The macOS adapter translates input and exposes virtual accessibility peers; iiGUI
owns placement, scrolling, hit testing, keyboard navigation, action dispatch and
GPU paint. Windows/Linux widget mounting still requires its platform adapter.

Menus belong to the same widget generation as their parent, including when a
menu belongs to a button or a menu item. They take no space in the parent's flow.
`open_at(x,y)` uses logical surface coordinates; `open_for(anchor,x,y)` maps an
offset from an anchor in the same generation. Placement chooses the available
side and clamps to the window margin. Repositioning an open menu does not emit a
second opened event. `close_popup()` is idempotent and restores the previous
eligible focus. Closing an owner popup also closes its descendants.

```cpp
auto root = builder.vstack({}, "root");
// Check Results before dereferencing in application code.
auto popup = builder.menu(*root, "actions", {}, {
    .triggered = [](int index) { /* stable index, including divider slots */ }
});
auto action = builder.menu_item(*popup, "open", {
    .label = "Open", .shortcut = "Ctrl+O", .has_child_items = false
}, [] { /* application action */ });
```

Regular menus use 24-unit rows and 4-unit vertical padding. Context style uses
18-unit rows and 8-unit padding, with Inter Regular 12-unit labels and Pretendard
shortcuts. The five bundled font faces and both OFL notices are attached through
Core application resources. LVRS factories hydrate original SVG icons and arrows.
Standalone menu items/dividers are also supported for component catalogs.

The popup captures input. Up/Down, Home/End and Tab navigate eligible items;
inactive/disabled items and dividers are skipped. Navigation reveals offscreen
rows. Pointer wheels scroll logical pixels. The popup is clipped to the viewport,
and scrolled rows preserve their model index. Outside input dismisses eligible
popups and consumes the matching release, preventing clicks through to content.
Escape closes the current popup; focus loss closes the open branch. A parent
menu can receive a click after a child popup is dismissed at that point.

`set_context_menu(popup)` binds context requests to a widget. Passing an empty
handle removes the binding. macOS right mouse events and the virtual Show Menu
action use the same state transition. `set_selected_index`, `set_menu_state`,
`set_menu_expanded`, text and enabled mutations remain UI-thread operations.
Inactive state cannot be bypassed by setting enabled to true.

Entry action, typed menu index callback and automatic close run in that order.
Child-bearing entries stay open by default; an explicit `close_on_trigger`
overrides the menu default. If a callback retires the tree, later dispatch stops.
If it reopens/repositions the menu, the previous activation cannot close that
new popup occurrence. All callbacks share the host exception barrier and retain
their accepted scope only for the current dispatch.

`WidgetSnapshot` reports visibility, blocked input, popup membership, model index,
content height and scroll offset. Virtual accessibility menus contain only their
visible, clipped entries; background controls are excluded while the popup owns
input. Peers remain accessibility metadata and do not create OS control objects.

Each popup uses an opaque authored material base and a 12% tint coating, then a
separate clipped content layer. Metal/Vulkan/D3D11 scissoring covers panels,
glyphs and images together. Changing only a clip does not rerasterize/upload the
foreground. Up to eight popups fit the sixteen-layer limit. The original 64-unit
backdrop blur, accent radial coating, grain and spring/open transitions remain
unimplemented; the current base/coating is an explicit material fallback.
Dynamic object models, named multi-event payloads, full inherited APIs and glyph
atlas/large-window memory parity also remain part of the LVRS migration.

`tests/menus.mm` checks lifecycle/order, placement, input capture, native/virtual
context requests, descendant close and scope retirement, regular/context paints,
and a 150-entry scrolling menu. `tests/images.cpp` checks actual Metal/Vulkan
clip pixels and foreground-cache reuse. D3D11 was updated in source; macOS tests
do not establish Windows runtime validation.
