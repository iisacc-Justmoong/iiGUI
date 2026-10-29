#pragma once
#include "Host.h"
#include "WidgetLayout.h"
#include "WindowProperties.h"
#include "Appearance.h"
#include "Image.h"
#include "ColorPicker.h"
#include "Motion.h"
#include "WidgetInteraction.h"
#include <iiSDKCore/Animation/Timeline.h>
#include <thread>
#include <optional>

namespace ii::gui {
namespace detail { class WidgetState; }
class WidgetBuilder;
using WidgetFactory=std::function<core::Result<void>(WidgetBuilder&)>;
enum class WidgetKind { vertical_stack, horizontal_stack, label, button, overlay_stack, spacer,
    check_box, radio_button, toggle_switch, input_field, slider, progress, tab_bar, tab,
    combo_box, stepper, segmented_control, menu, menu_item, menu_divider, color_surface, absolute_stack, image, popover, scroll_view, content_host, action_surface };
// Semantic metadata for application-owned custom containers. It does not
// create a widget, take ownership of a model, or change input and layout.
enum class AccessibilityRole { automatic, image, progress, slider, stepper, tab_group, menu };
struct AccessibilityRange final {
    double minimum = 0, maximum = 1, value = 0;
    friend bool operator==(const AccessibilityRange&, const AccessibilityRange&) = default;
};
struct AccessibilitySemantics final {
    AccessibilityRole role = AccessibilityRole::automatic;
    std::string name, description;
    std::optional<AccessibilityRange> range;
    bool can_increment=true,can_decrement=true;
    friend bool operator==(const AccessibilitySemantics&, const AccessibilitySemantics&) = default;
};
enum class ButtonTone { primary, normal, borderless, destructive, disabled };
struct KeyedContentItem final {
    std::string key;
    WidgetFactory factory; // Only evaluated when this key has no live activation.
    std::optional<Rect> frame; // Signed viewport coordinates in positioned mode.
};
struct KeyedContentOptions final {
    bool horizontal=false;
    StackOptions layout{.spacing=0,.alignment=StackAlignment::stretch};
    std::optional<Size> viewport; // Positioned/clipped cells; every item needs a frame.
    friend bool operator==(const KeyedContentOptions&,const KeyedContentOptions&)=default;
};
// One retained content child; zero width/height uses its natural extent.
struct ScrollOptions final {
    double width=0,height=0;
    bool enabled=true,show_indicator=true;
    // Logical extent, independent of retained child geometry. The owner draws
    // only the current viewport; its child stays at the viewport origin.
    std::optional<double> virtual_content_height;
    std::optional<double> virtual_content_width;
    // Natural content keeps its preferred width when enabled. Supplying a
    // virtual width enables the logical X axis without translating the child.
    bool horizontal_enabled=false;
    friend bool operator==(const ScrollOptions&,const ScrollOptions&)=default;
};
struct ScrollViewport final {
    Rect bounds;
    double offset=0,content_height=0;
    double horizontal_offset=0,content_width=0;
    friend bool operator==(const ScrollViewport&,const ScrollViewport&)=default;
};
struct WidgetMeasurement final {
    double available_width=0;
    Size natural;
    friend bool operator==(const WidgetMeasurement&,const WidgetMeasurement&)=default;
};
// Retained native vector ink in local coordinates, intrinsically clipped to its
// absolute stack. Colors use the same linear-light contract as CanvasFrame.
struct WidgetDrawing final {
    std::vector<Panel> panels;
    std::vector<TextRun> texts;
    std::vector<ImageRun> images;
    bool images_before_text=false;
    std::optional<BackdropMaterial> backdrop;
    std::optional<BoxShadow> shadow;
    friend bool operator==(const WidgetDrawing&,const WidgetDrawing&)=default;
};
struct WidgetPaintState final {
    Size size;
    bool enabled=true,hovered=false,pressed=false,focused=false,checked=false;
    std::size_t content_children=0;
    double backing_scale=1;
    // Resolved application font for host-independent custom control drawing.
    std::string font_family;
    friend bool operator==(const WidgetPaintState&,const WidgetPaintState&)=default;
};
// Pure, owner-thread paint provider. UI queries/mutations during it are rejected.
using WidgetPaintHandler=std::function<core::Result<std::shared_ptr<const WidgetDrawing>>(const WidgetPaintState&)>;
// Owner-thread Core frames may update widgets. False stops this request;
// request_frames restarts it. All handlers in a View share one frame slot.
using WidgetFrameHandler=std::function<core::Result<bool>(runtime::Clock::time_point)>;
struct LabelOptions final { std::string text; double font_size = 14; bool selectable = true; TextRole role=TextRole::description;std::optional<WidgetColor> color;TextAlignment alignment=TextAlignment::left;double line_height=0;std::optional<unsigned> font_weight;bool elide_right=false;friend bool operator==(const LabelOptions&,const LabelOptions&)=default; };
struct ButtonStyle final {
    std::optional<WidgetColor> fill,hover_fill,pressed_fill,disabled_fill,text,disabled_text,border;
    double border_width=0;
    double line_height=0;
    std::optional<TextRole> text_role;
    std::optional<double> font_size;
    friend bool operator==(const ButtonStyle&,const ButtonStyle&)=default;
};
struct ButtonOptions final {
    std::string text; bool enabled = true; ButtonTone tone=ButtonTone::primary;
    std::shared_ptr<const RasterImage> icon,indicator,disabled_indicator;
    std::string icon_name,accessibility_name;
    double icon_size=16;
    bool icon_mode=false,help=false;
    std::shared_ptr<const RasterImage> background_image;
    std::optional<double> height,corner_radius;
    std::optional<double> horizontal_padding;
    std::optional<ButtonStyle> style;
    // Explicit composition metrics; omission retains the existing button layout.
    std::optional<double> left_padding,right_padding,content_spacing,indicator_size,width;
};
// Semantic role for an application-owned interactive surface. The role does
// not determine its state transitions; the application may implement them.
enum class ActionRole { button,link,check_box,radio_button,toggle_switch,tab,menu_item };
// A transparent, keyboard-operable action container. Ordinary native children
// share its generation; the text fallback is painted only when the slot is empty.
struct ActionOptions final {
    std::string text,accessibility_name;
    bool enabled=true,underline=false,focus_ring=true;
    ActionRole role=ActionRole::button;
    TextRole text_role=TextRole::body;
    double font_size=13,line_height=0;
    Insets padding;
    double minimum_width=0,minimum_height=0,corner_radius=0;
    std::optional<double> width,height;
    std::optional<WidgetColor> text_color,hover_text_color,pressed_text_color,disabled_text_color;
    bool interaction_enabled=true;
    bool checkable=false,checked=false,tab_focusable=true,pointer_focusable=true;
    std::string accessibility_description;
};
enum class TabStyle { underline,surface };
struct TabOptions final {
    std::string text,icon_name,badge;
    std::shared_ptr<const RasterImage> icon;
    bool enabled=true,selected=false;
    TabStyle style=TabStyle::underline;
    double icon_size=18,minimum_width=80;
};
struct TabBarOptions final { double spacing=4,minimum_tab_width=120;int current_index=0;bool auto_select=true; };
enum class StepperArrow { up_down, up, down };
struct ControlEvents final { runtime::Task clicked,pressed,released,canceled; };
struct ColorButtonOptions final {
    std::shared_ptr<ColorPickerModel> model;
    std::shared_ptr<const RasterImage> hue_ring;
    double size=28;
    bool enabled=true,show_hue_ring=true;
    // Plain circular swatch for compact painting toolbars. Default stays square.
    bool circular=false;
    double border_opacity=1;
    std::string accessibility_name="Choose color";
};
struct StepperOptions final {
    ButtonTone tone=ButtonTone::primary;
    StepperArrow arrow=StepperArrow::up_down;
    bool enabled=true;
    double corner_radius=4;
    std::shared_ptr<const RasterImage> icon;
    std::string accessibility_name="Stepper";
};
struct ComboBoxOptions final {
    std::string text="Label";
    ButtonTone tone=ButtonTone::primary;
    StepperArrow arrow=StepperArrow::up_down;
    bool enabled=true;
    double width=97;
    std::shared_ptr<const RasterImage> indicator;
};
// A styled row of independent children; there is no implicit selection model.
struct SegmentedOptions final {
    double horizontal_padding=4,vertical_padding=3.5,spacing=2,border_width=2,corner_radius=12;
    // Opaque authored surfaces. Translucent borders/fills return invalid_argument.
    WidgetColor background{.1370945126,.1402995437,.1419020444,1},border{.1935568452,.1978072077,.1999323964,1};
    bool cylinder=false,force_borderless_tone=true;
};
enum class MenuStyle { regular,context };
enum class MenuItemState { normal,selected,inactive };
enum class MenuDirection { automatic,right,left,up,down };
struct MenuOptions final {
    MenuStyle style=MenuStyle::regular;
    std::optional<bool> compact_items;
    double item_width=0,item_spacing=2,left_padding=8,right_padding=8;
    std::optional<double> top_padding,bottom_padding;
    double edge_margin=4,corner_radius=12,opacity=.12;
    std::optional<WidgetColor> background;
    int selected_index=-1;
    bool auto_close_on_trigger=true,dismiss_on_outside_press=true,dismiss_on_context_request=true;
};
struct MenuEvents final { std::function<void(int)> triggered;runtime::Task opened,closed; };
enum class PopoverPlacement { anchored, centered };
struct PopoverShadow final {
    double offset_x=0,offset_y=16,blur_radius=32;
    WidgetColor color{0,0,0,.28};
};
struct PopoverTransition final {
    runtime::Clock::duration enter_duration{},exit_duration{};
    animation::Easing enter_easing=animation::Easing::linear,exit_easing=animation::Easing::linear;
    double overshoot=1.70158,collapsed_scale=1;
    bool enabled=true;
    std::shared_ptr<MotionPreferences> preferences;
    Size origin{.5,.5}; // Normalized scale pivot; existing popovers stay centered.
};
enum class SheetDetent { fit,medium,large };
// Retained modal surface: desktop center or a full-width mobile bottom sheet.
// The named scroll viewport and optional grabber belong to this popup tree.
struct SheetLayoutOptions final {
    bool mobile=false;
    SheetDetent detent=SheetDetent::fit;
    double preferred_height=420,desktop_margin=24,top_safe_inset=0;
    std::string viewport_id,grabber_id;
    bool dismiss_on_drag=true;
};
// Application-owned placement: the host first asks for a width with empty
// content, then measures retained children at that width and asks for the
// final rectangle. Coordinates are local to the host viewport.
struct PopoverLayoutMeasure final {
    Size viewport,content;
    double viewport_content_height=0;
};
struct PopoverAdaptiveLayout final {
    std::string viewport_id;
    std::function<core::Result<Rect>(const PopoverLayoutMeasure&)> layout;
    bool slide_from_bottom=false;
};
struct PopoverOptions final {
    double width=0,height=0,padding=12,margins=8,corner_radius=12,surface_opacity=.25;
    std::optional<WidgetColor> surface_color;
    bool modal=false,dim=false,focus_on_open=true,clip_content=false;
    bool dismiss_on_escape=true,dismiss_on_outside_press=true;
    std::string accessibility_name="Popover";
    PopoverPlacement placement=PopoverPlacement::anchored;
    double minimum_width=0,maximum_width=8192,minimum_height=0,vertical_offset=0;
    bool cylinder=false;
    WidgetColor backdrop_color{0,0,0,.4};
    bool fit_to_viewport=false,opaque_base=true;
    double corner_limit=.5,border_width=0;
    WidgetColor border_color{0,0,0,0};
    std::optional<double> blur_radius;
    std::optional<PopoverShadow> shadow;
    std::optional<PopoverTransition> transition;
    std::optional<SheetLayoutOptions> sheet_layout;
    // Explicit popup geometry in absolute native-surface coordinates. The
    // optional local input rectangle leaves a painted tail transparent to hits.
    std::optional<Rect> bounds,input_bounds;
    bool preserve_input=false; // Nonmodal, no focus-on-open; keeps the origin's grab.
    // When omitted, context requests follow the ordinary outside-press policy.
    std::optional<bool> dismiss_on_context_request;
    std::optional<PopoverAdaptiveLayout> adaptive_layout;
};
struct PopoverEvents final {runtime::Task opened,closed,canceled;};
struct MenuItemOptions final {
    MenuStyle style=MenuStyle::regular;
    std::string label="Label";
    std::optional<std::string> shortcut;
    std::string key_placeholder,icon_name="procedure";
    std::shared_ptr<const RasterImage> icon,chevron_right,chevron_left,chevron_up,chevron_down;
    bool enabled=true;
    std::optional<bool> compact;
    MenuItemState state=MenuItemState::normal;
    MenuDirection direction=MenuDirection::automatic;
    bool key_visible=true,show_icon_slot=true,show_chevron=true,has_child_items=true,expanded=false;
    double item_width=0,item_height=0,icon_size=18,chevron_size=16;
    std::optional<bool> close_on_trigger;
};
struct MenuDividerOptions final {
    MenuStyle style=MenuStyle::regular;
    bool vertical=false;
    double thickness=1,cross_padding=1;
    std::optional<double> line_padding;
    double line_length=0;
    std::optional<WidgetColor> color;
};
struct CheckOptions final { std::string text; bool enabled=true, checked=false; std::string group; };
// Optional text-editor appearance for composed controls. Omitting it preserves
// the ordinary InputField metrics, hover surface and focus ring.
struct InputStyle final {
    double height=30,font_size=13,line_height=0,horizontal_padding=8,corner_radius=8;
    std::optional<WidgetColor> background,hover_background,text;
    bool focus_ring=true;
};
struct InputOptions final {
    std::string text, placeholder; bool enabled=true, read_only=false; double width=206;
    bool commit_on_focus_loss=false;
    std::string accessibility_name;
    std::size_t maximum_length=0; // Unicode scalars; zero keeps the 64 KiB byte limit only.
    std::optional<InputStyle> style;
    bool multiline=false,password=false;
};
struct SliderOptions final {
    double minimum=0, maximum=1, value=.5, step=0, width=320; bool enabled=true;
    double thumb_diameter=18,track_height=6,range_inset=9;
    std::shared_ptr<const RasterImage> background_image;
    std::string accessibility_name;
    double height=32;
};
struct ImageOptions final {
    std::shared_ptr<const RasterImage> image;
    double width=32,height=32;
    std::string accessibility_name;
    double padding=0,corner_radius=0,border_width=0;
    std::optional<WidgetColor> background,border;
    bool preserve_aspect=false;
    std::optional<ImageFit> fit;
    bool circular=false,allow_empty=false;
};
struct ProgressOptions final { double minimum=0, maximum=100, start=0, value=0, width=100, height=6; };
struct ColorSurfaceOptions final {
    std::shared_ptr<ColorPickerModel> model;
    ColorField field=ColorField::wheel;
    double width=232,height=232;
    bool enabled=true;
    std::shared_ptr<const RasterImage> hue_ring;
    std::string accessibility_name="Color field";
};
enum class WidgetInputKind { pointer_move, pointer_down, pointer_up, pointer_leave, key_down, key_up,
    text, composition, focus_lost, activate, focus, increment, decrement, context_request, close_popup, scroll, drop, commit_composition };
enum class WidgetKey { none, tab, enter, space, escape, left, right, up, down, home, end, backspace, delete_forward, select_all, character, copy, cut, page_up, page_down, insert, undo, redo };
enum class PointerDevice { mouse, pen, eraser, touch };
struct WidgetPoint final {
    double x=0,y=0;
    friend bool operator==(const WidgetPoint&,const WidgetPoint&)=default;
};
// Native text services use UTF-16 code units. A range may never split a scalar.
struct TextRange final {
    std::size_t location=0,length=0;
    friend bool operator==(const TextRange&,const TextRange&)=default;
};
struct TextInputState final {
    std::size_t length = 0;
    TextRange selection;
    std::optional<TextRange> marked;
    Rect caret; // Local widget coordinates, already clipped to its viewport.
    bool read_only = false, multiline = true, auto_focus_on_press = true, password = false;
};
struct TextContent final { TextRange range; std::string text; };
// OS text services query bounded ranges rather than copying the whole document
// into WidgetSnapshot. state/read/position are owner-thread read providers and
// must not call Widget APIs. focus_changed is an ordinary mutation callback.
struct TextInputProvider final {
    std::function<core::Result<TextInputState>()> state;
    std::function<core::Result<TextContent>(TextRange)> read;
    std::function<core::Result<std::size_t>(double x,double y)> position;
    std::function<core::Result<void>(bool)> focus_changed;
    // Optional owner-thread mutation; invoked outside the read-provider guard.
    std::function<core::Result<void>(std::string_view)> set_text;
};
// Logical, top-left surface coordinates. OS adapters translate events only;
// iiGUI owns targeting, capture, focus, editing, control state and activation.
struct WidgetInput final {
    WidgetInputKind kind=WidgetInputKind::pointer_move;
    double x=0, y=0;
    WidgetKey key=WidgetKey::none;
    bool shift=false;
    std::string text, target;
    double scroll_y=0; // Logical pixels; positive moves popup content upwards.
    double scroll_x=0;
    bool control=false,alt=false,meta=false,precise_scroll=false;
    PointerDevice device=PointerDevice::mouse;
    double pressure=1,tilt_x=0,tilt_y=0,rotation=0,timestamp=0;
    std::uint64_t device_id=0;
    std::uint32_t buttons=0; // Primary=1, secondary=2, middle=4, remaining OS buttons follow.
    bool text_input_active=false; // Set by targeting before ancestor shortcut handlers run.
    bool native_control_active=false,native_navigation_active=false;
    std::optional<TextRange> replacement_range;
    std::optional<TextRange> composition_selection;
    std::shared_ptr<const ClipboardContent> content;
    // Filled by the host for pointer-like delivery to a handler; coordinates
    // are relative to that handler's surface, including nested content hosts.
    std::optional<WidgetPoint> local_position;
};
// Ancestor interception before ordinary control behavior. A handled pointer
// press captures move/release until release, focus loss, hiding or retirement.
using WidgetInputHandler=std::function<core::Result<bool>(const WidgetInput&)>;
struct WidgetSnapshot final {
    WidgetKind kind;
    std::string id, text, native_class;
    Size size;
    bool enabled = true, attached = false;
    bool styled = false;
    std::string font_family;
    double font_size = 0;
    Rect bounds;
    bool hovered=false, pressed=false, focused=false, checked=false, read_only=false;
    double value=0, minimum=0, maximum=1;
    std::string placeholder;
    // Actual OS controls are never constructed for these widgets.
    std::string renderer="iiGUI";
    std::string icon_name,accessibility_name;
    bool selected=false;
    int current_index=-1;
    ButtonTone tone=ButtonTone::normal;
    StepperArrow arrow=StepperArrow::up_down;
    std::size_t segment_count=0;
    bool visible=true,opened=false,input_blocked=false,compact=false;
    int menu_index=-1;
    std::size_t menu_count=0;
    std::string parent_id,popup_id;
    MenuItemState menu_state=MenuItemState::normal;
    MenuDirection horizontal_direction=MenuDirection::right,vertical_direction=MenuDirection::down;
    double scroll_offset=0,content_height=0;
    std::optional<Rect> clip_bounds;
    bool has_context_menu=false;
    std::optional<ColorPickerState> color_state;
    ColorField color_field=ColorField::wheel;
    std::uint64_t color_domain_rasterizations=0;
    Size implicit_size;
    bool modal=false,dim=false;
    bool color_button=false,show_hue_ring=false;
    std::uint64_t color_button_rasterizations=0;
    Rect visual_bounds;
    double reveal_progress=1;
    bool transitioning=false,exiting=false;
    std::optional<Rect> visual_clip_bounds;
    std::size_t drawing_panels=0,drawing_texts=0,drawing_images=0;
    TextRange text_selection;
    std::optional<TextRange> marked_range;
    std::string composition_text;
    bool password=false;
    double horizontal_scroll_offset=0,content_width=0;
    bool has_content=false;
    std::uint64_t content_generation=0;
    ActionRole action_role=ActionRole::button;
    bool custom_content=false,text_underline=false;
    std::optional<WidgetColor> foreground_color;
    std::string accessibility_description;
    std::size_t child_count=0;
    bool text_input=false,multiline=false;
    std::size_t text_length=0;
    Rect caret_bounds;
    // Authored opacity and its product through ordinary/embedded parents.
    // Transparency never changes geometry, focus, hit testing or enabled state.
    double opacity=1,effective_opacity=1;
    std::optional<AccessibilitySemantics> accessibility_semantics;
};
// Opaque generation identity plus actual parent/sibling placement. The weak
// identity never owns a native tree and cannot expose its implementation.
struct WidgetPlacement final {
    std::weak_ptr<const void> scope;
    std::uint64_t parent=std::uint64_t(-1),sibling=0;
    std::weak_ptr<const void> domain;
};
// Weak, generation-specific UI handles. Mutations require the host thread.
// A replaced/unmounted/closed tree returns closed, never targets its successor.
class IIGUI_EXPORT Widget final {
public:
    Widget() = default;
    [[nodiscard]] bool is_null()const noexcept{return owner_==std::thread::id{};}
    [[nodiscard]] core::Result<WidgetPlacement> placement() const;
    [[nodiscard]] core::Result<WidgetInteraction> interaction()const;
    [[nodiscard]] core::Result<Size> measure(double available_width)const;
    // Weak, independent subscription; never replaces an input/focus handler.
    // Coalesced after input/layout settles; final alive=false on retirement.
    [[nodiscard]] core::Result<void> observe_interaction(std::shared_ptr<const WidgetInteractionPort>)const;
    [[nodiscard]] core::Result<void> set_text(std::string_view) const;
    // Explicit replacement; programmatic changes emit no user-edit callback.
    [[nodiscard]] core::Result<void> set_label_options(LabelOptions) const;
    [[nodiscard]] core::Result<void> set_label_color(std::optional<WidgetColor>) const;
    [[nodiscard]] core::Result<void> set_input_options(InputOptions) const;
    // Clip ordinary stack descendants for both painting and pointer/AX exposure.
    [[nodiscard]] core::Result<void> set_clip_content(bool) const;
    [[nodiscard]] core::Result<void> set_opacity(double) const;
    // Replace the accessible label of a button or text editor.
    [[nodiscard]] core::Result<void> set_accessibility_name(std::string_view) const;
    // Atomically replace/clear custom-container semantics. Progress ranges are
    // optional for indeterminate progress; host thread and generation apply.
    [[nodiscard]] core::Result<void> set_accessibility_semantics(std::optional<AccessibilitySemantics>) const;
    [[nodiscard]] core::Result<void> set_enabled(bool) const;
    [[nodiscard]] core::Result<void> set_checked(bool) const;
    [[nodiscard]] core::Result<void> set_value(double) const;
    [[nodiscard]] core::Result<void> set_selected(bool) const;
    [[nodiscard]] core::Result<void> set_current_index(int) const;
    [[nodiscard]] core::Result<void> focus() const;
    // Invokes iiGUI's control action. Disabled controls do nothing.
    [[nodiscard]] core::Result<void> activate() const;
    // Emits +1/-1 without owning or mutating a numeric value. Disabled or
    // disallowed single-arrow directions do nothing. No repeat timer is started.
    [[nodiscard]] core::Result<void> step(int direction) const;
    [[nodiscard]] core::Result<void> open_at(double x,double y) const;
    [[nodiscard]] core::Result<void> open_for(Widget anchor,double x=0,double y=0) const;
    [[nodiscard]] core::Result<void> close_popup() const;
    [[nodiscard]] core::Result<void> cancel_popup() const;
    // Alternative to static content: build one activation on open, retire it
    // before closed callbacks. Repositioning an open popup keeps its activation.
    [[nodiscard]] core::Result<void> set_popup_content(WidgetFactory) const;
    // Retain static chrome; create/retire the per-open body in an empty VStack.
    [[nodiscard]] core::Result<void> set_popup_content_at(Widget anchor,WidgetFactory) const;
    [[nodiscard]] core::Result<Widget> popup_content(std::string_view id={}) const;
    [[nodiscard]] core::Result<void> set_selected_index(int) const;
    [[nodiscard]] core::Result<void> set_menu_state(MenuItemState) const;
    [[nodiscard]] core::Result<void> set_menu_expanded(bool) const;
    [[nodiscard]] core::Result<void> set_context_menu(Widget popup) const;
    [[nodiscard]] core::Result<void> set_color_model(std::shared_ptr<ColorPickerModel>) const;
    [[nodiscard]] core::Result<std::shared_ptr<ColorPickerModel>> color_model() const;
    [[nodiscard]] core::Result<void> set_color_field(ColorField) const;
    [[nodiscard]] core::Result<void> set_color_button_size(double) const;
    [[nodiscard]] core::Result<void> set_show_hue_ring(bool) const;
    // Fixed content coordinates apply only to direct children of an absolute
    // stack. Its extent still participates normally in the surrounding layout.
    [[nodiscard]] core::Result<void> set_frame(Rect) const;
    // Stack/scroll sizing; a zero axis keeps natural size except on absolute stacks.
    [[nodiscard]] core::Result<void> set_extent(Size) const;
    // Relative share of a linear stack's available main-axis space.
    [[nodiscard]] core::Result<void> set_flex(double) const;
    [[nodiscard]] core::Result<void> set_scroll_options(ScrollOptions) const;
    [[nodiscard]] core::Result<void> set_scroll_offset(double) const;
    [[nodiscard]] core::Result<void> set_horizontal_scroll_offset(double) const;
    [[nodiscard]] core::Result<void> set_scroll_position(double x,double y) const;
    // Virtualized owners publish logical extent and corrected offsets together.
    // May be called during a viewport callback; delivery settles afterwards.
    [[nodiscard]] core::Result<void> set_virtual_scroll_content(Size content,double x,double y) const;
    [[nodiscard]] core::Result<void> set_scroll_handler(std::function<void(const ScrollViewport&)>) const;
    // Build a fresh independent generation, validate it, then commit and
    // publish. A failed factory/commit preserves the current generation.
    [[nodiscard]] core::Result<void> set_content(WidgetFactory,std::function<core::Result<void>()> commit={}) const;
    [[nodiscard]] core::Result<void> clear_content() const;
    [[nodiscard]] core::Result<Widget> content_item(std::string_view id={}) const;
    // Reconcile by stable key. Existing row generations retain their widgets,
    // owners, edits and focus. A failed new factory preserves the entire list.
    [[nodiscard]] core::Result<void> set_keyed_content(std::vector<KeyedContentItem>,KeyedContentOptions={}) const;
    [[nodiscard]] core::Result<Widget> keyed_content_item(std::string_view key,std::string_view id={}) const;
    [[nodiscard]] core::Result<std::vector<std::string>> keyed_content_keys() const;
    [[nodiscard]] core::Result<void> set_layout_handler(std::function<void(Rect)>) const;
    // Reports preferred content size even when allocated bounds stay unchanged.
    [[nodiscard]] core::Result<void> set_measure_handler(std::function<void(const WidgetMeasurement&)>) const;
    [[nodiscard]] core::Result<void> set_drawing(WidgetDrawing) const;
    [[nodiscard]] core::Result<void> set_paint_handler(WidgetPaintHandler) const;
    [[nodiscard]] core::Result<void> set_paint_handler(WidgetPaintHandler,Widget interaction_owner) const;
    [[nodiscard]] core::Result<void> request_repaint() const;
    [[nodiscard]] core::Result<void> set_frame_handler(WidgetFrameHandler,bool active=false) const;
    [[nodiscard]] core::Result<void> request_frames() const;
    [[nodiscard]] core::Result<void> set_input_handler(WidgetInputHandler) const;
    [[nodiscard]] core::Result<void> set_text_input_provider(std::shared_ptr<const TextInputProvider>) const;
    // Publish a custom provider's changed selection, composition or value.
    // Invoke outside its state/read/position callbacks.
    [[nodiscard]] core::Result<void> notify_text_input_changed(bool value_changed=true) const;
    [[nodiscard]] core::Result<TextContent> text_content(TextRange) const;
    // Position in absolute, top-left native surface coordinates.
    [[nodiscard]] core::Result<std::size_t> text_position(double x,double y) const;
    [[nodiscard]] core::Result<void> set_stack_options(StackOptions) const;
    // Validate and apply stack options, extent and flex as one retained update.
    [[nodiscard]] core::Result<void> set_stack_layout(StackLayoutOptions) const;
    [[nodiscard]] core::Result<StackLayoutOptions> stack_layout() const;
    [[nodiscard]] core::Result<void> set_button_style(ButtonStyle) const;
    [[nodiscard]] core::Result<void> set_button_options(ButtonOptions) const;
    [[nodiscard]] core::Result<void> set_button_icon(std::shared_ptr<const RasterImage>,std::string_view name={}) const;
    [[nodiscard]] core::Result<void> set_action_options(ActionOptions) const;
    [[nodiscard]] core::Result<ActionOptions> action_options() const;
    [[nodiscard]] core::Result<void> set_control_events(ControlEvents) const;
    [[nodiscard]] core::Result<void> set_image_options(ImageOptions) const;
    [[nodiscard]] core::Result<void> set_popover_options(PopoverOptions) const;
    [[nodiscard]] core::Result<void> set_visible(bool) const;
    [[nodiscard]] core::Result<void> set_image(std::shared_ptr<const RasterImage>) const;
    [[nodiscard]] core::Result<void> set_escape_action(runtime::Task) const;
    [[nodiscard]] core::Result<WidgetSnapshot> snapshot() const;
private:
    friend class detail::WidgetState;
    Widget(std::weak_ptr<detail::WidgetState>, std::size_t, std::thread::id);
    [[nodiscard]] core::Result<std::shared_ptr<detail::WidgetState>> lock() const;
    std::weak_ptr<detail::WidgetState> state_;
    std::size_t index_ = 0;
    std::thread::id owner_;
};
// Weak, owner-thread endpoint for the root window's declared properties.
// Embedded content and popup builders do not own this endpoint.
class IIGUI_EXPORT WindowPropertiesTarget final {
public:
    WindowPropertiesTarget()=default;
    [[nodiscard]] core::Result<void> update(WindowProperties) const;
    [[nodiscard]] core::Result<WindowProperties> snapshot() const;
private:
    friend class WidgetBuilder;
    explicit WindowPropertiesTarget(std::weak_ptr<detail::WidgetState> state):state_(std::move(state)){}
    std::weak_ptr<detail::WidgetState> state_;
};
class IIGUI_EXPORT WidgetMount final {
public:
    WidgetMount() = default;
    [[nodiscard]] core::Result<Widget> find(std::string_view id) const;
    [[nodiscard]] core::Result<std::vector<WidgetSnapshot>> snapshot() const;
    [[nodiscard]] core::Result<void> dispatch_input(const WidgetInput&) const;
private:
    friend class Host;
    friend class detail::WidgetState;
    explicit WidgetMount(std::weak_ptr<detail::WidgetState>);
    [[nodiscard]] core::Result<std::shared_ptr<detail::WidgetState>> lock() const;
    std::weak_ptr<detail::WidgetState> state_;
    std::thread::id owner_;
};
// Used only during Host::mount_widgets. Calls construct iiGUI C++ widgets
// immediately; no OS controls, source evaluator or opcode dispatcher runs.
class IIGUI_EXPORT WidgetBuilder final {
public:
    WidgetBuilder(const WidgetBuilder&) = delete;
    WidgetBuilder& operator=(const WidgetBuilder&) = delete;
    [[nodiscard]] core::Result<WidgetAppearance> appearance() const;
    [[nodiscard]] core::Result<WindowPropertiesTarget> window_properties() const;
    [[nodiscard]] core::Result<std::shared_ptr<MotionPreferences>> motion_preferences() const;
    [[nodiscard]] core::Result<std::reference_wrapper<runtime::EventLoop>> event_loop() const;
    // Only future ordinary children are redirected. Popups retain their owner.
    [[nodiscard]] core::Result<void> set_default_content(Widget host,Widget slot);
    [[nodiscard]] core::Result<Widget> vstack(Widget parent, std::string_view id, StackOptions = {});
    [[nodiscard]] core::Result<Widget> hstack(Widget parent, std::string_view id, StackOptions = {});
    [[nodiscard]] core::Result<Widget> zstack(Widget parent, std::string_view id, StackOptions = {});
    [[nodiscard]] core::Result<Widget> absolute_stack(Widget parent,std::string_view id,Size);
    [[nodiscard]] core::Result<Widget> scroll_view(Widget parent,std::string_view id,ScrollOptions={});
    [[nodiscard]] core::Result<Widget> content_host(Widget parent,std::string_view id,Size={});
    [[nodiscard]] core::Result<Widget> action_surface(Widget parent,std::string_view id,ActionOptions={},runtime::Task={});
    [[nodiscard]] core::Result<Widget> image(Widget parent,std::string_view id,ImageOptions);
    [[nodiscard]] core::Result<Widget> spacer(Widget parent, std::string_view id, double minimum_length=0);
    [[nodiscard]] core::Result<Widget> label(Widget parent, std::string_view id, LabelOptions = {});
    [[nodiscard]] core::Result<Widget> button(Widget parent, std::string_view id, ButtonOptions = {}, runtime::Task action = {});
    [[nodiscard]] core::Result<Widget> tab(Widget parent,std::string_view id,TabOptions,runtime::Task action={});
    [[nodiscard]] core::Result<Widget> tab_bar(Widget parent,std::string_view id,TabBarOptions={},std::function<void(int)> changed={});
    [[nodiscard]] core::Result<Widget> combo_box(Widget parent,std::string_view id,ComboBoxOptions={},ControlEvents={});
    [[nodiscard]] core::Result<Widget> stepper(Widget parent,std::string_view id,StepperOptions={},std::function<void(int)> stepped={},ControlEvents={});
    [[nodiscard]] core::Result<Widget> segmented_control(Widget parent,std::string_view id,SegmentedOptions={});
    [[nodiscard]] core::Result<Widget> menu(Widget parent,std::string_view id,MenuOptions={},MenuEvents={});
    // One content view, plus any separately owned popups. The content remains
    // mounted while closed; opening does not reconstruct its C++ backend.
    [[nodiscard]] core::Result<Widget> popover(Widget parent,std::string_view id,PopoverOptions={},PopoverEvents={});
    [[nodiscard]] core::Result<Widget> menu_item(Widget parent,std::string_view id,MenuItemOptions={},runtime::Task={});
    [[nodiscard]] core::Result<Widget> menu_divider(Widget parent,std::string_view id,MenuDividerOptions={});
    [[nodiscard]] core::Result<Widget> check_box(Widget parent, std::string_view id, CheckOptions = {}, std::function<void(bool)> = {});
    [[nodiscard]] core::Result<Widget> radio_button(Widget parent, std::string_view id, CheckOptions = {}, std::function<void(bool)> = {});
    [[nodiscard]] core::Result<Widget> toggle_switch(Widget parent, std::string_view id, CheckOptions = {}, std::function<void(bool)> = {});
    [[nodiscard]] core::Result<Widget> input_field(Widget parent, std::string_view id, InputOptions = {}, std::function<void(std::string)> edited = {}, runtime::Task submitted = {});
    [[nodiscard]] core::Result<Widget> slider(Widget parent, std::string_view id, SliderOptions = {}, std::function<void(double)> = {});
    [[nodiscard]] core::Result<Widget> progress(Widget parent, std::string_view id, ProgressOptions = {});
    [[nodiscard]] core::Result<Widget> color_surface(Widget parent,std::string_view id,ColorSurfaceOptions,std::function<void(WidgetColor)> edited={});
    [[nodiscard]] core::Result<Widget> color_button(Widget parent,std::string_view id,ColorButtonOptions,ControlEvents={});
    // Retain scope-owned C++ objects even when no control captures them.
    [[nodiscard]] core::Result<void> retain(std::shared_ptr<void> owner);
    // Deliver once on the owner thread after this generation and every
    // containing candidate are committed. Failed candidates never deliver.
    // Hooks are short and must not throw; exceptions cannot undo publication.
    [[nodiscard]] core::Result<void> on_publish(runtime::Task);
    // Close admission and detach bindings before interaction notifications,
    // child teardown and retained-owner destruction. Called once per generation.
    [[nodiscard]] core::Result<void> on_retire(runtime::Task);
private:
    friend class Host;
    explicit WidgetBuilder(std::shared_ptr<detail::WidgetState> value) : state_(std::move(value)) {}
    friend class detail::WidgetState;
    std::shared_ptr<detail::WidgetState> state_;
};
} // namespace ii::gui
