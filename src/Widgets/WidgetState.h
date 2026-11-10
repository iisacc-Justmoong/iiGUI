#pragma once
#include <iiGUI/NativeWidgets.h>
#include <iiGUI/FontDatabase.h>
#include <iiSDKCore/Runtime/FrameLoop.h>
#include "WidgetSurface.h"

namespace ii::gui::detail {
struct WidgetFrameHooks final {
    std::function<core::Result<runtime::FrameLoop*>()> frames;
    std::function<void(bool)> activity;
    std::shared_ptr<MotionPreferences> preferences;
    Renderer renderer=Renderer::automatic;
    std::function<core::Result<runtime::EventLoop*>()> events;
    std::function<core::Result<void>(const WindowGeometry&)> window_geometry;
    std::shared_ptr<FontDatabase> fonts;
    std::shared_ptr<PresentationStream> presentations;
    // Optional owner-thread accessibility observer, independent of an OS surface.
    std::function<void()> accessibility_changed;
    std::function<runtime::Task(bool)> text_input_change;
    // Deferred value refresh for accessibility transports using projections.
    std::function<void()> accessibility_value_changed;
    // A borrowed host surface composites over its owning application window.
    bool transparent_background=false;
    // Root external views coalesce this signal on their supplied event loop.
    std::function<void()> invalidated;
};
class WidgetState final : public std::enable_shared_from_this<WidgetState> {
public:
    // invoke is the synchronous owner-thread Host exception/admission barrier;
    // it must complete or reject work before returning, never queue it.
    explicit WidgetState(std::function<void(runtime::Task)> invoke, WidgetAppearance = {},WidgetFrameHooks = {});
    ~WidgetState();
    [[nodiscard]] core::Result<void> ready() const;
    [[nodiscard]] core::Result<void> construct(WidgetFactory);
    // Complete a surface-independent widget tree after factory construction.
    [[nodiscard]] core::Result<void> activate_paint_view();
    [[nodiscard]] core::Result<void> begin_external_paint();
    void end_external_paint() noexcept;
    [[nodiscard]] core::Result<WidgetAppearance> appearance() const;
    [[nodiscard]] core::Result<void> window_properties(WindowProperties);
    [[nodiscard]] core::Result<WindowProperties> window_properties() const;
    [[nodiscard]] core::Result<void> commit_window_properties();
    [[nodiscard]] core::Result<std::shared_ptr<MotionPreferences>> motion_preferences();
    [[nodiscard]] core::Result<std::reference_wrapper<runtime::EventLoop>> event_loop();
    [[nodiscard]] core::Result<void> ensure_frame_tick();
    [[nodiscard]] core::Result<void> frame_handler(std::size_t,WidgetFrameHandler,bool);
    [[nodiscard]] core::Result<void> request_frames(std::size_t);
    [[nodiscard]] core::Result<void> button_options(std::size_t,ButtonOptions);
    [[nodiscard]] core::Result<void> default_content(Widget,Widget);
    [[nodiscard]] core::Result<Widget> add(WidgetKind, Widget, std::string_view,
        StackOptions, LabelOptions, ButtonOptions, runtime::Task);
    [[nodiscard]] core::Result<Widget> check(WidgetKind, Widget, std::string_view, CheckOptions, std::function<void(bool)>);
    [[nodiscard]] core::Result<Widget> input_field(Widget, std::string_view, InputOptions, std::function<void(std::string)>, runtime::Task);
    [[nodiscard]] core::Result<Widget> slider(Widget, std::string_view, SliderOptions, std::function<void(double)>);
    [[nodiscard]] core::Result<Widget> progress(Widget, std::string_view, ProgressOptions);
    [[nodiscard]] core::Result<Widget> absolute_stack(Widget,std::string_view,Size);
    [[nodiscard]] core::Result<Widget> scroll_view(Widget,std::string_view,ScrollOptions);
    [[nodiscard]] core::Result<Widget> content_host(Widget,std::string_view,Size);
    [[nodiscard]] core::Result<Widget> action_surface(Widget,std::string_view,ActionOptions,runtime::Task);
    [[nodiscard]] core::Result<void> action_options(std::size_t,ActionOptions);
    [[nodiscard]] core::Result<ActionOptions> action_options(std::size_t)const;
    [[nodiscard]] core::Result<void> content(std::size_t,WidgetFactory,std::function<core::Result<void>()>,
        std::optional<std::size_t> replacement_allowance={},std::string scope_namespace={},bool preserve_keyed=false,bool defer_layout=false);
    [[nodiscard]] core::Result<void> collapse_when_empty(std::size_t,bool);
    [[nodiscard]] core::Result<Widget> content_item(std::size_t,std::string_view);
    [[nodiscard]] core::Result<void> keyed_content(std::size_t,std::vector<KeyedContentItem>,KeyedContentOptions,std::function<core::Result<void>()> commit={});
    [[nodiscard]] core::Result<Widget> keyed_content_item(std::size_t,std::string_view,std::string_view);
    [[nodiscard]] core::Result<std::vector<std::string>> keyed_content_keys(std::size_t);
    [[nodiscard]] core::Result<Widget> image(Widget,std::string_view,ImageOptions);
    [[nodiscard]] core::Result<Widget> color_surface(Widget,std::string_view,ColorSurfaceOptions,std::function<void(WidgetColor)>);
    [[nodiscard]] core::Result<Widget> color_button(Widget,std::string_view,ColorButtonOptions,ControlEvents);
    [[nodiscard]] core::Result<Widget> tab(Widget,std::string_view,TabOptions,runtime::Task);
    [[nodiscard]] core::Result<Widget> tab_bar(Widget,std::string_view,TabBarOptions,std::function<void(int)>);
    [[nodiscard]] core::Result<Widget> combo_box(Widget,std::string_view,ComboBoxOptions,ControlEvents);
    [[nodiscard]] core::Result<Widget> stepper(Widget,std::string_view,StepperOptions,std::function<void(int)>,ControlEvents);
    [[nodiscard]] core::Result<Widget> segmented_control(Widget,std::string_view,SegmentedOptions);
    [[nodiscard]] core::Result<Widget> menu(Widget,std::string_view,MenuOptions,MenuEvents);
    [[nodiscard]] core::Result<Widget> popover(Widget,std::string_view,PopoverOptions,PopoverEvents,PopoverMotionAbiV2={});
    [[nodiscard]] core::Result<Widget> menu_item(Widget,std::string_view,MenuItemOptions,runtime::Task);
    [[nodiscard]] core::Result<Widget> menu_divider(Widget,std::string_view,MenuDividerOptions);
    [[nodiscard]] core::Result<void> attach(void* parent, WidgetSurfaceFactory);
    [[nodiscard]] core::Result<void> retain(std::shared_ptr<void>);
    [[nodiscard]] core::Result<void> on_publish(runtime::Task);
    // Called by the native host only after a root mount has committed.
    void commit_publication() noexcept;
    [[nodiscard]] core::Result<void> on_retire(runtime::Task);
    void close() noexcept;
    [[nodiscard]] core::Result<Widget> find(std::string_view);
    [[nodiscard]] core::Result<WidgetSnapshot> snapshot(std::size_t);
    [[nodiscard]] core::Result<WidgetPlacement> placement(std::size_t)const;
    [[nodiscard]] core::Result<CanvasScene> snapshot_scene(std::size_t);
    [[nodiscard]] core::Result<WidgetSceneSnapshot> snapshot_scene_graph(std::size_t);
    [[nodiscard]] core::Result<bool> scene_within(std::size_t,const Widget&)const;
    [[nodiscard]] core::Result<bool> within(std::size_t,const Widget&)const;
    [[nodiscard]] core::Result<WidgetInteraction> interaction(std::size_t);
    [[nodiscard]] core::Result<WidgetPressOrigin> press_origin(std::size_t)const;
    [[nodiscard]] core::Result<Size> measure(std::size_t,double);
    [[nodiscard]] core::Result<void> observe_interaction(std::size_t,std::shared_ptr<const WidgetInteractionPort>);
    [[nodiscard]] core::Result<void> interaction_handler(std::size_t,WidgetInteractionHandler);
    [[nodiscard]] core::Result<std::vector<WidgetSnapshot>> snapshots();
    [[nodiscard]] core::Result<std::optional<WidgetSnapshot>> focused_text_editor();
    // Includes virtual accessibility children without adding retained widgets.
    [[nodiscard]] core::Result<std::vector<WidgetSnapshot>> accessibility_snapshots();
    [[nodiscard]] core::Result<WidgetSnapshot> accessibility_snapshot(std::string_view);
    // Owner-thread, bounded scene inspection. Does not create an OS surface.
    [[nodiscard]] core::Result<CanvasFrame> drawing_snapshot(Size);
    // Probe a finger gesture through embedded views without mutating input.
    [[nodiscard]] bool touch_scrollable_at(double x, double y);
    [[nodiscard]] core::Result<void> text(std::size_t, std::string_view);
    [[nodiscard]] core::Result<void> label_options(std::size_t,LabelOptions);
    [[nodiscard]] core::Result<void> label_color(std::size_t,std::optional<WidgetColor>);
    [[nodiscard]] core::Result<void> input_options(std::size_t,InputOptions);
    [[nodiscard]] core::Result<void> clip_content(std::size_t,bool);
    [[nodiscard]] core::Result<void> content_input_passthrough(std::size_t,bool);
    [[nodiscard]] core::Result<void> opacity(std::size_t,double);
    [[nodiscard]] core::Result<void> accessibility_name(std::size_t,std::string_view);
    [[nodiscard]] core::Result<void> accessibility_semantics(std::size_t,std::optional<AccessibilitySemantics>);
    [[nodiscard]] core::Result<void> accessibility_value_handler(std::size_t,std::function<core::Result<void>(double)>);
    [[nodiscard]] core::Result<bool> can_set_accessibility_value(std::string_view,double);
    [[nodiscard]] core::Result<void> set_accessibility_value(std::string_view,double);
    // Owner-thread user edit, guarded by the published text generation.
    [[nodiscard]] core::Result<void> replace_accessibility_text(std::string_view,const TextInputRevision&,std::string);
    [[nodiscard]] core::Result<void> virtual_accessibility(std::size_t,std::vector<VirtualAccessibilityNode>,VirtualAccessibilityAction);
    [[nodiscard]] core::Result<void> enabled(std::size_t, bool);
    [[nodiscard]] core::Result<void> checked(std::size_t, bool);
    [[nodiscard]] core::Result<void> value(std::size_t, double);
    [[nodiscard]] core::Result<void> selected(std::size_t,bool);
    [[nodiscard]] core::Result<void> current_index(std::size_t,int);
    [[nodiscard]] core::Result<void> focus(std::size_t);
    [[nodiscard]] core::Result<void> reveal_local_rect(std::size_t,Rect);
    [[nodiscard]] core::Result<void> activate(std::size_t);
    [[nodiscard]] core::Result<void> step(std::size_t,int);
    [[nodiscard]] core::Result<void> open_at(std::size_t,double,double);
    [[nodiscard]] core::Result<void> open_for(std::size_t,Widget,double,double);
    [[nodiscard]] core::Result<void> close_popup(std::size_t,bool canceled=false);
    [[nodiscard]] core::Result<void> popup_factory(std::size_t,WidgetFactory,Widget anchor={});
    [[nodiscard]] core::Result<Widget> popup_content(std::size_t,std::string_view);
    [[nodiscard]] core::Result<void> selected_index(std::size_t,int);
    [[nodiscard]] core::Result<void> menu_state(std::size_t,MenuItemState);
    [[nodiscard]] core::Result<void> menu_expanded(std::size_t,bool);
    [[nodiscard]] core::Result<void> context_menu(std::size_t,Widget);
    [[nodiscard]] core::Result<void> color_model(std::size_t,std::shared_ptr<ColorPickerModel>);
    [[nodiscard]] core::Result<std::shared_ptr<ColorPickerModel>> color_model(std::size_t);
    [[nodiscard]] core::Result<void> color_field(std::size_t,ColorField);
    [[nodiscard]] core::Result<void> color_button_size(std::size_t,double);
    [[nodiscard]] core::Result<void> show_hue_ring(std::size_t,bool);
    [[nodiscard]] core::Result<void> fixed_frame(std::size_t,Rect);
    [[nodiscard]] core::Result<void> extent(std::size_t,Size);
    [[nodiscard]] core::Result<void> flex(std::size_t,double);
    [[nodiscard]] core::Result<void> scroll_options(std::size_t,ScrollOptions);
    [[nodiscard]] core::Result<void> scroll_offset(std::size_t,double);
    [[nodiscard]] core::Result<void> horizontal_scroll_offset(std::size_t,double);
    [[nodiscard]] core::Result<void> scroll_position(std::size_t,double,double);
    [[nodiscard]] core::Result<void> virtual_scroll_content(std::size_t,Size,double,double);
    [[nodiscard]] core::Result<void> scroll_handler(std::size_t,std::function<void(const ScrollViewport&)>);
    [[nodiscard]] core::Result<void> layout_handler(std::size_t,std::function<void(Rect)>);
    [[nodiscard]] core::Result<void> measurement_handler(std::size_t,std::function<void(const WidgetMeasurement&)>);
    [[nodiscard]] core::Result<void> drawing(std::size_t,WidgetDrawing);
    [[nodiscard]] core::Result<void> paint_handler(std::size_t,WidgetPaintHandler,Widget);
    [[nodiscard]] core::Result<void> backdrop_sources(std::size_t,Widget,Widget);
    [[nodiscard]] core::Result<void> repaint(std::size_t);
    [[nodiscard]] core::Result<void> input_handler(std::size_t,WidgetInputHandler);
    [[nodiscard]] core::Result<void> text_input_provider(std::size_t,std::shared_ptr<const TextInputProvider>);
#if defined(IIGUI_TEXT_GEOMETRY_API)
    [[nodiscard]] core::Result<void> text_geometry_provider(std::size_t,TextGeometryProvider);
#endif
#if defined(IIGUI_TEXT_CHANGE_SCOPE_API)
    [[nodiscard]] core::Result<TextInputChange> begin_text_input_change(std::size_t,bool,bool);
#endif
    [[nodiscard]] core::Result<void> text_input_changed(std::size_t,bool);
    [[nodiscard]] core::Result<TextInputState> text_input_state(std::size_t);
    [[nodiscard]] core::Result<TextContent> text_content(std::size_t,TextRange);
#if defined(IIGUI_TEXT_GEOMETRY_API)
    [[nodiscard]] core::Result<TextInputGeometry> text_geometry(std::size_t,TextRange);
#endif
    [[nodiscard]] core::Result<std::size_t> text_position(std::size_t,double,double);
    [[nodiscard]] core::Result<void> stack_options(std::size_t,StackOptions);
    [[nodiscard]] core::Result<void> stack_layout(std::size_t,StackLayoutOptions);
    [[nodiscard]] core::Result<StackLayoutOptions> stack_layout(std::size_t);
    [[nodiscard]] core::Result<void> button_style(std::size_t,ButtonStyle);
    [[nodiscard]] core::Result<void> button_icon(std::size_t,std::shared_ptr<const RasterImage>,std::string_view);
    [[nodiscard]] core::Result<void> control_events(std::size_t,ControlEvents);
    [[nodiscard]] core::Result<void> image_options(std::size_t,ImageOptions);
    [[nodiscard]] core::Result<void> popover_options(std::size_t,PopoverOptions,PopoverMotionAbiV2={});
    [[nodiscard]] core::Result<void> popover_underlay(std::size_t,PopoverUnderlayProvider,WidgetColor);
    [[nodiscard]] core::Result<void> popover_underlay_with_bounds(std::size_t,PopoverUnderlayBoundsProvider,WidgetColor);
    [[nodiscard]] core::Result<void> popover_untinted_underlay_with_bounds(std::size_t,PopoverUnderlayBoundsProvider);
    [[nodiscard]] core::Result<void> popover_material_follows_content(std::size_t,bool);
    [[nodiscard]] core::Result<void> visibility(std::size_t,bool);
    [[nodiscard]] core::Result<void> image(std::size_t,std::shared_ptr<const RasterImage>);
    [[nodiscard]] core::Result<void> escape_action(std::size_t,runtime::Task);
    [[nodiscard]] core::Result<void> input(const WidgetInput&);
    [[nodiscard]] core::Result<void> touch_begin(double timestamp);
    [[nodiscard]] core::Result<void> touch_end(double timestamp);
    [[nodiscard]] core::Result<void> touch_cancel();
    [[nodiscard]] core::Result<GraphicsInfo> graphics_info();
    [[nodiscard]] core::Result<PixelBuffer> capture();
    void present();
    bool native_event(const void*);
#if defined(_WIN32)
    std::optional<std::intptr_t> native_result(const void*);
#endif
    void set_frame_paced(bool);
private:
    struct VirtualTarget final {std::shared_ptr<WidgetState> owner;std::size_t node=0,child=0;};
    [[nodiscard]] core::Result<VirtualTarget> virtual_target(std::string_view);
    [[nodiscard]] core::Result<WidgetSnapshot> virtual_snapshot(std::size_t,std::size_t,const WidgetSnapshot* parent=nullptr);
    [[nodiscard]] core::Result<void> append_virtual_accessibility(std::vector<WidgetSnapshot>&);
    [[nodiscard]] bool published_in_tree() const noexcept;
    void deliver_publication() noexcept;
    [[nodiscard]] core::Result<void> observe_fonts();
    void fonts_changed();
    void apply_appearance(const WidgetAppearance&);
    [[nodiscard]] core::Result<std::size_t> prepare(Widget, std::string_view,WidgetKind=WidgetKind::label);
    Widget publish(std::size_t, WidgetKind, std::string_view);
    struct CaptureFilter;
    struct SceneBuild;
    core::Result<CanvasFrame> capture_frame(std::size_t,SceneBuild*);
    core::Result<CanvasScene> capture_scene(std::size_t,SceneBuild*);
    CanvasFrame frame(Size,std::size_t* popup_start=nullptr,const CaptureFilter* capture=nullptr,bool captured=false,SceneBuild* scenes=nullptr);
    core::Result<void> flush_viewports();
    core::Result<void> flush_interactions();
    core::Result<void> scroll_position_at(std::size_t,double,double,std::optional<WidgetPoint>);
    core::Result<void> input_impl(const WidgetInput&, WidgetInput* scroll_remainder=nullptr, bool scroll_normalized=false, bool accessibility_text=false);
    void advance_touch_motion(runtime::Clock::time_point);
    core::Result<bool> route_input_handler(const WidgetInput&);
    core::Result<void> create_popup_content(std::size_t);
    core::Result<void> route_content(WidgetInput&,bool& handled,bool& handler_checked,bool& scroll_normalized);
    core::Result<void> route_host_content(WidgetInput&,bool& handled,bool& handler_checked,bool& scroll_normalized);
    core::Result<void> focus_content(std::size_t,int);
    void release_collapsed_focus() noexcept;
    bool has_escape_handler() const;
    void layout_ancestry();
    core::Result<WidgetSnapshot> snapshot_chunk(std::size_t,bool);
    Size global_offset() const;
    Rect visual_bounds_in_window(std::size_t) const;
    core::Result<void> prepare_motion(PopoverOptions&);
    core::Result<void> set_popover_underlay(std::size_t,PopoverUnderlayBoundsProvider,std::optional<WidgetColor>);
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace ii::gui::detail
