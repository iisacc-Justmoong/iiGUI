#include "NativeWidgets.h"
#include "WidgetState.h"

namespace ii::gui {
Widget::Widget(std::weak_ptr<detail::WidgetState> state, std::size_t index, std::thread::id owner)
    : state_(std::move(state)), index_(index), owner_(owner) {}
core::Result<std::shared_ptr<detail::WidgetState>> Widget::lock() const {
    // Workers cannot extend the lifetime of native objects past UI teardown.
    if (owner_ != std::thread::id{} && owner_ != std::this_thread::get_id())
        return core::fail(core::ErrorCode::wrong_executor, "Native widgets require the host thread");
    auto value = state_.lock();
    if (!value) return core::fail(core::ErrorCode::closed, "Widget mount has closed");
    if (auto ready = value->ready(); !ready) return std::unexpected(ready.error());
    return value;
}
core::Result<void> Widget::set_text(std::string_view text) const {
    auto state = lock(); if (!state) return std::unexpected(state.error());
    return (*state)->text(index_, text);
}
core::Result<WidgetPlacement> Widget::placement()const{auto s=lock();if(!s)return std::unexpected(s.error());return (*s)->placement(index_);}
core::Result<void> Widget::set_label_options(LabelOptions v) const {auto s=lock();if(!s)return std::unexpected(s.error());return (*s)->label_options(index_,std::move(v));}
core::Result<void> Widget::set_input_options(InputOptions v) const {auto s=lock();if(!s)return std::unexpected(s.error());return (*s)->input_options(index_,std::move(v));}
core::Result<void> Widget::set_clip_content(bool v) const {auto s=lock();if(!s)return std::unexpected(s.error());return (*s)->clip_content(index_,v);}
core::Result<void> Widget::set_accessibility_name(std::string_view v) const {auto s=lock();if(!s)return std::unexpected(s.error());return (*s)->accessibility_name(index_,v);}
core::Result<void> Widget::set_enabled(bool enabled) const {
    auto state = lock(); if (!state) return std::unexpected(state.error());
    return (*state)->enabled(index_, enabled);
}
core::Result<void> Widget::set_checked(bool value) const {
    auto state=lock(); if (!state) return std::unexpected(state.error());
    return (*state)->checked(index_,value);
}
core::Result<void> Widget::set_value(double value) const {
    auto state=lock(); if (!state) return std::unexpected(state.error());
    return (*state)->value(index_,value);
}
core::Result<void> Widget::set_selected(bool value) const {
    auto state=lock();if(!state) return std::unexpected(state.error());return (*state)->selected(index_,value);
}
core::Result<void> Widget::set_current_index(int value) const {
    auto state=lock();if(!state) return std::unexpected(state.error());return (*state)->current_index(index_,value);
}
core::Result<void> Widget::focus() const {
    auto state=lock(); if (!state) return std::unexpected(state.error());
    return (*state)->focus(index_);
}
core::Result<void> Widget::activate() const {
    auto state = lock(); if (!state) return std::unexpected(state.error());
    return (*state)->activate(index_);
}
core::Result<void> Widget::step(int direction) const {
    auto state=lock();if(!state) return std::unexpected(state.error());return (*state)->step(index_,direction);
}
core::Result<void> Widget::open_at(double x,double y) const { auto s=lock();if(!s) return std::unexpected(s.error());return (*s)->open_at(index_,x,y); }
core::Result<void> Widget::open_for(Widget anchor,double x,double y) const { auto s=lock();if(!s) return std::unexpected(s.error());return (*s)->open_for(index_,anchor,x,y); }
core::Result<void> Widget::close_popup() const { auto s=lock();if(!s) return std::unexpected(s.error());return (*s)->close_popup(index_); }
core::Result<void> Widget::cancel_popup() const {auto s=lock();if(!s)return std::unexpected(s.error());return (*s)->close_popup(index_,true);}
core::Result<void> Widget::set_selected_index(int i) const { auto s=lock();if(!s) return std::unexpected(s.error());return (*s)->selected_index(index_,i); }
core::Result<void> Widget::set_menu_state(MenuItemState v) const { auto s=lock();if(!s) return std::unexpected(s.error());return (*s)->menu_state(index_,v); }
core::Result<void> Widget::set_menu_expanded(bool v) const { auto s=lock();if(!s) return std::unexpected(s.error());return (*s)->menu_expanded(index_,v); }
core::Result<void> Widget::set_context_menu(Widget p) const { auto s=lock();if(!s) return std::unexpected(s.error());return (*s)->context_menu(index_,p); }
core::Result<void> Widget::set_color_model(std::shared_ptr<ColorPickerModel> p) const {auto s=lock();if(!s)return std::unexpected(s.error());return (*s)->color_model(index_,std::move(p));}
core::Result<void> Widget::set_popup_content(WidgetFactory f) const {auto s=lock();if(!s)return std::unexpected(s.error());return (*s)->popup_factory(index_,std::move(f));}
core::Result<void> Widget::set_popup_content_at(Widget anchor,WidgetFactory f) const {auto s=lock();if(!s)return std::unexpected(s.error());return (*s)->popup_factory(index_,std::move(f),anchor);}
core::Result<Widget> Widget::popup_content(std::string_view id) const {auto s=lock();if(!s)return std::unexpected(s.error());return (*s)->popup_content(index_,id);}
core::Result<std::shared_ptr<ColorPickerModel>> Widget::color_model() const {auto s=lock();if(!s)return std::unexpected(s.error());return (*s)->color_model(index_);}
core::Result<void> Widget::set_color_field(ColorField f) const {auto s=lock();if(!s)return std::unexpected(s.error());return (*s)->color_field(index_,f);}
core::Result<void> Widget::set_color_button_size(double v) const {auto s=lock();if(!s)return std::unexpected(s.error());return (*s)->color_button_size(index_,v);}
core::Result<void> Widget::set_show_hue_ring(bool v) const {auto s=lock();if(!s)return std::unexpected(s.error());return (*s)->show_hue_ring(index_,v);}
core::Result<void> Widget::set_frame(Rect r) const {auto s=lock();if(!s)return std::unexpected(s.error());return (*s)->fixed_frame(index_,r);}
core::Result<void> Widget::set_extent(Size size) const {auto s=lock();if(!s)return std::unexpected(s.error());return (*s)->extent(index_,size);}
core::Result<void> Widget::set_flex(double v) const {auto s=lock();if(!s)return std::unexpected(s.error());return (*s)->flex(index_,v);}
core::Result<void> Widget::set_scroll_options(ScrollOptions v) const {auto s=lock();if(!s)return std::unexpected(s.error());return (*s)->scroll_options(index_,v);}
core::Result<void> Widget::set_scroll_offset(double v) const {auto s=lock();if(!s)return std::unexpected(s.error());return (*s)->scroll_offset(index_,v);}
core::Result<void> Widget::set_horizontal_scroll_offset(double v) const {auto s=lock();if(!s)return std::unexpected(s.error());return (*s)->horizontal_scroll_offset(index_,v);}
core::Result<void> Widget::set_scroll_position(double x,double y) const {auto s=lock();if(!s)return std::unexpected(s.error());return (*s)->scroll_position(index_,x,y);}
core::Result<void> Widget::set_scroll_handler(std::function<void(const ScrollViewport&)> v) const {auto s=lock();if(!s)return std::unexpected(s.error());return (*s)->scroll_handler(index_,std::move(v));}
core::Result<void> Widget::set_content(WidgetFactory factory,std::function<core::Result<void>()> commit) const {auto s=lock();if(!s)return std::unexpected(s.error());return (*s)->content(index_,std::move(factory),std::move(commit));}
core::Result<void> Widget::clear_content() const {return set_content({});}
core::Result<Widget> Widget::content_item(std::string_view id) const {auto s=lock();if(!s)return std::unexpected(s.error());return (*s)->content_item(index_,id);}
core::Result<void> Widget::set_layout_handler(std::function<void(Rect)> v) const {auto s=lock();if(!s)return std::unexpected(s.error());return (*s)->layout_handler(index_,std::move(v));}
core::Result<void> Widget::set_drawing(WidgetDrawing v) const {auto s=lock();if(!s)return std::unexpected(s.error());return (*s)->drawing(index_,std::move(v));}
core::Result<void> Widget::set_input_handler(WidgetInputHandler v) const {auto s=lock();if(!s)return std::unexpected(s.error());return (*s)->input_handler(index_,std::move(v));}
core::Result<void> Widget::set_stack_options(StackOptions v) const {auto s=lock();if(!s)return std::unexpected(s.error());return (*s)->stack_options(index_,std::move(v));}
core::Result<void> Widget::set_button_style(ButtonStyle v) const {auto s=lock();if(!s)return std::unexpected(s.error());return (*s)->button_style(index_,std::move(v));}
core::Result<void> Widget::set_button_icon(std::shared_ptr<const RasterImage> v,std::string_view name) const {auto s=lock();if(!s)return std::unexpected(s.error());return (*s)->button_icon(index_,std::move(v),name);}
core::Result<void> Widget::set_action_options(ActionOptions v)const{auto s=lock();if(!s)return std::unexpected(s.error());return (*s)->action_options(index_,std::move(v));}
core::Result<ActionOptions> Widget::action_options()const{auto s=lock();if(!s)return std::unexpected(s.error());return (*s)->action_options(index_);}
core::Result<void> Widget::set_control_events(ControlEvents v) const {auto s=lock();if(!s)return std::unexpected(s.error());return (*s)->control_events(index_,std::move(v));}
core::Result<void> Widget::set_image_options(ImageOptions v) const {auto s=lock();if(!s)return std::unexpected(s.error());return (*s)->image_options(index_,std::move(v));}
core::Result<void> Widget::set_popover_options(PopoverOptions v) const {auto s=lock();if(!s)return std::unexpected(s.error());return (*s)->popover_options(index_,std::move(v));}
core::Result<void> Widget::set_visible(bool v) const {auto s=lock();if(!s)return std::unexpected(s.error());return (*s)->visibility(index_,v);}
core::Result<void> Widget::set_image(std::shared_ptr<const RasterImage> v) const {auto s=lock();if(!s)return std::unexpected(s.error());return (*s)->image(index_,std::move(v));}
core::Result<void> Widget::set_escape_action(runtime::Task v) const {auto s=lock();if(!s)return std::unexpected(s.error());return (*s)->escape_action(index_,std::move(v));}
core::Result<WidgetSnapshot> Widget::snapshot() const {
    auto state = lock(); if (!state) return std::unexpected(state.error());
    return (*state)->snapshot(index_);
}
WidgetMount::WidgetMount(std::weak_ptr<detail::WidgetState> state)
    : state_(std::move(state)), owner_(std::this_thread::get_id()) {}
core::Result<WidgetAppearance> WidgetBuilder::appearance() const {return state_->appearance();}
core::Result<std::shared_ptr<detail::WidgetState>> WidgetMount::lock() const {
    if (owner_ != std::thread::id{} && owner_ != std::this_thread::get_id())
        return core::fail(core::ErrorCode::wrong_executor, "Native mount requires the host thread");
    auto value = state_.lock();
    if (!value) return core::fail(core::ErrorCode::closed, "Widget mount has closed");
    if (auto ready = value->ready(); !ready) return std::unexpected(ready.error());
    return value;
}
core::Result<Widget> WidgetMount::find(std::string_view id) const {
    auto state = lock(); if (!state) return std::unexpected(state.error());
    return (*state)->find(id);
}
core::Result<std::vector<WidgetSnapshot>> WidgetMount::snapshot() const {
    auto state = lock(); if (!state) return std::unexpected(state.error());
    return (*state)->snapshots();
}
core::Result<void> WidgetMount::dispatch_input(const WidgetInput& event) const {
    auto state=lock(); if (!state) return std::unexpected(state.error());
    return (*state)->input(event);
}
core::Result<Widget> WidgetBuilder::vstack(Widget parent, std::string_view id, StackOptions options) {
    return state_->add(WidgetKind::vertical_stack, parent, id, options, {}, {}, {});
}
core::Result<Widget> WidgetBuilder::hstack(Widget parent, std::string_view id, StackOptions options) {
    return state_->add(WidgetKind::horizontal_stack, parent, id, options, {}, {}, {});
}
core::Result<Widget> WidgetBuilder::zstack(Widget parent, std::string_view id, StackOptions options) {
    return state_->add(WidgetKind::overlay_stack,parent,id,options,{},{},{});
}
core::Result<Widget> WidgetBuilder::absolute_stack(Widget p,std::string_view id,Size size) {return state_->absolute_stack(p,id,size);}
core::Result<Widget> WidgetBuilder::scroll_view(Widget p,std::string_view id,ScrollOptions o) {return state_->scroll_view(p,id,o);}
core::Result<Widget> WidgetBuilder::content_host(Widget p,std::string_view id,Size size) {return state_->content_host(p,id,size);}
core::Result<Widget> WidgetBuilder::action_surface(Widget p,std::string_view id,ActionOptions o,runtime::Task task){return state_->action_surface(p,id,std::move(o),std::move(task));}
core::Result<Widget> WidgetBuilder::image(Widget p,std::string_view id,ImageOptions options) {return state_->image(p,id,std::move(options));}
core::Result<Widget> WidgetBuilder::spacer(Widget parent,std::string_view id,double minimum) {
    return state_->add(WidgetKind::spacer,parent,id,{.spacing=minimum},{},{},{});
}
core::Result<Widget> WidgetBuilder::label(Widget parent, std::string_view id, LabelOptions options) {
    return state_->add(WidgetKind::label, parent, id, {}, std::move(options), {}, {});
}
core::Result<Widget> WidgetBuilder::button(Widget parent, std::string_view id, ButtonOptions options, runtime::Task action) {
    return state_->add(WidgetKind::button, parent, id, {}, {}, std::move(options), std::move(action));
}
core::Result<Widget> WidgetBuilder::tab(Widget p,std::string_view id,TabOptions o,runtime::Task f) { return state_->tab(p,id,std::move(o),std::move(f)); }
core::Result<Widget> WidgetBuilder::tab_bar(Widget p,std::string_view id,TabBarOptions o,std::function<void(int)> f) { return state_->tab_bar(p,id,o,std::move(f)); }
core::Result<Widget> WidgetBuilder::combo_box(Widget p,std::string_view id,ComboBoxOptions o,ControlEvents e) { return state_->combo_box(p,id,std::move(o),std::move(e)); }
core::Result<Widget> WidgetBuilder::stepper(Widget p,std::string_view id,StepperOptions o,std::function<void(int)> f,ControlEvents e) { return state_->stepper(p,id,std::move(o),std::move(f),std::move(e)); }
core::Result<Widget> WidgetBuilder::segmented_control(Widget p,std::string_view id,SegmentedOptions o) { return state_->segmented_control(p,id,o); }
core::Result<Widget> WidgetBuilder::menu(Widget p,std::string_view id,MenuOptions o,MenuEvents e) { return state_->menu(p,id,std::move(o),std::move(e)); }
core::Result<Widget> WidgetBuilder::popover(Widget p,std::string_view id,PopoverOptions o,PopoverEvents e) {return state_->popover(p,id,std::move(o),std::move(e));}
core::Result<Widget> WidgetBuilder::menu_item(Widget p,std::string_view id,MenuItemOptions o,runtime::Task f) { return state_->menu_item(p,id,std::move(o),std::move(f)); }
core::Result<Widget> WidgetBuilder::menu_divider(Widget p,std::string_view id,MenuDividerOptions o) { return state_->menu_divider(p,id,std::move(o)); }
core::Result<Widget> WidgetBuilder::check_box(Widget p,std::string_view id,CheckOptions o,std::function<void(bool)> f) {
    return state_->check(WidgetKind::check_box,p,id,std::move(o),std::move(f));
}
core::Result<Widget> WidgetBuilder::radio_button(Widget p,std::string_view id,CheckOptions o,std::function<void(bool)> f) {
    return state_->check(WidgetKind::radio_button,p,id,std::move(o),std::move(f));
}
core::Result<Widget> WidgetBuilder::toggle_switch(Widget p,std::string_view id,CheckOptions o,std::function<void(bool)> f) {
    return state_->check(WidgetKind::toggle_switch,p,id,std::move(o),std::move(f));
}
core::Result<Widget> WidgetBuilder::input_field(Widget p,std::string_view id,InputOptions o,std::function<void(std::string)> f,runtime::Task s) {
    return state_->input_field(p,id,std::move(o),std::move(f),std::move(s));
}
core::Result<Widget> WidgetBuilder::slider(Widget p,std::string_view id,SliderOptions o,std::function<void(double)> f) {
    return state_->slider(p,id,o,std::move(f));
}
core::Result<Widget> WidgetBuilder::progress(Widget p,std::string_view id,ProgressOptions o) {
    return state_->progress(p,id,o);
}
core::Result<void> WidgetBuilder::set_default_content(Widget host,Widget slot){return state_->default_content(host,slot);}
core::Result<void> Widget::set_button_options(ButtonOptions options)const{auto s=lock();if(!s)return std::unexpected(s.error());return (*s)->button_options(index_,std::move(options));}
core::Result<void> WidgetBuilder::retain(std::shared_ptr<void> owner) {
    return state_->retain(std::move(owner));
}
core::Result<Widget> WidgetBuilder::color_surface(Widget p,std::string_view id,ColorSurfaceOptions o,std::function<void(WidgetColor)> f) {return state_->color_surface(p,id,std::move(o),std::move(f));}
core::Result<Widget> WidgetBuilder::color_button(Widget p,std::string_view id,ColorButtonOptions o,ControlEvents e) {return state_->color_button(p,id,std::move(o),std::move(e));}
} // namespace ii::gui
