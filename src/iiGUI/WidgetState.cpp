#include "WidgetState.h"
#include "ColorButton.h"
#include "WidgetSurface.h"
#include "NativeText.h"
#include "ColorSurface.h"
#include "HostCommon.h"
#include "DesktopCommon.h"
#include "TextBoundary.h"
#include <iiSDKCore/Core/Text/Utf8.h>
#include <limits>
#include <map>

namespace ii::gui::detail {
namespace {
constexpr auto absent=std::numeric_limits<std::size_t>::max();
bool horizontal_kind(WidgetKind k) { return k==WidgetKind::horizontal_stack || k==WidgetKind::tab_bar || k==WidgetKind::segmented_control; }
bool stack_kind(WidgetKind k) { return k==WidgetKind::vertical_stack || horizontal_kind(k) || k==WidgetKind::overlay_stack || k==WidgetKind::absolute_stack || k==WidgetKind::scroll_view; }
bool check_kind(WidgetKind k) { return k==WidgetKind::check_box || k==WidgetKind::radio_button || k==WidgetKind::toggle_switch; }
bool interactive(WidgetKind k) { return k==WidgetKind::button || k==WidgetKind::action_surface || check_kind(k) || k==WidgetKind::input_field || k==WidgetKind::slider || k==WidgetKind::tab || k==WidgetKind::combo_box || k==WidgetKind::stepper || k==WidgetKind::menu_item || k==WidgetKind::color_surface; }
bool geometry(double v) { return std::isfinite(v) && v>=0 && v<=32767; }
float linear(double c) { return float(c<=.04045 ? c/12.92 : std::pow((c+.055)/1.055,2.4)); }
Color color(WidgetColor c) { return {linear(c.red),linear(c.green),linear(c.blue),float(c.alpha)}; }
bool valid_color(WidgetColor c) { return geometry(c.red) && geometry(c.green) && geometry(c.blue) && geometry(c.alpha) && c.red<=1 && c.green<=1 && c.blue<=1 && c.alpha<=1; }
bool valid_stack(StackOptions o){if(!geometry(o.spacing)||!geometry(o.padding)||static_cast<unsigned>(o.alignment)>3||!geometry(o.minimum_width)||o.minimum_width>8192||!geometry(o.minimum_height)||o.minimum_height>8192)return false;for(auto insets:{o.insets,o.implicit_insets})if(insets)for(double n:{insets->left,insets->top,insets->right,insets->bottom})if(!geometry(n)||n>8192)return false;return true;}
bool valid_style(const ButtonStyle& s){for(const auto* c:{&s.fill,&s.hover_fill,&s.pressed_fill,&s.disabled_fill,&s.text,&s.disabled_text,&s.border})if(*c&&!valid_color(**c))return false;return geometry(s.border_width)&&geometry(s.line_height)&&s.line_height<=8192&&(!s.text_role||unsigned(*s.text_role)<=7)&&(!s.font_size||(std::isfinite(*s.font_size)&&*s.font_size>=1&&*s.font_size<=256));}
bool valid_button(const ButtonOptions& o){return valid_title(o.text)&&o.text.size()<=65536&&unsigned(o.tone)<=4&&valid_title(o.icon_name)&&o.icon_name.size()<=65536&&valid_title(o.accessibility_name)&&o.accessibility_name.size()<=65536&&geometry(o.icon_size)&&o.icon_size>=1&&o.icon_size<=256&&(!o.style||valid_style(*o.style))&&(!o.height||(geometry(*o.height)&&*o.height>=1))&&(!o.corner_radius||geometry(*o.corner_radius))&&(!o.horizontal_padding||geometry(*o.horizontal_padding))&&(!o.left_padding||geometry(*o.left_padding))&&(!o.right_padding||geometry(*o.right_padding))&&(!o.content_spacing||(std::isfinite(*o.content_spacing)&&*o.content_spacing>=-256&&*o.content_spacing<=8192))&&(!o.indicator_size||(geometry(*o.indicator_size)&&*o.indicator_size>=1&&*o.indicator_size<=256))&&(!o.width||(geometry(*o.width)&&*o.width>=1))&&(!o.icon_mode||o.icon);}
bool valid_label(const LabelOptions& o){return valid_title(o.text)&&o.text.size()<=65536&&std::isfinite(o.font_size)&&o.font_size>=1&&o.font_size<=256&&geometry(o.line_height)&&o.line_height<=8192&&unsigned(o.role)<=7&&unsigned(o.alignment)<=2&&(!o.color||valid_color(*o.color))&&(!o.font_weight||*o.font_weight==400||*o.font_weight==500||*o.font_weight==600||*o.font_weight==700);}
bool valid_action(const ActionOptions& o){
    if(!valid_title(o.accessibility_description)||o.accessibility_description.size()>65536||!valid_title(o.text)||o.text.size()>65536||!valid_title(o.accessibility_name)||o.accessibility_name.size()>65536||unsigned(o.role)>1||unsigned(o.text_role)>7||!std::isfinite(o.font_size)||o.font_size<1||o.font_size>256)return false;
    for(auto n:{o.line_height,o.padding.left,o.padding.top,o.padding.right,o.padding.bottom,o.minimum_width,o.minimum_height,o.corner_radius})if(!geometry(n)||n>8192)return false;
    for(auto n:{o.width,o.height})if(n&&(!geometry(*n)||*n>8192))return false;
    for(auto c:{o.text_color,o.hover_text_color,o.pressed_text_color,o.disabled_text_color})if(c&&!valid_color(*c))return false;return true;
}
bool valid_input(const InputOptions& o){if(!valid_title(o.text)||o.text.size()>65536||!valid_title(o.placeholder)||!valid_title(o.accessibility_name)||!geometry(o.width)||o.width<1||o.maximum_length>65536)return false;if(!o.style)return true;const auto& s=*o.style;for(auto c:{s.background,s.hover_background,s.text})if(c&&!valid_color(*c))return false;return geometry(s.height)&&s.height>=1&&s.height<=8192&&geometry(s.font_size)&&s.font_size>=1&&s.font_size<=256&&geometry(s.line_height)&&s.line_height<=8192&&geometry(s.horizontal_padding)&&s.horizontal_padding<=8192&&geometry(s.corner_radius);}
bool valid_image_options(const ImageOptions& o){return (o.image||o.allow_empty)&&(!o.fit||unsigned(*o.fit)<=2)&&geometry(o.width)&&geometry(o.height)&&o.width&&o.height&&valid_title(o.accessibility_name)&&geometry(o.padding)&&o.padding*2<=std::min(o.width,o.height)&&geometry(o.corner_radius)&&geometry(o.border_width)&&(!o.background||valid_color(*o.background))&&(!o.border||valid_color(*o.border));}
bool valid_transition(const std::optional<PopoverTransition>& value){
    if(!value)return true;const auto& t=*value;
    return t.enter_duration>=runtime::Clock::duration::zero()&&t.exit_duration>=runtime::Clock::duration::zero()&&
        t.enter_duration<=std::chrono::hours(24)&&t.exit_duration<=std::chrono::hours(24)&&
        unsigned(t.enter_easing)<=unsigned(animation::Easing::out_back)&&unsigned(t.exit_easing)<=unsigned(animation::Easing::out_back)&&
        std::isfinite(t.overshoot)&&t.overshoot>=0&&t.overshoot<=4&&std::isfinite(t.collapsed_scale)&&t.collapsed_scale>=.5&&t.collapsed_scale<=1&&std::isfinite(t.origin.width)&&std::isfinite(t.origin.height)&&t.origin.width>=0&&t.origin.width<=1&&t.origin.height>=0&&t.origin.height<=1;
}
bool valid_sheet(const std::optional<SheetLayoutOptions>& value){if(!value)return true;const auto& o=*value;return unsigned(o.detent)<=2&&geometry(o.preferred_height)&&o.preferred_height<=8192&&geometry(o.desktop_margin)&&o.desktop_margin<=8192&&geometry(o.top_safe_inset)&&o.top_safe_inset<=8192&&!o.viewport_id.empty()&&valid_title(o.viewport_id)&&valid_title(o.grabber_id);}
bool valid_popup_bounds(const PopoverOptions& o){
    const auto valid=[](Rect r){return std::isfinite(r.x)&&std::isfinite(r.y)&&std::abs(r.x)<=32767&&std::abs(r.y)<=32767&&geometry(r.width)&&geometry(r.height)&&r.width<=8192&&r.height<=8192;};
    if(o.preserve_input&&(o.modal||o.focus_on_open))return false;
    if(o.bounds&&(!valid(*o.bounds)||o.bounds->width<1||o.bounds->height<1||o.sheet_layout))return false;
    if(o.input_bounds){const auto r=*o.input_bounds;if(!valid(r)||r.x<0||r.y<0)return false;if(o.bounds&&(r.x+r.width>o.bounds->width+.01f||r.y+r.height>o.bounds->height+.01f))return false;}
    return true;
}
bool valid_popover_options(const PopoverOptions& o){return valid_popup_bounds(o)&&valid_sheet(o.sheet_layout)&&(!o.sheet_layout||(o.padding==0&&o.placement==PopoverPlacement::centered))&&geometry(o.width)&&o.width<=8192&&geometry(o.height)&&o.height<=8192&&geometry(o.padding)&&o.padding<=8192&&geometry(o.margins)&&geometry(o.corner_radius)&&std::isfinite(o.surface_opacity)&&o.surface_opacity>=0&&o.surface_opacity<=1&&(!o.surface_color||valid_color(*o.surface_color))&&valid_title(o.accessibility_name)&&static_cast<unsigned>(o.placement)<=1&&geometry(o.minimum_width)&&geometry(o.maximum_width)&&o.maximum_width&&o.maximum_width<=8192&&o.minimum_width<=o.maximum_width&&geometry(o.minimum_height)&&std::isfinite(o.vertical_offset)&&std::abs(o.vertical_offset)<=32767&&valid_color(o.backdrop_color)&&std::isfinite(o.corner_limit)&&o.corner_limit>=0&&o.corner_limit<=.5&&geometry(o.border_width)&&valid_color(o.border_color)&&(!o.blur_radius||(geometry(*o.blur_radius)&&*o.blur_radius<=64))&&(!o.shadow||(std::isfinite(o.shadow->offset_x)&&std::abs(o.shadow->offset_x)<=8192&&std::isfinite(o.shadow->offset_y)&&std::abs(o.shadow->offset_y)<=8192&&geometry(o.shadow->blur_radius)&&o.shadow->blur_radius<=64&&valid_color(o.shadow->color)));}
bool contains(Rect r,double x,double y) { return x>=r.x && y>=r.y && x<r.x+r.width && y<r.y+r.height; }
Rect intersect(Rect a,Rect b){const auto right=std::min(a.x+a.width,b.x+b.width),bottom=std::min(a.y+a.height,b.y+b.height);a.x=std::max(a.x,b.x);a.y=std::max(a.y,b.y);a.width=std::max(0.f,right-a.x);a.height=std::max(0.f,bottom-a.y);return a;}
bool valid_scroll(ScrollOptions o){const auto extent=[](auto v){return !v||(std::isfinite(*v)&&*v>=0&&*v<=1e15);};return geometry(o.width)&&geometry(o.height)&&o.width<=8192&&o.height<=8192&&extent(o.virtual_content_height)&&extent(o.virtual_content_width);}
bool overlaps(Rect a,Rect b) { return a.x<b.x+b.width && b.x<a.x+a.width && a.y<b.y+b.height && b.y<a.y+a.height; }
std::string trimmed(std::string_view s) { const auto a=s.find_first_not_of(" \t\r\n");return a==s.npos?std::string{}:std::string(s.substr(a,s.find_last_not_of(" \t\r\n")-a+1)); }
std::size_t previous_scalar(std::string_view s,std::size_t n) { if (n) { --n; while (n && (static_cast<unsigned char>(s[n])&0xc0)==0x80) --n; } return n; }
std::size_t next_scalar(std::string_view s,std::size_t n) { if (n<s.size()) { ++n; while (n<s.size() && (static_cast<unsigned char>(s[n])&0xc0)==0x80) ++n; } return n; }
std::size_t scalar_count(std::string_view s){return std::size_t(std::ranges::count_if(s,[](unsigned char c){return (c&0xc0)!=0x80;}));}
std::size_t scalar_prefix(std::string_view s,std::size_t limit){std::size_t n=0;while(limit-- && n<s.size())n=next_scalar(s,n);return n;}
std::size_t utf16_count(std::string_view s){std::size_t count=0;for(std::size_t i=0;i<s.size();i=next_scalar(s,i))count+=static_cast<unsigned char>(s[i])>=0xf0?2:1;return count;}
std::optional<std::size_t> utf16_byte(std::string_view s,std::size_t unit){std::size_t i=0;while(unit&&i<s.size()){const unsigned width=static_cast<unsigned char>(s[i])>=0xf0?2:1;if(unit<width)return {};unit-=width;i=next_scalar(s,i);}return unit?std::nullopt:std::optional{i};}
std::shared_ptr<const RasterImage> slider_thumb_image(){
    static const auto image=[] {std::vector<std::uint8_t> p(64*64*4,255);for(unsigned y=0;y<64;++y)for(unsigned x=0;x<64;++x)p[(y*64+x)*4+3]=std::uint8_t(std::lround(std::clamp(32.-std::hypot(x+.5-32,y+.5-32),0.,1.)*.9*255));return rgba_image(64,64,std::move(p));}();
    if(!image)throw std::runtime_error(image.error().message);return *image;
}
}
struct WidgetState::Impl final {
    bool painting=false,providing_text=false;
    struct Popup final {
        double drag_offset=0,drag_start=0,snap_from=0;
        bool dragging=false,snap_active=false;
        runtime::Clock::time_point snap_start{};
        struct Motion final {
            double progress=0,from=0,target=0;
            runtime::Clock::time_point start{};
            bool active=false;
        } motion;
        MenuOptions options;
        std::optional<PopoverOptions> popover;
        WidgetFactory content_factory;
        std::shared_ptr<WidgetState> content_view;
        std::size_t content_anchor=absent;
        MenuEvents events;
        runtime::Task canceled;
        std::vector<std::size_t> entries;
        std::size_t return_focus=absent;
        std::uint64_t epoch=0;
        bool opened=false;
        double anchor_x=0,anchor_y=0,scroll=0,content_height=0;
        std::size_t anchor_widget=absent;
        double anchor_offset_x=0,anchor_offset_y=0;
        Rect content;
        MenuDirection horizontal=MenuDirection::right,vertical=MenuDirection::down;
    };
    struct Node final {
        struct Content final {std::shared_ptr<WidgetState> view;std::uint64_t epoch=0;bool constructing=false;};
        std::unique_ptr<Content> content;
        struct Scroll final {ScrollOptions options;double offset=0,content_height=0,horizontal_offset=0,content_width=0;std::function<void(const ScrollViewport&)> changed;std::optional<ScrollViewport> delivered;bool notifying=false;};
        WidgetKind kind=WidgetKind::label;
        std::string id,text,group,marked;
        std::size_t parent=absent,cursor=0,anchor=0,default_content=absent;
        TextRange marked_selection;
        std::function<void(Rect)> layout_changed;
        std::optional<Rect> delivered_bounds;
        std::vector<std::size_t> children;
        StackOptions stack;
        LabelOptions label;
        ButtonOptions button;
        std::unique_ptr<ActionOptions> action_surface;
        InputOptions field;
        SliderOptions slider;
        ProgressOptions progress;
        ImageOptions image;
        Size extent;
        double flex=0;
        std::optional<double> layout_width;
        Size layout_measurement;
        std::optional<Rect> fixed_frame;
        bool shown=true,clip=false;
        runtime::Task escape;
        TabOptions tab;
        TabBarOptions tabbar;
        ComboBoxOptions combo;
        StepperOptions stepper;
        SegmentedOptions segments;
        ControlEvents events;
        std::unique_ptr<Popup> popup;
        std::unique_ptr<MenuItemOptions> menu_item;
        std::unique_ptr<MenuDividerOptions> divider;
        std::unique_ptr<ColorSurface> color;
        std::unique_ptr<ColorButton> color_button;
        std::unique_ptr<Scroll> scroll;
        std::optional<WidgetDrawing> drawing;
        WidgetPaintHandler paint_handler;std::size_t paint_source=absent;
        WidgetFrameHandler frame_handler;bool frame_active=false;std::uint64_t frame_revision=0;
        WidgetInputHandler input_handler;
        std::shared_ptr<const TextInputProvider> text_input;
        int menu_index=-1;
        std::size_t context_menu=absent;
        std::size_t activation_owner=absent;
        bool enabled=true,checked=false,metrics_dirty=true;
        Size measured;
        Rect bounds;
        runtime::Task action;
        std::function<void(bool)> toggled;
        std::function<void(std::string)> edited;
        std::function<void(double)> moved;
        std::function<void(int)> selection_changed;
        std::function<void(int)> stepped;
    };
    static Insets padding(const Node& n){if(n.kind==WidgetKind::segmented_control)return {n.segments.horizontal_padding,n.segments.vertical_padding,n.segments.horizontal_padding,n.segments.vertical_padding};return n.stack.insets.value_or(Insets{n.stack.padding,n.stack.padding,n.stack.padding,n.stack.padding});}
    static Insets implicit_padding(const Node& n){return n.stack.implicit_insets.value_or(padding(n));}
    static void configure_popup(Popup& p,PopoverOptions o){p.popover=o;p.options.left_padding=p.options.right_padding=o.padding;p.options.top_padding=p.options.bottom_padding=o.padding;p.options.edge_margin=o.margins;p.options.corner_radius=o.corner_radius;p.options.opacity=o.surface_opacity;p.options.background=o.surface_color.value_or(WidgetColor{20./255,20./255,20./255,1});p.options.dismiss_on_outside_press=p.options.dismiss_on_context_request=o.dismiss_on_outside_press;}
    const std::thread::id owner=std::this_thread::get_id();
    bool active=true,building=true,layout_dirty=true;
    struct Domain final {std::size_t nodes=0,allowance=0;std::weak_ptr<WidgetState> staged;};
    std::shared_ptr<Domain> domain=std::make_shared<Domain>();
    bool content_transition=false;
    std::weak_ptr<WidgetState> embedding_owner;
    std::size_t embedding_popup=absent,node_capacity=4096;
    std::string scope_prefix;
    Size viewport{640,420};
    std::vector<Node> nodes;
    mutable std::vector<std::size_t> host_order;
    mutable bool host_order_dirty=true;
    std::map<std::string,std::size_t,std::less<>> ids;
    std::vector<std::shared_ptr<void>> owners;
    std::function<void(runtime::Task)> invoke;
    WidgetAppearance appearance;
    std::string resolved_font_family;
    std::string resolved_compact_font_family;
    std::unique_ptr<WidgetSurface> surface;
    std::size_t hovered=absent,pressed=absent,focused=absent;
    std::uint64_t focus_epoch=0;
    std::vector<std::size_t> popups;
    std::vector<std::size_t> visual_popups;
    WidgetFrameHooks frame_hooks;
    std::vector<std::size_t> frame_nodes;
    runtime::FrameLoop* motion_frames=nullptr;
    std::optional<runtime::TickToken> motion_tick;
    runtime::Clock::time_point motion_now{};
    bool motion_active=false;
    struct MotionReceiver final {std::weak_ptr<MotionPreferences> model;std::shared_ptr<MotionChangePort> port;};
    std::vector<MotionReceiver> motion_receivers;
    bool consume_release=false;
    std::size_t input_capture=absent;
    std::vector<std::size_t> input_hover_path;
    struct InteractionWatch final {std::size_t node;std::weak_ptr<const WidgetInteractionPort> port;std::optional<WidgetInteraction> delivered;};
    std::vector<std::shared_ptr<InteractionWatch>> interaction_watches;
    bool notifying_interactions=false,interactions_dirty=true;
    bool notifying_viewports=false;
    bool fill_viewport=false;
    std::uint64_t layout_revision=0,delivered_layout_revision=std::numeric_limits<std::uint64_t>::max();
    runtime::Task cancel_input_capture(){
        const auto captured=std::exchange(input_capture,absent);
        if(captured==absent||captured>=nodes.size()||!nodes[captured].input_handler)return {};
        auto handler=nodes[captured].input_handler;auto target=nodes[captured].id;
        return [handler=std::move(handler),target=std::move(target)]{auto r=handler({.kind=WidgetInputKind::focus_lost,.target=target});if(!r)throw std::runtime_error(r.error().message);};
    }
    bool& domain_transition() {if(auto parent=embedding_owner.lock())return parent->impl_->domain_transition();return content_transition;}

    WidgetSurface* presentation_surface() const {
        if(!active)return nullptr;if(surface)return surface.get();
        if(auto parent=embedding_owner.lock())return parent->impl_->presentation_surface();return nullptr;
    }
    void accessibility_changed() const {if(auto s=presentation_surface())s->accessibility_changed();}
    void accessibility_value_changed(std::string_view id) const {if(auto s=presentation_surface())s->accessibility_value_changed(scope_prefix+std::string(id));}
    std::size_t active_nodes() const {auto count=nodes.size();for(const auto& n:nodes)if(auto child=n.content?n.content->view:n.popup?n.popup->content_view:nullptr)count+=child->impl_->active_nodes();return count;}
    std::size_t active_popups() const {auto count=popups.size();for(const auto& n:nodes)if(auto child=n.content?n.content->view:n.popup?n.popup->content_view:nullptr)count+=child->impl_->active_popups();return count;}
    std::size_t active_visual_popups() const {auto count=visual_popups.size();for(const auto& n:nodes)if(auto child=n.content?n.content->view:n.popup?n.popup->content_view:nullptr)count+=child->impl_->active_visual_popups();return count;}
    std::shared_ptr<WidgetState> embedded_view(std::size_t i) const {
        if(i==absent)return {};const auto& n=nodes[i];return n.content?n.content->view:n.popup?n.popup->content_view:n.activation_owner!=absent?nodes[n.activation_owner].popup->content_view:std::shared_ptr<WidgetState>{};
    }
    bool focusable_content(std::size_t i)const {auto child=embedded_view(i);return child&&child->impl_->next_content(absent,absent,1)!=absent;}
    const std::vector<std::size_t>& ordered_hosts()const{
        if(!host_order_dirty)return host_order;host_order.clear();
        const auto visit=[&](auto&& self,std::size_t i)->void{if(nodes[i].content)host_order.push_back(i);for(auto child:nodes[i].children)self(self,child);};
        if(!nodes.empty())visit(visit,0);host_order_dirty=false;return host_order;
    }
    std::size_t input_popup()const{for(auto it=popups.rbegin();it!=popups.rend();++it)if(!nodes[*it].popup->popover||!nodes[*it].popup->popover->preserve_input)return *it;return absent;}
    std::size_t modal_host() const {
        const auto scope=input_popup();const auto& hosts=ordered_hosts();
        for(auto it=hosts.rbegin();it!=hosts.rend();++it){const auto& n=nodes[*it];if(n.enabled&&logical_visible(*it)&&owning_popup(*it)==scope&&n.content->view&&n.content->view->impl_->modal_content())return *it;}
        return absent;
    }
    bool modal_content() const {
        if(const auto top=input_popup();top!=absent){const auto& popup=*nodes[top].popup;if(!popup.popover||popup.popover->modal||(popup.content_view&&popup.content_view->impl_->modal_content()))return true;}
        return modal_host()!=absent;
    }
    bool captures_input()const{
        if(pressed!=absent||input_capture!=absent)return true;
        for(auto i:ordered_hosts())if(auto child=nodes[i].content->view;child&&child->impl_->captures_input())return true;
        for(auto i:popups)if(auto child=nodes[i].popup->content_view;child&&child->impl_->captures_input())return true;return false;
    }
    std::size_t content_focus(std::size_t popup)const {if(nodes[popup].content)return popup;const auto anchor=nodes[popup].popup->content_anchor;return anchor==absent?popup:anchor;}
    Rect activation_rect(std::size_t popup)const {
        if(nodes[popup].content)return nodes[popup].bounds;
        const auto& p=*nodes[popup].popup;
        if(p.content_anchor==absent)return {p.content.x,p.content.y-float(p.scroll),p.content.width,float(std::max(double(p.content.height),p.content_height))};
        const auto& anchor=nodes[p.content_anchor];const auto pad=padding(anchor);const auto r=anchor.bounds;
        return {r.x+float(pad.left),r.y+float(pad.top),std::max(0.f,r.width-float(pad.left+pad.right)),std::max(0.f,r.height-float(pad.top+pad.bottom))};
    }
    static bool visual(const Popup& p){return p.opened||(p.motion.active&&p.motion.progress>.001);}
    void motion_activity() {
        const bool next=std::ranges::any_of(visual_popups,[&](auto i){return nodes[i].popup->motion.active||nodes[i].popup->snap_active;})||!frame_nodes.empty();
        if(next==motion_active)return;
        if(motion_tick){auto result=motion_frames->set_enabled(*motion_tick,next);if(!result)throw std::runtime_error(result.error().message);}
        motion_active=next;if(frame_hooks.activity)frame_hooks.activity(next);
    }
    runtime::Clock::duration motion_duration(const Popup& p) const {
        if(!p.popover||!p.popover->transition||!p.popover->transition->enabled)return {};
        const auto& t=*p.popover->transition;auto result=t.preferences->duration(p.opened?t.enter_duration:t.exit_duration);
        if(!result)throw std::runtime_error(result.error().message);return *result;
    }
    void advance_motion(runtime::Clock::time_point now,MotionPreferences* changed=nullptr) {
        motion_now=std::max(motion_now,now);bool dirty=false;
        for(auto i:visual_popups){auto& p=*nodes[i].popup;auto& m=p.motion;
            if(changed&&(!p.popover->transition||p.popover->transition->preferences.get()!=changed))continue;
            if(m.active){const auto duration=motion_duration(p);const auto elapsed=motion_now-m.start;const bool done=duration<=runtime::Clock::duration::zero()||elapsed>=duration;const double t=done?1.:std::max(0.,std::chrono::duration<double>(elapsed).count()/std::chrono::duration<double>(duration).count());const auto& o=*p.popover->transition;const auto value=done?m.target:m.from+(m.target-m.from)*animation::ease(p.opened?o.enter_easing:o.exit_easing,t,o.overshoot);dirty|=value!=m.progress;m.progress=value;if(done)m.active=false;}
            if(p.snap_active){const auto duration=motion_duration(p);const auto elapsed=motion_now-p.snap_start;const bool done=duration<=runtime::Clock::duration::zero()||elapsed>=duration;const auto t=done?1.:std::clamp(std::chrono::duration<double>(elapsed).count()/std::chrono::duration<double>(duration).count(),0.,1.);p.drag_offset=done?0:p.snap_from*(1-animation::ease(animation::Easing::out_back,t,p.popover->transition->overshoot));p.snap_active=!done;dirty=true;}
        }
        std::erase_if(visual_popups,[&](auto i){return !nodes[i].popup->opened&&!nodes[i].popup->motion.active;});
        if(dirty)invalidate();
        if(!changed){const auto pending=frame_nodes;for(auto i:pending){if(!active)return;if(i>=nodes.size()||!nodes[i].frame_active||!nodes[i].frame_handler)continue;
            auto callback=nodes[i].frame_handler;const auto revision=nodes[i].frame_revision;core::Result<bool> keep=false;bool completed=false;auto barrier=invoke;
            try{barrier([&]{try{keep=callback(now);}catch(...){keep=core::fail(core::ErrorCode::failed,"Widget frame callback failed",std::current_exception());}completed=true;});}
            catch(...){if(active&&i<nodes.size()&&nodes[i].frame_revision==revision){nodes[i].frame_active=false;std::erase(frame_nodes,i);}if(active)motion_activity();throw;}
            if(!active)return;if(nodes[i].frame_revision==revision)nodes[i].frame_active=completed&&keep&&*keep;if(!nodes[i].frame_active)std::erase(frame_nodes,i);
            if(!completed||!keep){motion_activity();if(!completed)throw std::runtime_error("Widget frame callback was not admitted");if(keep.error().cause)std::rethrow_exception(keep.error().cause);throw std::runtime_error(keep.error().message);}
        }}
        motion_activity();
    }
    void begin_motion(Popup& p,bool animated=true) {
        p.dragging=false;p.snap_active=false;if(p.opened)p.drag_offset=0;
        auto& m=p.motion;m.from=m.progress;m.target=p.opened?1.:0.;m.start=std::max(runtime::Clock::now(),motion_now);
        m.active=animated&&m.from!=m.target&&motion_duration(p)>runtime::Clock::duration::zero();
        if(!m.active)m.progress=m.target;
    }
    ContentTransform transform(std::size_t popup) const {
        const auto& n=nodes[popup];const auto& p=*n.popup;if(!p.popover)return {};
        const auto progress=p.motion.progress;
        if(p.popover->sheet_layout&&p.popover->sheet_layout->mobile)return {.offset_y=float(n.bounds.height*(1-progress)+p.drag_offset)};
        if(!p.popover->transition)return {};
        return {.origin_x=float(n.bounds.x+n.bounds.width*p.popover->transition->origin.width),.origin_y=float(n.bounds.y+n.bounds.height*p.popover->transition->origin.height),
            .scale=float(p.popover->transition->collapsed_scale+(1-p.popover->transition->collapsed_scale)*progress),.opacity=float(std::clamp(progress,0.,1.))};
    }
    Rect visual_bounds(std::size_t i) const {
        const auto popup=owning_popup(i);return popup==absent?nodes[i].bounds:transform_rect(nodes[i].bounds,transform(popup));
    }
    Size logical_point(std::size_t i,double x,double y) const {
        const auto popup=owning_popup(i);if(popup==absent)return {x,y};const auto t=transform(popup);
        return {t.origin_x+(x-t.offset_x-t.origin_x)/t.scale,t.origin_y+(y-t.offset_y-t.origin_y)/t.scale};
    }
    Rect popup_viewport(Size size) const {
        if(auto parent=embedding_owner.lock();parent&&embedding_popup<parent->impl_->nodes.size()){
            auto offset=parent->global_offset();const auto body=parent->impl_->activation_rect(embedding_popup);auto window=presentation_surface()?presentation_surface()->size():size;
            return {-float(offset.width+body.x),-float(offset.height+body.y),float(window.width),float(window.height)};
        }return {0,0,float(size.width),float(size.height)};
    }

    runtime::Task pointer_focus(std::size_t next){if(next!=absent&&nodes[next].action_surface&&!nodes[next].action_surface->pointer_focusable)return {};return change_focus(next);}
    runtime::Task change_focus(std::size_t next) {
        if(focused==next) return {};
        const auto epoch=++focus_epoch;
        auto embedded=embedded_view(focused);
        auto old_provider=focused==absent?nullptr:nodes[focused].text_input;
        auto new_provider=next==absent?nullptr:nodes[next].text_input;
        auto callback=focused!=absent && nodes[focused].kind==WidgetKind::input_field &&
            nodes[focused].field.commit_on_focus_loss && !nodes[focused].field.read_only?nodes[focused].action:runtime::Task{};
        if(focused!=absent) nodes[focused].marked.clear();
        focused=next;
        if(embedded||old_provider||new_provider)return [this,next,epoch,embedded,old_provider=std::move(old_provider),new_provider=std::move(new_provider),callback=std::move(callback)]{
            if(embedded){auto r=embedded->input({.kind=WidgetInputKind::focus_lost});if(!r&&r.error().code!=core::ErrorCode::closed)throw std::runtime_error(r.error().message);}
            if(old_provider&&old_provider->focus_changed){auto r=old_provider->focus_changed(false);if(!r&&r.error().code!=core::ErrorCode::closed)throw std::runtime_error(r.error().message);}
            if(!active||focus_epoch!=epoch||focused!=next)return;
            if(callback)callback();
            if(!active||focus_epoch!=epoch||focused!=next)return;
            if(new_provider&&new_provider->focus_changed){auto r=new_provider->focus_changed(true);if(!r&&r.error().code!=core::ErrorCode::closed)throw std::runtime_error(r.error().message);}
        };
        return callback;
    }

    std::size_t owning_popup(std::size_t i) const {
        for(auto p=i;p!=absent;p=nodes[p].parent) if(nodes[p].popup) return p;
        return absent;
    }
    bool descendant(std::size_t i,std::size_t root) const {
        for(auto p=i;p!=absent;p=nodes[p].parent) if(p==root) return true;
        return false;
    }
    bool effective_enabled(const Node& n)const{
        if(!n.enabled)return false;for(auto p=n.parent;p!=absent;p=nodes[p].parent)if(!nodes[p].enabled)return false;
        if(auto parent=embedding_owner.lock())return parent->impl_->active&&parent->impl_->effective_enabled(embedding_popup);return true;
    }
    bool effective_enabled(std::size_t i)const{return effective_enabled(nodes[i]);}
    bool logical_visible(std::size_t i) const {
        if(auto parent=embedding_owner.lock();parent && (!parent->impl_->active || embedding_popup>=parent->impl_->nodes.size() || (parent->impl_->nodes[embedding_popup].popup&&!parent->impl_->nodes[embedding_popup].popup->opened) || !parent->impl_->logical_visible(parent->impl_->content_focus(embedding_popup))))return false;
        for(auto p=i;p!=absent;p=nodes[p].parent) if(!nodes[p].shown || (nodes[p].popup && !nodes[p].popup->opened)) return false;
        return true;
    }
    bool visible(std::size_t i) const {
        if(owning_popup(i)!=absent)if(auto parent=embedding_owner.lock();parent&&!parent->impl_->effective_enabled(embedding_popup))return false;
        if(auto parent=embedding_owner.lock();parent&&(!parent->impl_->active||!parent->impl_->visible(parent->impl_->content_focus(embedding_popup))))return false;
        for(auto p=i;p!=absent;p=nodes[p].parent)if(!nodes[p].shown||(nodes[p].popup&&!visual(*nodes[p].popup)))return false;
        if(auto clip=clip_rect(i);clip&&!overlaps(nodes[i].bounds,*clip))return false;
        const auto popup=owning_popup(i);
        return popup==absent || popup==i || (nodes[popup].popup->popover && !nodes[popup].popup->popover->clip_content) || overlaps(nodes[i].bounds,nodes[popup].popup->content);
    }
    bool blocked(std::size_t i) const {
        if(!logical_visible(i)||!effective_enabled(i))return true;
        if(auto parent=embedding_owner.lock();parent && (!parent->impl_->active || embedding_popup>=parent->impl_->nodes.size() || !parent->impl_->nodes[embedding_popup].enabled || parent->impl_->blocked(embedding_popup)))return true;
        const auto own=owning_popup(i);if(own!=absent&&nodes[own].popup->popover&&nodes[own].popup->popover->preserve_input)return false;
        if(auto host=modal_host();host!=absent&&host!=i)return true;
        const auto top=input_popup();if(top==absent)return false;
        if(auto child=nodes[top].popup->content_view;child&&i!=top&&i!=content_focus(top)&&child->impl_->modal_content())return true;
        return (!nodes[top].popup->popover || nodes[top].popup->popover->modal) && owning_popup(i)!=top;
    }
    std::optional<Rect> clip_rect(std::size_t i)const{
        std::optional<Rect> result;const auto popup=owning_popup(i);
        if(popup==absent&&fill_viewport)if(auto parent=embedding_owner.lock()){
            const auto index=embedding_popup;const auto body=parent->impl_->activation_rect(index);auto clipped=body;
            if(auto inherited=parent->impl_->clip_rect(index))clipped=intersect(clipped,*inherited);
            clipped.x-=body.x;clipped.y-=body.y;result=clipped;
        }
        if(nodes[i].drawing)result=result?intersect(*result,nodes[i].bounds):nodes[i].bounds;
        if(popup!=absent&&popup!=i&&(!nodes[popup].popup->popover||nodes[popup].popup->popover->clip_content))result=result?intersect(*result,nodes[popup].popup->content):nodes[popup].popup->content;
        if(i!=popup)for(auto p=nodes[i].parent;p!=absent&&p!=popup;p=nodes[p].parent)if(nodes[p].scroll||nodes[p].clip)result=result?intersect(*result,nodes[p].bounds):nodes[p].bounds;
        return result;
    }
    std::string shortcut(const Node& n) const {
        if(!n.menu_item || !n.menu_item->key_visible) return {};
        auto s=trimmed(n.menu_item->shortcut.value_or(""));return s.empty()?n.menu_item->key_placeholder:s;
    }
    bool item_selected(std::size_t i) const {
        const auto& n=nodes[i];if(!n.menu_item) return false;
        return n.menu_item->state==MenuItemState::selected || (n.menu_item->state==MenuItemState::normal && n.parent!=absent && nodes[n.parent].popup && nodes[n.parent].popup->options.selected_index==n.menu_index);
    }
    void reveal(std::size_t i) {
        const auto owner=owning_popup(i);
        if(i!=owner)for(auto parent=nodes[i].parent;parent!=absent&&parent!=owner;parent=nodes[parent].parent)if(nodes[parent].scroll){
            auto& view=*nodes[parent].scroll;const auto r=nodes[i].bounds,v=nodes[parent].bounds;auto offset=view.offset,x=view.horizontal_offset;
            if(!view.options.virtual_content_height){if(r.y<v.y)offset-=v.y-r.y;else if(r.y+r.height>v.y+v.height)offset+=r.y+r.height-v.y-v.height;}
            if(!view.options.virtual_content_width){if(r.x<v.x)x-=v.x-r.x;else if(r.x+r.width>v.x+v.width)x+=r.x+r.width-v.x-v.width;}
            offset=std::clamp(offset,0.,std::max(0.,view.content_height-v.height));
            x=std::clamp(x,0.,std::max(0.,view.content_width-v.width));
            if(offset!=view.offset||x!=view.horizontal_offset){view.offset=offset;view.horizontal_offset=x;invalidate(true);layout(viewport);}
        }
        const auto p=owning_popup(i);if(p==absent || p==i) return;
        auto& popup=*nodes[p].popup;const auto row=nodes[i].bounds;
        if(popup.popover && !popup.popover->clip_content)return;
        auto offset=popup.scroll;
        if(row.y<popup.content.y) offset-=popup.content.y-row.y;
        else if(row.y+row.height>popup.content.y+popup.content.height) offset+=row.y+row.height-popup.content.y-popup.content.height;
        offset=std::clamp(offset,0.0,std::max(0.0,popup.content_height-popup.content.height));
        if(offset!=popup.scroll) {popup.scroll=offset;invalidate(true);}
    }
    std::size_t next_entry(std::size_t popup,std::size_t from,int direction,bool edge=false) const {
        const auto& entries=nodes[popup].popup->entries;if(entries.empty()) return absent;
        const auto count=int(entries.size());int start=direction>0?-1:count;
        if(!edge && from!=absent) start=nodes[from].menu_index;
        for(int n=1;n<=count;++n) {
            const auto entry=entries[std::size_t((start+n*direction+count*2)%count)];
            if(nodes[entry].kind==WidgetKind::menu_item && nodes[entry].enabled && logical_visible(entry)) return entry;
        }
        return absent;
    }
    std::size_t next_content(std::size_t popup,std::size_t from,int direction) const {
        const auto count=nodes.size();auto current=from;
        for(std::size_t n=0;n<count;++n){current=current==absent?(direction>0?0:count-1):(current+count+(direction>0?1:-1))%count;
            if(nodes[current].action_surface&&!nodes[current].action_surface->tab_focusable)continue;
            if((interactive(nodes[current].kind)||nodes[current].input_handler||((nodes[current].activation_owner!=absent||nodes[current].content)&&focusable_content(current)))&&nodes[current].enabled&&logical_visible(current)&&owning_popup(current)==popup)return current;}
        return popup;
    }
    std::vector<runtime::Task> end_popup(std::size_t root,bool restore=true,bool canceled=false) {
        std::vector<runtime::Task> callbacks;const auto previous=nodes[root].popup->return_focus;
        if(input_capture!=absent&&descendant(input_capture,root)){auto cancel=cancel_input_capture();if(cancel)callbacks.push_back(std::move(cancel));}
        const bool owned_focus=focused==absent || descendant(focused,root);
        if(owned_focus){const bool embedded=bool(embedded_view(focused));auto blur=change_focus(absent);if(blur&&!embedded)callbacks.push_back(std::move(blur));}
        for(auto it=popups.rbegin();it!=popups.rend();++it) if(descendant(*it,root)) {
            auto& popup=*nodes[*it].popup;popup.opened=false;++popup.epoch;begin_motion(popup,*it==root&&restore);
            if(popup.content_anchor!=absent){nodes[popup.content_anchor].shown=false;invalidate(true,popup.content_anchor);}
            if(popup.content_view){
                auto view=std::move(popup.content_view);auto owner=view->impl_->embedding_owner;
                callbacks.push_back([view=std::move(view),owner]{
                    auto parent=owner.lock();if(!parent||!parent->impl_->active){view->close();return;}
                    auto& transition=parent->impl_->domain_transition();const bool previous=transition;transition=true;
                    try {auto opened=view->impl_->popups;for(auto it=opened.rbegin();it!=opened.rend();++it){if(!view->ready())break;auto r=view->close_popup(*it);if(!r&&r.error().code!=core::ErrorCode::closed)throw std::runtime_error(r.error().message);}if(view->ready()){auto r=view->input({.kind=WidgetInputKind::focus_lost});if(!r&&r.error().code!=core::ErrorCode::closed)throw std::runtime_error(r.error().message);}view->close();}
                    catch(...){view->close();transition=previous;throw;}
                    transition=previous;
                });
            }
            if(popup.events.closed) callbacks.push_back(popup.events.closed);
            if(*it==root&&canceled&&popup.canceled)callbacks.push_back(popup.canceled);
        }
        std::erase_if(popups,[&](auto p){return descendant(p,root);});
        std::erase_if(visual_popups,[&](auto i){return !nodes[i].popup->opened&&!nodes[i].popup->motion.active;});motion_activity();
        const bool preserve=nodes[root].popup->popover&&nodes[root].popup->popover->preserve_input;
        if(!preserve||(hovered!=absent&&descendant(hovered,root)))hovered=absent;
        if(!preserve||(pressed!=absent&&descendant(pressed,root)))pressed=absent;
        if(owned_focus&&restore&&previous!=absent&&nodes[previous].enabled&&logical_visible(previous)&&!blocked(previous))focused=previous;
        invalidate(true);accessibility_changed();return callbacks;
    }

    void select(std::size_t index,int current) {
        auto& bar=nodes[index];bar.tabbar.current_index=current;
        for(std::size_t i=0;i<bar.children.size();++i) nodes[bar.children[i]].tab.selected=int(i)==current;
        invalidate();
    }
    void reconcile(std::size_t index) {
        auto& bar=nodes[index];if(!bar.tabbar.auto_select) return;
        auto current=bar.tabbar.current_index;
        if(current<0 || std::size_t(current)>=bar.children.size() || !nodes[bar.children[std::size_t(current)]].enabled) {
            current=-1;for(std::size_t i=0;i<bar.children.size();++i) if(nodes[bar.children[i]].enabled) { current=int(i);break; }
        }
        select(index,current);
    }

    void invalidate(bool layout=false,std::size_t node=absent) {
        interactions_dirty=true;
        if (layout) {
            layout_dirty=true;
            while (node!=absent) { nodes[node].metrics_dirty=true;nodes[node].layout_width.reset(); node=nodes[node].parent; }
        }
        if (surface) surface->invalidate();
        else if(auto parent=embedding_owner.lock();parent&&parent->impl_->active)parent->impl_->invalidate(layout,parent->impl_->content_focus(embedding_popup));
    }
    bool custom_action_content(const Node& n)const{return std::ranges::any_of(n.children,[&](auto child){return !nodes[child].popup;});}
    WidgetColor action_ink(const Node& n)const{
        const auto& o=*n.action_surface;const auto i=std::size_t(&n-nodes.data());const auto base=o.text_color.value_or(appearance.primary);
        if(!effective_enabled(n))return o.disabled_text_color.value_or(WidgetColor{1,1,1,appearance.text_alpha[7]});
        if(pressed==i)return o.pressed_text_color.value_or(WidgetColor{base.red/1.2,base.green/1.2,base.blue/1.2,base.alpha});
        if(hovered!=absent&&descendant(hovered,i))return o.hover_text_color.value_or(WidgetColor{1,1,1,.9});return base;
    }
    TextRun text_run(const Node& n) const {
        const bool label=n.kind==WidgetKind::label,enabled=effective_enabled(n);
        const bool compact=n.menu_item && n.menu_item->compact.value_or(false);
        const auto role=n.action_surface?n.action_surface->text_role:label?n.label.role:n.kind==WidgetKind::button&&n.button.style&&n.button.style->text_role?*n.button.style->text_role:TextRole::body;
        const auto i=static_cast<unsigned>(role);
        auto run=TextRun{.bounds=n.bounds,.text=n.text,.color=color(label&&n.label.color&&enabled?*n.label.color:WidgetColor{1,1,1,enabled?appearance.text_alpha[i]:appearance.text_alpha[7]}),
            .font_size=float(compact?12:label?n.label.font_size:13),
            .font_weight=compact?400u:role==TextRole::title || role==TextRole::title2 ? 700u : role==TextRole::header || role==TextRole::header2 || role==TextRole::description ? 600u : role==TextRole::body ? 500u : 400u,
            .alignment=label?n.label.alignment:TextAlignment::left,.font_family=compact?"Inter":appearance.styled?appearance.font_family:std::string{},.line_height=label?float(n.label.line_height):0};
        if(label&&n.label.font_weight)run.font_weight=*n.label.font_weight;if(label)run.elide_right=n.label.elide_right;if(n.kind==WidgetKind::button&&n.button.style){run.line_height=float(n.button.style->line_height);if(n.button.style->font_size)run.font_size=float(*n.button.style->font_size);}
        if(n.action_surface){run.color=color(action_ink(n));run.font_size=float(n.action_surface->font_size);run.line_height=float(n.action_surface->line_height);run.underline=n.action_surface->underline;run.elide_right=true;}
        if(n.kind==WidgetKind::input_field&&n.field.style){const auto& s=*n.field.style;run.font_size=float(s.font_size);run.line_height=float(s.line_height);if(s.text)run.color=color(*s.text);}return run;
    }
    Size measure(std::size_t index) {
        auto& n=nodes[index]; if (!n.metrics_dirty) return n.measured;
        Size s;
        if(n.action_surface){
            const auto& o=*n.action_surface;auto run=text_run(n);run.bounds={0,0,8192,8192};s={measure_text_width(run),measure_text(run)};
            double left=0,top=0,right=0,bottom=0;bool first=true;
            for(auto child:n.children)if(!nodes[child].popup){auto c=measure(child);auto r=nodes[child].fixed_frame.value_or(Rect{0,0,float(c.width),float(c.height)});if(first){left=r.x;top=r.y;right=r.x+r.width;bottom=r.y+r.height;first=false;}else{left=std::min(left,double(r.x));top=std::min(top,double(r.y));right=std::max(right,double(r.x+r.width));bottom=std::max(bottom,double(r.y+r.height));}}
            s.width=std::max(o.minimum_width,std::max(s.width,right-left)+o.padding.left+o.padding.right);s.height=std::max(o.minimum_height,std::max(s.height,bottom-top)+o.padding.top+o.padding.bottom);
            if(o.width)s.width=*o.width;if(o.height)s.height=*o.height;
        }
        else if(n.content)s=n.extent;
        else if(n.activation_owner!=absent){auto view=embedded_view(index);if(view&&!view->impl_->nodes.empty()){s=view->impl_->measure(0);const auto p=padding(n);s.width+=p.left+p.right;s.height+=p.top+p.bottom;}}
        else if(n.fixed_frame) s={n.fixed_frame->width,n.fixed_frame->height};
        else if(n.kind==WidgetKind::absolute_stack) s=n.extent;
        else if(n.kind==WidgetKind::image) s={n.image.width,n.image.height};
        else if(n.scroll){for(auto child:n.children)if(!nodes[child].popup&&nodes[child].shown){s=measure(child);break;}if(n.scroll->options.width)s.width=n.scroll->options.width;if(n.scroll->options.height)s.height=n.scroll->options.height;}
        else if (stack_kind(n.kind)) {
            std::size_t count=0;
            for (auto child:n.children) {
                if(nodes[child].popup || (!nodes[child].shown&&!n.stack.measure_hidden_children)) continue;
                ++count;
                auto c=measure(child);
                if (n.kind==WidgetKind::vertical_stack) { s.width=std::max(s.width,c.width); s.height+=c.height; }
                else if (horizontal_kind(n.kind)) { s.width+=c.width; s.height=std::max(s.height,c.height); }
                else { s.width=std::max(s.width,c.width); s.height=std::max(s.height,c.height); }
            }
            const auto gaps=count?count-1:0;
            if (n.kind==WidgetKind::vertical_stack) s.height+=n.stack.spacing*gaps;
            else if (horizontal_kind(n.kind)) s.width+=n.stack.spacing*gaps;
            if(n.kind==WidgetKind::tab_bar && count) {
                double widest=n.tabbar.minimum_tab_width;for(auto child:n.children) if(!nodes[child].popup&&nodes[child].shown) widest=std::max(widest,measure(child).width);
                s.width=widest*count+n.stack.spacing*gaps;
            }
            const auto p=implicit_padding(n);s.width+=p.left+p.right;s.height+=p.top+p.bottom;
        } else if(n.popup && n.popup->popover) {
            const auto& o=*n.popup->popover;Size content;
            if(n.popup->content_anchor==absent&&n.popup->content_view&&!n.popup->content_view->impl_->nodes.empty())content=n.popup->content_view->impl_->measure(0);
            for(auto child:n.children)if(!nodes[child].popup&&nodes[child].shown){content=measure(child);break;}
            n.popup->content_height=content.height;
            s={o.width?o.width:content.width+2*o.padding,o.height?o.height:content.height+2*o.padding};
        } else if(n.popup) {
            double width=n.popup->options.item_width,height=0;
            std::size_t count=0;for(auto child:n.popup->entries) {if(!nodes[child].shown)continue;auto c=measure(child);width=std::max(width,c.width);height+=c.height;++count;}
            if(count)height+=n.popup->options.item_spacing*(count-1);
            n.popup->content_height=height;
            s={width+n.popup->options.left_padding+n.popup->options.right_padding,height+*n.popup->options.top_padding+*n.popup->options.bottom_padding};
        } else if(n.divider) {
            auto& d=*n.divider;
            const auto length=n.parent!=absent && nodes[n.parent].popup?nodes[n.parent].popup->options.item_width:d.line_length;
            s=d.vertical?Size{d.thickness+d.cross_padding*2,d.line_length}:Size{length,d.thickness+d.cross_padding*2};
        } else if(n.menu_item) {
            auto run=text_run(n);run.bounds={0,0,8192,8192};auto key=run;key.text=shortcut(n);key.font_size=n.menu_item->compact.value()?12:13;key.font_family=appearance.font_family;
            const auto icon=n.menu_item->show_icon_slot?n.menu_item->icon_size+8:0;
            const auto chevron=n.menu_item->show_chevron && n.menu_item->has_child_items?n.menu_item->chevron_size:0;
            const auto key_width=key.text.empty()?0:measure_text_width(key);
            const auto trailing=key_width+(key_width>0 && chevron>0?8:0)+chevron;
            s={std::max(n.menu_item->item_width,8+icon+measure_text_width(run)+(trailing>0?8+trailing:0)),n.menu_item->item_height};
        } else if (n.kind==WidgetKind::spacer) {
            const bool vertical=n.parent==absent || !horizontal_kind(nodes[n.parent].kind);
            s=vertical?Size{0,n.stack.spacing}:Size{n.stack.spacing,0};
        } else if (n.kind==WidgetKind::input_field) s={n.field.width,n.field.style?n.field.style->height:30};
        else if (n.kind==WidgetKind::slider) s={n.slider.width,n.slider.height};
        else if (n.kind==WidgetKind::progress) s={n.progress.width,n.progress.height};
        else if (n.kind==WidgetKind::combo_box) s={n.combo.width,20};
        else if (n.kind==WidgetKind::stepper) s={18,18};
        else if (n.color) s={n.color->options.width,n.color->options.height};
        else if (n.color_button) s={n.color_button->options.size,n.color_button->options.size};
        else {
            auto run=text_run(n); run.bounds={0,0,8192,8192};
            s={measure_text_width(run),measure_text(run)};
            if (n.kind==WidgetKind::button) {
                const auto& o=n.button;
                if(o.left_padding||o.right_padding||o.content_spacing||o.indicator_size){
                    const auto pad=o.horizontal_padding.value_or(o.icon_mode?2:appearance.button_padding),gap=o.content_spacing.value_or(o.icon_mode&&!n.text.empty()&&o.tone!=ButtonTone::primary?7:0);
                    if(o.icon_mode)s.width+=o.icon_size+(n.text.empty()?0:gap);
                    if(o.indicator)s.width+=o.indicator_size.value_or(16)+((o.icon_mode||!n.text.empty())?gap:0);
                    s.width=std::max(0.,s.width)+o.left_padding.value_or(pad)+o.right_padding.value_or(pad);
                }else{
                    if(o.icon_mode) { s.width+=o.icon_size+(n.text.empty()?0:(o.tone==ButtonTone::primary?0:7));s.width+=o.horizontal_padding.value_or(2)*2; }
                    else s.width+=o.horizontal_padding.value_or(appearance.button_padding)*2;
                    if(o.indicator)s.width+=16;
                }
                if(o.width)s.width=*o.width;
                s.height=n.button.height.value_or(appearance.button_height);
                if(n.button.help) s={21,21};
            }
            else if(n.kind==WidgetKind::tab) {
                s.width=std::max(n.tab.minimum_width,s.width+n.tab.icon_size+8+24);
                if(!n.tab.badge.empty()) { auto badge=text_run(n);badge.text=n.tab.badge;badge.font_size=11;s.width+=8+measure_text_width(badge); }
                s.height=32;
            }
            else if (check_kind(n.kind)) { s.width+=(n.kind==WidgetKind::toggle_switch?38:17)+(n.text.empty()?0:8); s.height=std::max(22.0,s.height); }
        }
        if(stack_kind(n.kind)&&!n.fixed_frame){s.width=std::max(s.width,n.stack.minimum_width);s.height=std::max(s.height,n.stack.minimum_height);if(n.extent.width>0)s.width=n.extent.width;if(n.extent.height>0)s.height=n.extent.height;}
        n.metrics_dirty=false; return n.measured=s;
    }
    Size measure_width(std::size_t i,double available) {
        auto& n=nodes[i];available=std::max(0.0,available);
        if(n.layout_width&&*n.layout_width==available)return n.layout_measurement;
        auto s=measure(i);
        if(n.action_surface){s.width=std::min(available,s.width);}
        else if(n.content){s.width=n.extent.width?std::min(available,n.extent.width):available;s.height=n.extent.height;}
        else if(n.activation_owner!=absent){auto view=embedded_view(i);s={};if(view&&!view->impl_->nodes.empty()){const auto p=padding(n);s=view->impl_->measure_width(0,std::max(0.0,available-p.left-p.right));s.width=std::min(available,s.width+p.left+p.right);s.height+=p.top+p.bottom;if(n.stack.alignment==StackAlignment::stretch)s.width=available;}}
        else if(n.scroll){s.width=n.scroll->options.width?std::min(available,n.scroll->options.width):available;for(auto child:n.children)if(!nodes[child].popup&&nodes[child].shown){auto c=measure_width(child,s.width);s.height=n.scroll->options.height?n.scroll->options.height:c.height;break;}}
        else if(!n.fixed_frame&&n.kind==WidgetKind::label){auto run=text_run(n);s.width=std::min(s.width,available);run.bounds.width=float(std::max(1.0,s.width));s.height=measure_text(run);}
        else if(!n.fixed_frame&&stack_kind(n.kind)&&n.kind!=WidgetKind::absolute_stack){
            const auto p=padding(n);const auto width=std::max(0.0,available-p.left-p.right);std::size_t count=0;double fixed=0,weight=0;
            for(auto child:n.children)if(!nodes[child].popup&&(nodes[child].shown||n.stack.measure_hidden_children)){++count;const auto& c=nodes[child];if(horizontal_kind(n.kind)){if(c.flex>0||c.kind==WidgetKind::spacer){weight+=c.flex>0?c.flex:1;if(c.kind==WidgetKind::spacer)fixed+=measure(child).width;}else fixed+=measure(child).width;}}
            const auto gaps=count?n.stack.spacing*(count-1):0.0;const auto remaining=std::max(0.0,width-fixed-gaps);s={};
            for(auto child:n.children)if(!nodes[child].popup&&(nodes[child].shown||n.stack.measure_hidden_children)){const auto& node=nodes[child];const bool grow=horizontal_kind(n.kind)&&(node.flex>0||node.kind==WidgetKind::spacer);const auto child_width=grow?(node.kind==WidgetKind::spacer?measure(child).width:0)+remaining*(node.flex>0?node.flex:1)/weight:horizontal_kind(n.kind)?std::min(width,measure(child).width):width;auto c=measure_width(child,child_width);if(grow)c.width=child_width;
                if(n.kind==WidgetKind::vertical_stack){s.width=std::max(s.width,c.width);s.height+=c.height;}else if(horizontal_kind(n.kind)){s.width+=c.width;s.height=std::max(s.height,c.height);}else{s.width=std::max(s.width,c.width);s.height=std::max(s.height,c.height);}}
            if(n.kind==WidgetKind::vertical_stack)s.height+=gaps;else if(horizontal_kind(n.kind))s.width+=gaps;
            const auto natural=implicit_padding(n);s.width=std::min(available,s.width+natural.left+natural.right);s.height+=natural.top+natural.bottom;if(n.stack.alignment==StackAlignment::stretch&&n.kind==WidgetKind::vertical_stack&&!n.stack.intrinsic_width)s.width=available;
        }
        if(stack_kind(n.kind)&&!n.fixed_frame){s.width=std::min(available,std::max(s.width,n.stack.minimum_width));s.height=std::max(s.height,n.stack.minimum_height);if(n.extent.width>0)s.width=std::min(available,n.extent.width);if(n.extent.height>0)s.height=n.extent.height;}
        n.layout_width=available;n.layout_measurement=s;return s;
    }
    void arrange(std::size_t i,Rect bounds) {
        auto& n=nodes[i]; n.bounds=bounds;
        if(n.action_surface){const auto& p=n.action_surface->padding;const auto width=std::max(0.,double(bounds.width)-p.left-p.right);for(auto child:n.children)if(!nodes[child].popup&&nodes[child].shown){const auto c=measure_width(child,width);auto r=nodes[child].fixed_frame.value_or(Rect{0,0,float(c.width),float(c.height)});r.x+=bounds.x+float(p.left);r.y+=bounds.y+float(p.top);arrange(child,r);}return;}
        if(n.content){if(auto child=n.content->view){child->impl_->layout_dirty=true;child->impl_->layout({bounds.width,bounds.height});}return;}
        if(n.scroll){
            auto& view=*n.scroll;const auto& options=view.options;
            const bool virtual_y=options.virtual_content_height.has_value(),virtual_x=options.virtual_content_width.has_value();
            view.content_height=options.enabled?options.virtual_content_height.value_or(0):bounds.height;
            view.content_width=options.enabled?options.virtual_content_width.value_or(bounds.width):bounds.width;
            for(auto child:n.children)if(!nodes[child].popup&&nodes[child].shown){
                const auto c=options.horizontal_enabled&&!virtual_x?measure(child):measure_width(child,bounds.width);
                view.content_height=options.enabled?options.virtual_content_height.value_or(c.height):bounds.height;
                view.content_width=options.enabled?options.virtual_content_width.value_or(options.horizontal_enabled?std::max(double(bounds.width),c.width):bounds.width):bounds.width;
                view.offset=std::clamp(view.offset,0.,std::max(0.,view.content_height-bounds.height));
                view.horizontal_offset=std::clamp(view.horizontal_offset,0.,std::max(0.,view.content_width-bounds.width));
                arrange(child,{virtual_x?bounds.x:bounds.x-float(view.horizontal_offset),virtual_y?bounds.y:bounds.y-float(view.offset),
                    virtual_x?bounds.width:float(std::max(double(bounds.width),view.content_width)),virtual_y?bounds.height:float(std::max(double(bounds.height),view.content_height))});return;
            }
            view.offset=std::clamp(view.offset,0.,std::max(0.,view.content_height-bounds.height));
            view.horizontal_offset=std::clamp(view.horizontal_offset,0.,std::max(0.,view.content_width-bounds.width));return;
        }
        if(n.kind==WidgetKind::absolute_stack) {
            for(auto child:n.children) {
                if(nodes[child].popup || !nodes[child].shown) continue;
                auto r=nodes[child].fixed_frame.value_or(Rect{0,0,float(measure(child).width),float(measure(child).height)});
                r.x+=bounds.x;r.y+=bounds.y;arrange(child,r);
            }
            return;
        }
        if (!stack_kind(n.kind)) return;
        const auto p=padding(n);auto x=bounds.x+float(p.left), y=bounds.y+float(p.top);
        const auto width=std::max(0.f,bounds.width-float(p.left+p.right)),height=std::max(0.f,bounds.height-float(p.top+p.bottom));
        if(n.kind==WidgetKind::vertical_stack&&std::ranges::any_of(n.children,[&](auto child){return nodes[child].shown&&!nodes[child].popup&&nodes[child].flex>0;})){
            std::size_t count=0;double fixed=0,weight=0;
            for(auto child:n.children)if(nodes[child].shown&&!nodes[child].popup){++count;const auto& c=nodes[child];const auto size=measure_width(child,width);if(c.flex>0)weight+=c.flex;else if(c.kind==WidgetKind::spacer){++weight;fixed+=size.height;}else fixed+=size.height;}
            const auto remaining=std::max(0.,height-fixed-(count?count-1:0)*n.stack.spacing);
            for(auto child:n.children)if(nodes[child].shown&&!nodes[child].popup){const auto& node=nodes[child];auto c=measure_width(child,width);if(node.flex>0)c.height=remaining*node.flex/weight;else if(node.kind==WidgetKind::spacer)c.height+=remaining/weight;if(n.stack.alignment==StackAlignment::stretch)c.width=width;const auto offset=n.stack.alignment==StackAlignment::leading||n.stack.alignment==StackAlignment::stretch?0:n.stack.alignment==StackAlignment::trailing?width-c.width:(width-c.width)/2;arrange(child,{x+float(offset),y,float(c.width),float(c.height)});y+=float(c.height+n.stack.spacing);}return;
        }
        if(n.layout_width||n.stack.alignment!=StackAlignment::center){
            (void)measure_width(i,bounds.width);double fixed=0,weight=0,desired=0;std::size_t count=0,fillers=0;
            for(auto child:n.children)if(!nodes[child].popup&&nodes[child].shown){++count;const auto& c=nodes[child];if(horizontal_kind(n.kind)){if(c.flex>0||c.kind==WidgetKind::spacer){weight+=c.flex>0?c.flex:1;if(c.kind==WidgetKind::spacer)fixed+=measure(child).width;}else fixed+=measure(child).width;}else {desired+=measure_width(child,width).height;if(c.kind==WidgetKind::spacer)++fillers;}}
            const auto gaps=count?n.stack.spacing*(count-1):0.0;const auto remaining=std::max(0.0,width-fixed-gaps),extra=fillers?std::max(0.0,height-desired-gaps)/fillers:0;
            for(auto child:n.children)if(!nodes[child].popup&&nodes[child].shown){const auto& node=nodes[child];const bool grow=horizontal_kind(n.kind)&&(node.flex>0||node.kind==WidgetKind::spacer);const auto child_width=grow?(node.kind==WidgetKind::spacer?measure(child).width:0)+remaining*(node.flex>0?node.flex:1)/weight:horizontal_kind(n.kind)?std::min(double(width),measure(child).width):double(width);auto c=measure_width(child,child_width);if(grow)c.width=child_width;
                if(n.kind==WidgetKind::vertical_stack){if(node.kind==WidgetKind::spacer)c.height+=extra;if(n.stack.alignment==StackAlignment::stretch)c.width=width;const auto offset=n.stack.alignment==StackAlignment::leading||n.stack.alignment==StackAlignment::stretch?0:n.stack.alignment==StackAlignment::trailing?width-c.width:(width-c.width)/2;arrange(child,{x+float(offset),y,float(c.width),float(c.height)});y+=float(c.height+n.stack.spacing);}
                else if(horizontal_kind(n.kind)){if(n.stack.alignment==StackAlignment::stretch)c.height=height;const auto offset=n.stack.alignment==StackAlignment::leading||n.stack.alignment==StackAlignment::stretch?0:n.stack.alignment==StackAlignment::trailing?height-c.height:(height-c.height)/2;arrange(child,{x,y+float(offset),float(c.width),float(c.height)});x+=float(c.width+n.stack.spacing);}
                else {if(n.stack.alignment==StackAlignment::stretch){c.width=width;c.height=height;}const bool leading=n.stack.alignment==StackAlignment::leading||n.stack.alignment==StackAlignment::stretch;arrange(child,{leading?x:x+(width-float(c.width))/2,leading?y:y+(height-float(c.height))/2,float(c.width),float(c.height)});}
            }
            return;
        }
        double desired=0; std::size_t fillers=0;
        std::size_t count=0;
        for (auto child:n.children) { if(nodes[child].popup || !nodes[child].shown) continue;++count;auto c=measure(child); desired+=n.kind==WidgetKind::vertical_stack?c.height:c.width; if (nodes[child].kind==WidgetKind::spacer) ++fillers; }
        if (count) desired+=n.stack.spacing*(count-1);
        const auto extra=fillers?std::max(0.0,(n.kind==WidgetKind::vertical_stack?height:width)-desired)/fillers:0;
        for (auto child:n.children) {
            if(nodes[child].popup || !nodes[child].shown) continue;
            auto c=measure(child); c.width=std::min(double(width),c.width);
            if (n.kind==WidgetKind::vertical_stack) {
                if (nodes[child].kind==WidgetKind::label && c.width>0) { auto run=text_run(nodes[child]); run.bounds.width=float(c.width); c.height=measure_text(run); }
                if (nodes[child].kind==WidgetKind::spacer) c.height+=extra;
                arrange(child,{x+(width-float(c.width))/2,y,float(c.width),float(c.height)}); y+=float(c.height+n.stack.spacing);
            } else if (horizontal_kind(n.kind)) {
                if(n.kind==WidgetKind::tab_bar) c.width=std::max(0.0,(width-n.stack.spacing*(count-1))/count);
                if (nodes[child].kind==WidgetKind::spacer) c.width+=extra;
                arrange(child,{x,y+(height-float(c.height))/2,float(c.width),float(c.height)}); x+=float(c.width+n.stack.spacing);
            } else arrange(child,{x+(width-float(c.width))/2,y+(height-float(c.height))/2,float(c.width),float(c.height)});
        }
    }
    bool constrained(std::size_t i) const {
        const auto& n=nodes[i];if(n.flex>0||n.stack.alignment!=StackAlignment::center||n.stack.insets||n.stack.minimum_width||n.stack.minimum_height||n.stack.implicit_insets||n.stack.intrinsic_width||n.stack.measure_hidden_children)return true;
        for(auto child:n.children)if(!nodes[child].popup&&nodes[child].shown&&constrained(child))return true;
        return false;
    }
    void layout(Size size) {
        if (viewport!=size) { viewport=size; layout_dirty=true; }
        if (!layout_dirty || nodes.empty()) return;
        if(!nodes[0].popup) {
            auto natural=nodes[0].layout_width||constrained(0)?measure_width(0,size.width):measure(0);
            arrange(0,{0,0,float(fill_viewport?size.width:nodes[0].kind==WidgetKind::absolute_stack?std::min(size.width,natural.width):size.width),float(fill_viewport?size.height:std::min(size.height,std::max(1.0,natural.height)))});
        }
        interactions_dirty=true;
        const auto area=popup_viewport(size);
        for(auto index:visual_popups) {
            auto& n=nodes[index];auto& p=*n.popup;auto natural=measure(index);const auto& o=p.options;
            if(p.popover&&p.popover->bounds){
                auto frame=*p.popover->bounds;frame.x+=area.x;frame.y+=area.y;n.bounds=p.content=frame;p.content_height=frame.height;p.scroll=0;
                for(auto child:n.children)if(!nodes[child].popup&&nodes[child].shown){arrange(child,frame);break;}
                if(p.content_view){p.content_view->impl_->layout_dirty=true;p.content_view->impl_->layout({frame.width,frame.height});}continue;
            }
            if(p.popover&&p.popover->sheet_layout){
                const auto& sheet=*p.popover->sheet_layout;const auto margin=sheet.mobile?0.:sheet.desktop_margin;
                const auto width=sheet.mobile?double(area.width):std::min(p.popover->width,std::max(0.,double(area.width)-margin*2));
                Size content;for(auto child:n.children)if(!nodes[child].popup&&nodes[child].shown){content=measure_width(child,width);break;}
                auto found=ids.find(sheet.viewport_id);const auto body=found==ids.end()?0.:nodes[found->second].layout_measurement.height;
                const auto chrome=std::max(0.,content.height-body);const auto maximum=std::max(0.,double(area.height)-(sheet.mobile?std::max(24.,sheet.top_safe_inset):margin*2));
                const auto requested=sheet.mobile?(sheet.detent==SheetDetent::large?std::round(area.height*.9):sheet.detent==SheetDetent::medium?std::round(area.height*.57):content.height):std::max(chrome,sheet.preferred_height);
                const auto height=std::min(maximum,requested);const auto x=area.x+(area.width-width)/2,y=sheet.mobile?area.y+area.height-height:area.y+(area.height-height)/2;
                n.bounds={float(x),float(y),float(width),float(height)};p.content=n.bounds;p.content_height=height;p.scroll=0;
                for(auto child:n.children)if(!nodes[child].popup&&nodes[child].shown){arrange(child,p.content);break;}continue;
            }
            if(p.anchor_widget!=absent&&logical_visible(p.anchor_widget)){const auto& anchor=nodes[p.anchor_widget].bounds;p.anchor_x=anchor.x+p.anchor_offset_x;p.anchor_y=anchor.y+p.anchor_offset_y;}
            const bool centered=p.popover&&p.popover->placement==PopoverPlacement::centered;
            const auto margin=centered?o.edge_margin:std::min(o.edge_margin,std::max(0.0,double(std::min(area.width,area.height))/2-1));
            const auto width=centered?(p.popover->fit_to_viewport?std::max(0.0,std::min({double(area.width)-margin*2,p.popover->maximum_width,std::max(p.popover->minimum_width,natural.width)})):std::min(p.popover->maximum_width,std::max(p.popover->minimum_width,std::min(natural.width,std::max(1.0,area.width-margin*2))))):std::min(natural.width,std::max(1.0,area.width-margin*2));
            double height=std::min(natural.height,std::max(1.0,area.height-margin*2));
            if(centered){Size content;const auto inner=std::max(0.0,width-o.left_padding-o.right_padding);if(p.content_anchor==absent&&p.content_view&&!p.content_view->impl_->nodes.empty())content=p.content_view->impl_->measure_width(0,inner);for(auto child:n.children)if(!nodes[child].popup&&nodes[child].shown){content=measure_width(child,inner);break;}p.content_height=content.height;height=p.popover->height?p.popover->height:std::max(p.popover->minimum_height,content.height+*o.top_padding+*o.bottom_padding);}
            const auto right=area.x+area.width-margin-p.anchor_x,left=p.anchor_x-area.x-margin,down=area.y+area.height-margin-p.anchor_y,up=p.anchor_y-area.y-margin;
            p.horizontal=right>=width?MenuDirection::right:left>=width?MenuDirection::left:left>right?MenuDirection::left:MenuDirection::right;
            p.vertical=down>=height?MenuDirection::down:up>=height?MenuDirection::up:up>down?MenuDirection::up:MenuDirection::down;
            const auto x=centered?area.x+(area.width-width)/2:std::clamp(p.anchor_x-(p.horizontal==MenuDirection::left?width:0),area.x+margin,area.x+area.width-margin-width);
            const auto y=centered?area.y+(area.height-height)/2+p.popover->vertical_offset:std::clamp(p.anchor_y-(p.vertical==MenuDirection::up?height:0),area.y+margin,area.y+area.height-margin-height);
            n.bounds={float(std::round(x)),float(std::round(y)),float(width),float(height)};
            p.content={n.bounds.x+float(o.left_padding),n.bounds.y+float(*o.top_padding),float(std::max(0.0,width-o.left_padding-o.right_padding)),float(std::max(0.0,height-*o.top_padding-*o.bottom_padding))};
            p.scroll=std::clamp(p.scroll,0.0,std::max(0.0,p.content_height-p.content.height));
            if(p.popover){for(auto child:n.children)if(!nodes[child].popup&&nodes[child].shown){const auto c=measure(child);arrange(child,{p.content.x,p.content.y-float(p.scroll),p.content.width,float(std::max(double(p.content.height),c.height))});break;}if(p.content_view){const auto body=activation_rect(index);p.content_view->impl_->layout_dirty=true;p.content_view->impl_->layout({body.width,body.height});}}
            else {double row_y=p.content.y-p.scroll;for(auto child:p.entries){if(!nodes[child].shown)continue;auto c=measure(child);nodes[child].bounds={p.content.x,float(row_y),p.content.width,float(c.height)};row_y+=c.height+o.item_spacing;}}
        }
        layout_dirty=false;
        ++layout_revision;
    }
    std::size_t hit_tree(std::size_t i,std::size_t popup,double x,double y) const {
        const auto& n=nodes[i];if((n.popup&&i!=popup)||!visible(i)||blocked(i))return absent;
        if(n.popup&&n.popup->popover&&n.popup->popover->input_bounds){auto r=*n.popup->popover->input_bounds;r.x+=n.bounds.x;r.y+=n.bounds.y;if(!contains(r,x,y))return absent;}
        for(auto child=n.children.rbegin();child!=n.children.rend();++child)if(auto found=hit_tree(*child,popup,x,y);found!=absent)return found;
        if(n.enabled&&(interactive(n.kind)||n.input_handler||(n.content&&n.content->view))&&contains(n.bounds,x,y)){if(auto clip=clip_rect(i);!clip||contains(*clip,x,y))return i;}return absent;
    }
    bool popup_contains(std::size_t i,double x,double y)const {
        const auto& n=nodes[i];const auto point=logical_point(i,x,y);auto r=n.bounds;
        if(n.popup->popover&&n.popup->popover->input_bounds){r=*n.popup->popover->input_bounds;r.x+=n.bounds.x;r.y+=n.bounds.y;r=intersect(r,n.bounds);}
        return contains(r,point.width,point.height);
    }
    std::size_t hit(double x,double y) const {
        for(auto p=popups.rbegin();p!=popups.rend();++p){const auto local=logical_point(*p,x,y);if(auto found=hit_tree(*p,*p,local.width,local.height);found!=absent)return found;if(popup_contains(*p,x,y))return absent;}
        return nodes.empty()?absent:hit_tree(0,absent,x,y);
    }
    double slider_value(std::size_t i,double x) const {
        x=logical_point(i,x,0).width;
        const auto& n=nodes[i]; const auto& o=n.slider;
        auto value=o.minimum+std::clamp((x-n.bounds.x-o.range_inset)/std::max(1.0,double(n.bounds.width)-2*o.range_inset),0.0,1.0)*(o.maximum-o.minimum);
        if (o.step>0) value=o.minimum+std::round((value-o.minimum)/o.step)*o.step;
        return std::clamp(value,o.minimum,o.maximum);
    }
};
WidgetState::WidgetState(std::function<void(runtime::Task)> invoke,WidgetAppearance appearance,WidgetFrameHooks hooks):impl_(std::make_unique<Impl>()) {
    if (!invoke) throw std::invalid_argument("Widget tree requires a callback barrier");
    if (!appearance.styled) appearance=lvrs_appearance();
    for (auto c:{appearance.window,appearance.primary,appearance.hover,appearance.pressed,appearance.disabled_background})
        if (!valid_color(c)) throw std::invalid_argument("Invalid widget appearance color");
    if (!geometry(appearance.button_height) || !appearance.button_height || !geometry(appearance.button_padding) || !geometry(appearance.button_radius) || !valid_title(appearance.font_family))
        throw std::invalid_argument("Invalid widget appearance");
    for (auto alpha:appearance.text_alpha) if (!std::isfinite(alpha) || alpha<0 || alpha>1) throw std::invalid_argument("Invalid text alpha");
    impl_->appearance=std::move(appearance); impl_->invoke=std::move(invoke);
    impl_->frame_hooks=std::move(hooks);
}
WidgetState::~WidgetState() { close(); }
core::Result<void> WidgetState::prepare_motion(PopoverOptions& options) {
    if(!options.transition)return {};
    auto& transition=*options.transition;
    if(!transition.preferences){if(!impl_->frame_hooks.preferences)impl_->frame_hooks.preferences=std::make_shared<MotionPreferences>();transition.preferences=impl_->frame_hooks.preferences;}
    if(auto result=transition.preferences->state();!result)return std::unexpected(result.error());
    std::erase_if(impl_->motion_receivers,[&](const auto& receiver){auto model=receiver.model.lock();return !model||std::ranges::none_of(impl_->nodes,[&](const auto& n){return n.popup&&n.popup->popover&&n.popup->popover->transition&&n.popup->popover->transition->preferences==model;});});
    if(std::ranges::none_of(impl_->motion_receivers,[&](const auto& receiver){return receiver.model.lock()==transition.preferences;})){
        if(impl_->motion_receivers.size()>=256)return core::fail(core::ErrorCode::full,"At most 256 motion preference models per View");
        auto port=std::make_shared<MotionChangePort>([weak=weak_from_this(),model=transition.preferences.get()](const MotionState&){if(auto owner=weak.lock();owner&&owner->impl_->active)owner->impl_->advance_motion(runtime::Clock::now(),model);});
        if(auto result=transition.preferences->observe(port);!result)return result;
        impl_->motion_receivers.push_back({transition.preferences,std::move(port)});
    }
    if(!transition.enabled||(transition.enter_duration==runtime::Clock::duration::zero()&&transition.exit_duration==runtime::Clock::duration::zero()))return {};
    return ensure_frame_tick();
}
core::Result<void> WidgetState::ensure_frame_tick(){
    if(impl_->motion_tick)return {};
    if(!impl_->frame_hooks.frames)return core::fail(core::ErrorCode::unsupported,"This native host has no display frame source");
    auto frames=impl_->frame_hooks.frames();if(!frames)return std::unexpected(frames.error());
    auto tick=(*frames)->add([weak=weak_from_this()](const runtime::FrameContext& context){if(auto owner=weak.lock();owner&&owner->impl_->active)owner->impl_->advance_motion(context.now);},
        {.phase=runtime::FramePhase::update,.clock=runtime::TickClock::unscaled,.enabled=false});
    if(!tick)return std::unexpected(tick.error());impl_->motion_frames=*frames;impl_->motion_tick=*tick;return {};
}
core::Result<std::shared_ptr<MotionPreferences>> WidgetState::motion_preferences(){if(auto r=ready();!r)return std::unexpected(r.error());if(!impl_->frame_hooks.preferences)impl_->frame_hooks.preferences=std::make_shared<MotionPreferences>();return impl_->frame_hooks.preferences;}
core::Result<std::reference_wrapper<runtime::EventLoop>> WidgetState::event_loop(){
    if(auto r=ready();!r)return std::unexpected(r.error());
    if(!impl_->frame_hooks.events)return core::fail(core::ErrorCode::unsupported,"This native host has no Core event loop");
    auto loop=impl_->frame_hooks.events();if(!loop)return std::unexpected(loop.error());
    if(!*loop)return core::fail(core::ErrorCode::closed,"The native host event loop has closed");return std::ref(**loop);
}
core::Result<void> WidgetState::text_input_provider(std::size_t i,std::shared_ptr<const TextInputProvider> provider){
    if(auto r=ready();!r)return r;
    if(i>=impl_->nodes.size())return core::fail(core::ErrorCode::not_found,"Text input owner was not found");
    if(provider&&(!provider->state||!provider->read||impl_->nodes[i].kind==WidgetKind::input_field))return core::fail(core::ErrorCode::invalid_argument,"A custom text input requires read/state providers and its own widget");
    auto previous=std::exchange(impl_->nodes[i].text_input,std::move(provider));
    if(impl_->focused==i){
        auto next=impl_->nodes[i].text_input;
        if(previous&&previous->focus_changed){auto r=previous->focus_changed(false);if(!r)return r;}
        if(impl_->active&&impl_->focused==i&&impl_->nodes[i].text_input==next&&next&&next->focus_changed){auto r=next->focus_changed(true);if(!r)return r;}
    }
    impl_->accessibility_changed();impl_->invalidate();return {};
}
core::Result<TextInputState> WidgetState::text_input_state(std::size_t i){
    if(auto r=ready();!r)return std::unexpected(r.error());
    if(i>=impl_->nodes.size())return core::fail(core::ErrorCode::not_found,"Text input owner was not found");
    const auto& n=impl_->nodes[i];
    if(!n.text_input){
        if(n.kind!=WidgetKind::input_field)return core::fail(core::ErrorCode::unsupported,"Widget has no text input provider");
        const auto first=std::min(n.anchor,n.cursor),last=std::max(n.anchor,n.cursor);
        TextInputState value{.length=utf16_count(n.text),.selection={utf16_count(std::string_view(n.text).substr(0,first)),utf16_count(std::string_view(n.text).substr(first,last-first))},
            .caret={8,std::max(0.f,n.bounds.height-22),1,16},.read_only=n.field.read_only,.multiline=n.field.multiline};
        if(!n.marked.empty()){value.marked=TextRange{value.selection.location,utf16_count(n.marked)};value.length=value.length-value.selection.length+value.marked->length;value.selection={value.marked->location+n.marked_selection.location,n.marked_selection.length};}
        return value;
    }
    auto provider=n.text_input;
    struct Guard{bool& flag;Guard(bool& f):flag(f){flag=true;}~Guard(){flag=false;}}guard{impl_->providing_text};
    try{
        auto value=provider->state();if(!value)return std::unexpected(value.error());
        const auto range=[&](TextRange r){return r.location<=value->length&&r.length<=value->length-r.location;};
        const auto c=value->caret;
        if(!range(value->selection)||(value->marked&&!range(*value->marked))||!std::isfinite(c.x)||!std::isfinite(c.y)||std::abs(c.x)>32767||std::abs(c.y)>32767||!geometry(c.width)||!geometry(c.height)||c.width>8192||c.height>8192)
            return core::fail(core::ErrorCode::invalid_argument,"Text input provider returned invalid state");
        return value;
    }catch(...){return core::fail(core::ErrorCode::failed,"Text input state provider failed",std::current_exception());}
}
core::Result<TextContent> WidgetState::text_content(std::size_t i,TextRange range){
    auto state=text_input_state(i);if(!state)return std::unexpected(state.error());
    if(range.location>state->length)return core::fail(core::ErrorCode::out_of_range,"Text service range exceeds the document");
    range.length=std::min({range.length,state->length-range.location,std::size_t(65536)});
    auto provider=impl_->nodes[i].text_input;
    struct Guard{bool& flag;Guard(bool& f):flag(f){flag=true;}~Guard(){flag=false;}}guard{impl_->providing_text};
    try{
        core::Result<TextContent> content=core::fail(core::ErrorCode::unsupported,"No text content reader");
        if(provider)content=provider->read(range);
        else {
            const auto& n=impl_->nodes[i];auto text=n.text;
            if(!n.marked.empty())text.replace(std::min(n.anchor,n.cursor),std::max(n.anchor,n.cursor)-std::min(n.anchor,n.cursor),n.marked);
            auto a=utf16_byte(text,range.location),b=utf16_byte(text,range.location+range.length);
            if(!a||!b)return core::fail(core::ErrorCode::out_of_range,"Text service range splits a scalar");
            content=TextContent{range,text.substr(*a,*b-*a)};
        }
        if(!content)return std::unexpected(content.error());
        const auto& actual=content->range;
        if(content->text.size()>4*65536||!core::valid_utf8(content->text)||actual.location<range.location||actual.location>range.location+range.length
            ||actual.length>range.location+range.length-actual.location||utf16_count(content->text)!=actual.length)
            return core::fail(core::ErrorCode::invalid_argument,"Text content provider returned an invalid bounded range");
        return content;
    }catch(...){return core::fail(core::ErrorCode::failed,"Text content provider failed",std::current_exception());}
}
core::Result<std::size_t> WidgetState::text_position(std::size_t i,double x,double y){
    auto state=text_input_state(i);if(!state)return std::unexpected(state.error());
    if(!std::isfinite(x)||!std::isfinite(y))return core::fail(core::ErrorCode::invalid_argument,"Text service point must be finite");
    auto provider=impl_->nodes[i].text_input;
    if(!provider||!provider->position)return state->selection.location;
    auto offset=global_offset();auto point=impl_->logical_point(i,x-offset.width,y-offset.height);const auto bounds=impl_->nodes[i].bounds;
    struct Guard{bool& flag;Guard(bool& f):flag(f){flag=true;}~Guard(){flag=false;}}guard{impl_->providing_text};
    try{auto position=provider->position(point.width-bounds.x,point.height-bounds.y);if(!position)return std::unexpected(position.error());if(*position>state->length)return core::fail(core::ErrorCode::out_of_range,"Text input hit exceeds the document");return position;}
    catch(...){return core::fail(core::ErrorCode::failed,"Text hit provider failed",std::current_exception());}
}
core::Result<void> WidgetState::frame_handler(std::size_t i,WidgetFrameHandler callback,bool active){
    if(auto r=ready();!r)return r;if(i>=impl_->nodes.size())return core::fail(core::ErrorCode::not_found,"Frame owner not found");
    if(callback&&active){if(auto r=ensure_frame_tick();!r)return r;}
    if(callback&&active){if(std::ranges::find(impl_->frame_nodes,i)==impl_->frame_nodes.end())impl_->frame_nodes.push_back(i);}else std::erase(impl_->frame_nodes,i);
    auto& n=impl_->nodes[i];n.frame_handler=std::move(callback);n.frame_active=bool(n.frame_handler)&&active;++n.frame_revision;impl_->motion_activity();return {};
}
core::Result<void> WidgetState::request_frames(std::size_t i){
    if(auto r=ready();!r)return r;if(i>=impl_->nodes.size()||!impl_->nodes[i].frame_handler)return core::fail(core::ErrorCode::invalid_argument,"Widget has no frame callback");
    if(auto r=ensure_frame_tick();!r)return r;
    if(!impl_->nodes[i].frame_active)impl_->frame_nodes.push_back(i);impl_->nodes[i].frame_active=true;++impl_->nodes[i].frame_revision;impl_->motion_activity();return {};
}
core::Result<void> WidgetState::ready() const {
    if (impl_->owner!=std::this_thread::get_id()) return core::fail(core::ErrorCode::wrong_executor,"Widgets require the owning UI thread");
    if(impl_->painting)return core::fail(core::ErrorCode::busy,"UI access is not allowed inside a paint provider");
    if(impl_->providing_text)return core::fail(core::ErrorCode::busy,"UI access is not allowed inside a text read provider");
    if (!impl_->active) return core::fail(core::ErrorCode::closed,"Widget tree is closed");
    if(!impl_->scope_prefix.empty()){auto parent=impl_->embedding_owner.lock();if(!parent||!parent->impl_->active)return core::fail(core::ErrorCode::closed,"Content owner is closed");}
    return {};
}
core::Result<std::size_t> WidgetState::prepare(Widget parent,std::string_view id,WidgetKind kind) {
    if (auto r=ready();!r) return std::unexpected(r.error());
    if (!impl_->building) return core::fail(core::ErrorCode::closed,"Widget construction finished");
    if (impl_->nodes.size()>=impl_->node_capacity||impl_->domain->nodes>=4096+impl_->domain->allowance) return core::fail(core::ErrorCode::out_of_range,"Active widget domain limit is 4096");
    if (id.empty() || id.starts_with("@iiscope:") || !valid_title(id) || impl_->ids.contains(id)) return core::fail(core::ErrorCode::invalid_argument,"Widget id must be unique nonempty UTF-8, outside the reserved @iiscope namespace");
    if (parent.owner_==std::thread::id{}) {
        if (!impl_->nodes.empty()) return core::fail(core::ErrorCode::invalid_argument,"Widget tree requires exactly one root");
        return absent;
    }
    if (parent.state_.lock().get()!=this || parent.index_>=impl_->nodes.size())
        return core::fail(core::ErrorCode::invalid_argument,"Widget parent must belong to this generation");
    const bool popup=kind==WidgetKind::menu || kind==WidgetKind::popover;
    if(!popup)while(impl_->nodes[parent.index_].default_content!=absent)parent.index_=impl_->nodes[parent.index_].default_content;
    const auto container=impl_->nodes[parent.index_].kind;
    if(!popup && container==WidgetKind::menu && kind!=WidgetKind::menu_item && kind!=WidgetKind::menu_divider)
        return core::fail(core::ErrorCode::invalid_argument,"Menu accepts menu items and dividers only");
    if(!popup && container!=WidgetKind::menu && container!=WidgetKind::popover && container!=WidgetKind::action_surface && !stack_kind(container))
        return core::fail(core::ErrorCode::invalid_argument,"Widget parent must be a stack or menu");
    if(container==WidgetKind::tab_bar && kind!=WidgetKind::tab && !popup)
        return core::fail(core::ErrorCode::invalid_argument,"TabBar accepts Tab children only");
    if(container==WidgetKind::popover&&!popup)for(auto child:impl_->nodes[parent.index_].children)if(!impl_->nodes[child].popup)return core::fail(core::ErrorCode::invalid_argument,"Popover accepts one content view; wrap multiple views in a stack");
    if(container==WidgetKind::scroll_view&&!popup)for(auto child:impl_->nodes[parent.index_].children)if(!impl_->nodes[child].popup)return core::fail(core::ErrorCode::invalid_argument,"Scroll view accepts one content root");
    std::size_t depth=1; for(auto p=parent.index_;p!=absent;p=impl_->nodes[p].parent) if (++depth>64) return core::fail(core::ErrorCode::out_of_range,"Widget depth limit is 64");
    return parent.index_;
}
core::Result<WidgetAppearance> WidgetState::appearance() const {if(auto r=ready();!r)return std::unexpected(r.error());return impl_->appearance;}
core::Result<void> WidgetState::default_content(Widget host,Widget slot){
    if(auto result=ready();!result)return result;
    if(!impl_->building)return core::fail(core::ErrorCode::closed,"Default content is a construction-time contract");
    if(host.state_.lock().get()!=this||slot.state_.lock().get()!=this||host.index_>=impl_->nodes.size()||slot.index_>=impl_->nodes.size()||host.index_==slot.index_||!impl_->descendant(slot.index_,host.index_))return core::fail(core::ErrorCode::invalid_argument,"Default content must be a strict descendant in the same generation");
    const auto valid=[](WidgetKind kind){return kind==WidgetKind::vertical_stack||kind==WidgetKind::horizontal_stack||kind==WidgetKind::overlay_stack||kind==WidgetKind::absolute_stack||kind==WidgetKind::action_surface;};
    if(!valid(impl_->nodes[host.index_].kind)||!valid(impl_->nodes[slot.index_].kind))return core::fail(core::ErrorCode::invalid_argument,"Default content requires ordinary native containers");
    impl_->nodes[host.index_].default_content=slot.index_;return {};
}
core::Result<void> WidgetState::button_options(std::size_t i,ButtonOptions options){
    if(auto result=ready();!result)return result;
    if(i>=impl_->nodes.size()||impl_->nodes[i].kind!=WidgetKind::button||impl_->nodes[i].color_button||!valid_button(options))return core::fail(core::ErrorCode::invalid_argument,"Button options require a valid ordinary button");
    const bool requested=options.enabled;auto& node=impl_->nodes[i];node.text=options.text;node.button=std::move(options);impl_->invalidate(true,i);impl_->accessibility_changed();return enabled(i,requested);
}
Widget WidgetState::publish(std::size_t parent,WidgetKind kind,std::string_view id) {
    const auto index=impl_->nodes.size();
    impl_->nodes.emplace_back();++impl_->domain->nodes; auto& n=impl_->nodes.back(); n.kind=kind; n.parent=parent; n.id=id;
    if (parent!=absent) {
        impl_->nodes[parent].children.push_back(index);
        if(impl_->nodes[parent].popup && !impl_->nodes[parent].popup->popover && kind!=WidgetKind::menu && kind!=WidgetKind::popover) {
            auto& entries=impl_->nodes[parent].popup->entries;n.menu_index=int(entries.size());entries.push_back(index);
        }
    }
    impl_->ids.emplace(id,index);impl_->layout_dirty=true;impl_->host_order_dirty=true;
    for(auto current=index;current!=absent;current=impl_->nodes[current].parent){impl_->nodes[current].metrics_dirty=true;impl_->nodes[current].layout_width.reset();}
    return Widget(weak_from_this(),index,impl_->owner);
}
core::Result<Widget> WidgetState::add(WidgetKind kind,Widget parent,std::string_view id,StackOptions stack,LabelOptions label,ButtonOptions button,runtime::Task action) {
    auto p=prepare(parent,id); if (!p) return std::unexpected(p.error());
    if ((!stack_kind(kind) && kind!=WidgetKind::spacer && kind!=WidgetKind::label && kind!=WidgetKind::button) ||
        !valid_stack(stack)||!valid_label(label)||!valid_button(button)||kind==WidgetKind::tab_bar||kind==WidgetKind::segmented_control)
        return core::fail(core::ErrorCode::invalid_argument,"Invalid widget options");
    if(kind==WidgetKind::button && *p!=absent && impl_->nodes[*p].kind==WidgetKind::segmented_control && impl_->nodes[*p].segments.force_borderless_tone) button.tone=ButtonTone::borderless;
    auto w=publish(*p,kind,id); auto& n=impl_->nodes.back(); n.stack=stack; n.label=std::move(label); n.button=std::move(button);
    n.text=kind==WidgetKind::label?n.label.text:n.button.text; n.enabled=n.button.enabled && n.button.tone!=ButtonTone::disabled; n.action=std::move(action); return w;
}
core::Result<Widget> WidgetState::check(WidgetKind kind,Widget p,std::string_view id,CheckOptions o,std::function<void(bool)> action) {
    auto parent=prepare(p,id); if (!parent) return std::unexpected(parent.error());
    if (!check_kind(kind) || !valid_title(o.text) || o.text.size()>65536 || !valid_title(o.group)) return core::fail(core::ErrorCode::invalid_argument,"Invalid check control");
    auto w=publish(*parent,kind,id); auto& n=impl_->nodes.back(); n.text=std::move(o.text); n.enabled=o.enabled; n.group=std::move(o.group); n.toggled=std::move(action);
    if (o.checked) { auto result=checked(w.index_,true); if (!result) return std::unexpected(result.error()); } return w;
}
core::Result<Widget> WidgetState::input_field(Widget p,std::string_view id,InputOptions o,std::function<void(std::string)> edited,runtime::Task submitted) {
    auto parent=prepare(p,id); if (!parent) return std::unexpected(parent.error());
    if (!valid_input(o)) return core::fail(core::ErrorCode::invalid_argument,"Invalid input control");
    if(o.maximum_length)o.text.resize(scalar_prefix(o.text,o.maximum_length));
    auto w=publish(*parent,WidgetKind::input_field,id); auto& n=impl_->nodes.back(); n.text=o.text; n.cursor=n.anchor=n.text.size(); n.field=std::move(o); n.enabled=n.field.enabled; n.edited=std::move(edited); n.action=std::move(submitted); return w;
}
core::Result<Widget> WidgetState::slider(Widget p,std::string_view id,SliderOptions o,std::function<void(double)> action) {
    auto parent=prepare(p,id); if (!parent) return std::unexpected(parent.error());
    if (!std::isfinite(o.minimum) || !std::isfinite(o.maximum) || !std::isfinite(o.maximum-o.minimum) || o.minimum>=o.maximum || !std::isfinite(o.value) || !geometry(o.step) || !geometry(o.width) || o.width<18 ||
        !geometry(o.thumb_diameter) || !o.thumb_diameter || !geometry(o.track_height) || !o.track_height || !geometry(o.height) || o.height<std::max(o.thumb_diameter,o.track_height) || !geometry(o.range_inset) || o.range_inset<o.thumb_diameter/2 || o.range_inset>o.width/2 || !valid_title(o.accessibility_name))
        return core::fail(core::ErrorCode::invalid_argument,"Invalid slider range");
    auto w=publish(*parent,WidgetKind::slider,id); auto& n=impl_->nodes.back(); n.slider=o; n.slider.value=std::clamp(o.value,o.minimum,o.maximum); n.enabled=o.enabled; n.moved=std::move(action); return w;
}
core::Result<Widget> WidgetState::absolute_stack(Widget p,std::string_view id,Size size) {
    auto parent=prepare(p,id);if(!parent)return std::unexpected(parent.error());
    if(!geometry(size.width) || !geometry(size.height))return core::fail(core::ErrorCode::invalid_argument,"Invalid absolute stack extent");
    auto w=publish(*parent,WidgetKind::absolute_stack,id);impl_->nodes.back().extent=size;return w;
}
core::Result<Widget> WidgetState::image(Widget p,std::string_view id,ImageOptions options) {
    auto parent=prepare(p,id);if(!parent)return std::unexpected(parent.error());
    if(!valid_image_options(options))return core::fail(core::ErrorCode::invalid_argument,"Invalid image view");
    auto w=publish(*parent,WidgetKind::image,id);impl_->nodes.back().image=std::move(options);return w;
}
core::Result<Widget> WidgetState::scroll_view(Widget p,std::string_view id,ScrollOptions options){auto parent=prepare(p,id);if(!parent)return std::unexpected(parent.error());if(!valid_scroll(options))return core::fail(core::ErrorCode::invalid_argument,"Invalid scroll viewport");auto w=publish(*parent,WidgetKind::scroll_view,id);impl_->nodes.back().scroll=std::make_unique<Impl::Node::Scroll>();impl_->nodes.back().scroll->options=options;return w;}
core::Result<Widget> WidgetState::progress(Widget p,std::string_view id,ProgressOptions o) {
    auto parent=prepare(p,id); if (!parent) return std::unexpected(parent.error());
    if (!std::isfinite(o.minimum) || !std::isfinite(o.maximum) || !std::isfinite(o.maximum-o.minimum) || o.minimum>o.maximum || !std::isfinite(o.start) || !std::isfinite(o.value) || !geometry(o.width) || !geometry(o.height) || !o.width || !o.height)
        return core::fail(core::ErrorCode::invalid_argument,"Invalid progress range");
    auto w=publish(*parent,WidgetKind::progress,id); auto& n=impl_->nodes.back(); n.progress=o; n.progress.value=std::clamp(o.value,o.minimum,o.maximum); return w;
}
core::Result<Widget> WidgetState::tab(Widget p,std::string_view id,TabOptions o,runtime::Task action) {
    auto parent=prepare(p,id,WidgetKind::tab);if(!parent) return std::unexpected(parent.error());
    if(!valid_title(o.text) || o.text.size()>65536 || !valid_title(o.icon_name) || o.icon_name.find_first_not_of(" \t\r\n")==std::string::npos || !o.icon ||
        !valid_title(o.badge) || o.badge.size()>1024 || !geometry(o.icon_size) || o.icon_size<1 || o.icon_size>256 || !geometry(o.minimum_width) || o.minimum_width<1 || static_cast<unsigned>(o.style)>1)
        return core::fail(core::ErrorCode::invalid_argument,"Tab requires a named icon and valid text, badge, style and metrics");
    auto w=publish(*parent,WidgetKind::tab,id);auto& n=impl_->nodes.back();n.text=o.text;n.enabled=o.enabled;n.tab=std::move(o);n.action=std::move(action);return w;
}
core::Result<Widget> WidgetState::tab_bar(Widget p,std::string_view id,TabBarOptions o,std::function<void(int)> changed) {
    auto parent=prepare(p,id);if(!parent) return std::unexpected(parent.error());
    if(!geometry(o.spacing) || !geometry(o.minimum_tab_width) || o.minimum_tab_width<1 || o.current_index < -1)
        return core::fail(core::ErrorCode::invalid_argument,"Invalid TabBar metrics or index");
    auto w=publish(*parent,WidgetKind::tab_bar,id);auto& n=impl_->nodes.back();n.tabbar=o;n.stack.spacing=o.spacing;n.selection_changed=std::move(changed);return w;
}
core::Result<Widget> WidgetState::combo_box(Widget p,std::string_view id,ComboBoxOptions o,ControlEvents events) {
    auto parent=prepare(p,id);if(!parent) return std::unexpected(parent.error());
    if(!valid_title(o.text) || o.text.size()>65536 || !geometry(o.width) || o.width<19 ||
        (o.tone!=ButtonTone::primary && o.tone!=ButtonTone::borderless) || static_cast<unsigned>(o.arrow)>2)
        return core::fail(core::ErrorCode::invalid_argument,"Invalid ComboBox text, tone, arrow or width");
    if(*parent!=absent && impl_->nodes[*parent].kind==WidgetKind::segmented_control && impl_->nodes[*parent].segments.force_borderless_tone) o.tone=ButtonTone::borderless;
    auto w=publish(*parent,WidgetKind::combo_box,id);auto& n=impl_->nodes.back();n.text=o.text;n.enabled=o.enabled;n.combo=std::move(o);n.action=std::move(events.clicked);n.events=std::move(events);return w;
}
core::Result<Widget> WidgetState::stepper(Widget p,std::string_view id,StepperOptions o,std::function<void(int)> stepped,ControlEvents events) {
    auto parent=prepare(p,id);if(!parent) return std::unexpected(parent.error());
    if(static_cast<unsigned>(o.tone)>4 || static_cast<unsigned>(o.arrow)>2 || !geometry(o.corner_radius) || !valid_title(o.accessibility_name))
        return core::fail(core::ErrorCode::invalid_argument,"Invalid Stepper tone, arrow, radius or accessible name");
    if(*parent!=absent && impl_->nodes[*parent].kind==WidgetKind::segmented_control && impl_->nodes[*parent].segments.force_borderless_tone) o.tone=ButtonTone::borderless;
    auto w=publish(*parent,WidgetKind::stepper,id);auto& n=impl_->nodes.back();n.enabled=o.enabled && o.tone!=ButtonTone::disabled;n.stepper=std::move(o);n.action=std::move(events.clicked);n.events=std::move(events);n.stepped=std::move(stepped);return w;
}
core::Result<Widget> WidgetState::segmented_control(Widget p,std::string_view id,SegmentedOptions o) {
    auto parent=prepare(p,id);if(!parent) return std::unexpected(parent.error());
    if(!geometry(o.horizontal_padding) || !geometry(o.vertical_padding) || !geometry(o.spacing) || !geometry(o.border_width) || !geometry(o.corner_radius) || !valid_color(o.background) || !valid_color(o.border) || o.background.alpha!=1 || o.border.alpha!=1)
        return core::fail(core::ErrorCode::invalid_argument,"Invalid segmented row metrics or color");
    auto w=publish(*parent,WidgetKind::segmented_control,id);auto& n=impl_->nodes.back();n.segments=o;n.stack.spacing=o.spacing;return w;
}
core::Result<Widget> WidgetState::popover(Widget p,std::string_view id,PopoverOptions o,PopoverEvents events) {
    auto parent=prepare(p,id,WidgetKind::popover);if(!parent)return std::unexpected(parent.error());
    if(!valid_popover_options(o)||!valid_transition(o.transition))return core::fail(core::ErrorCode::invalid_argument,"Invalid popover options");
    if(auto result=prepare_motion(o);!result)return std::unexpected(result.error());
    auto data=std::make_unique<Impl::Popup>();Impl::configure_popup(*data,std::move(o));data->events.opened=std::move(events.opened);data->events.closed=std::move(events.closed);
    data->canceled=std::move(events.canceled);
    auto result=publish(*parent,WidgetKind::popover,id);impl_->nodes.back().popup=std::move(data);return result;
}
core::Result<Widget> WidgetState::menu(Widget p,std::string_view id,MenuOptions o,MenuEvents events) {
    auto parent=prepare(p,id,WidgetKind::menu);if(!parent) return std::unexpected(parent.error());
    const bool context=o.style==MenuStyle::context;
    o.compact_items=o.compact_items.value_or(context);if(!o.item_width) o.item_width=context?141:145;
    o.top_padding=o.top_padding.value_or(context?8:4);o.bottom_padding=o.bottom_padding.value_or(context?8:4);
    if(static_cast<unsigned>(o.style)>1 || !geometry(o.item_width) || o.item_width<1 || o.item_width>8192 || !geometry(o.item_spacing) ||
        !geometry(o.left_padding) || o.left_padding>8192 || !geometry(o.right_padding) || o.right_padding>8192 || !geometry(*o.top_padding) || *o.top_padding>8192 || !geometry(*o.bottom_padding) || *o.bottom_padding>8192 ||
        !geometry(o.edge_margin) || !geometry(o.corner_radius) || !std::isfinite(o.opacity) || o.opacity<0 || o.opacity>1 ||
        (o.background && !valid_color(*o.background)) || o.selected_index < -1)
        return core::fail(core::ErrorCode::invalid_argument,"Invalid menu metrics, palette or selection");
    auto data=std::make_unique<Impl::Popup>();data->options=std::move(o);data->events=std::move(events);
    auto w=publish(*parent,WidgetKind::menu,id);impl_->nodes.back().popup=std::move(data);return w;
}
core::Result<Widget> WidgetState::menu_item(Widget p,std::string_view id,MenuItemOptions o,runtime::Task action) {
    auto parent=prepare(p,id,WidgetKind::menu_item);if(!parent) return std::unexpected(parent.error());
    const auto popup=*parent!=absent?impl_->nodes[*parent].popup.get():nullptr;
    const bool menu_parent=popup&&!popup->popover;
    o.compact=o.compact.value_or(menu_parent?*popup->options.compact_items:o.style==MenuStyle::context);
    o.shortcut=o.shortcut.value_or(o.style==MenuStyle::context?"Key":"key");
    if(!o.item_width) o.item_width=menu_parent?popup->options.item_width:*o.compact?141:161;
    if(!o.item_height) o.item_height=*o.compact?18:24;
    if(static_cast<unsigned>(o.style)>1 || static_cast<unsigned>(o.state)>2 || static_cast<unsigned>(o.direction)>4 ||
        !valid_title(o.label) || o.label.size()>65536 || !valid_title(*o.shortcut) || o.shortcut->size()>1024 || !valid_title(o.key_placeholder) || o.key_placeholder.size()>1024 || !valid_title(o.icon_name) ||
        !geometry(o.item_width) || o.item_width<1 || o.item_width>8192 || !geometry(o.item_height) || o.item_height<1 || o.item_height>8192 ||
        !geometry(o.icon_size) || o.icon_size<1 || o.icon_size>256 || !geometry(o.chevron_size) || o.chevron_size<1 || o.chevron_size>256)
        return core::fail(core::ErrorCode::invalid_argument,"Invalid menu item text, state, direction or metrics");
    auto data=std::make_unique<MenuItemOptions>(std::move(o));auto w=publish(*parent,WidgetKind::menu_item,id);auto& n=impl_->nodes.back();
    n.text=data->label;n.enabled=data->enabled && data->state!=MenuItemState::inactive;n.menu_item=std::move(data);n.action=std::move(action);return w;
}
core::Result<Widget> WidgetState::menu_divider(Widget p,std::string_view id,MenuDividerOptions o) {
    auto parent=prepare(p,id,WidgetKind::menu_divider);if(!parent) return std::unexpected(parent.error());
    const auto popup=*parent!=absent?impl_->nodes[*parent].popup.get():nullptr;
    const auto style=popup?popup->options.style:o.style;
    o.line_padding=o.line_padding.value_or(popup?(*popup->options.compact_items?4:0):style==MenuStyle::context?4:0);
    if(!o.line_length) o.line_length=style==MenuStyle::context?145:220;
    o.color=o.color.value_or(style==MenuStyle::context?WidgetColor{1,1,1,.3}:WidgetColor{.1370945126,.1402995437,.1419020444,1});
    if(static_cast<unsigned>(o.style)>1 || !geometry(o.thickness) || !geometry(o.cross_padding) || !geometry(*o.line_padding) || !geometry(o.line_length) || o.line_length>8192 || !valid_color(*o.color))
        return core::fail(core::ErrorCode::invalid_argument,"Invalid menu divider metrics or color");
    auto data=std::make_unique<MenuDividerOptions>(std::move(o));auto w=publish(*parent,WidgetKind::menu_divider,id);impl_->nodes.back().divider=std::move(data);return w;
}
core::Result<void> WidgetState::attach(void* parent) {
    if (auto r=ready();!r) return r;
    if (!impl_->building || impl_->nodes.empty()) return core::fail(core::ErrorCode::invalid_argument,"Factory must construct one root widget");
    for(std::size_t i=0;i<impl_->nodes.size();++i) if(impl_->nodes[i].kind==WidgetKind::tab_bar) impl_->reconcile(i);
    for(const auto& n:impl_->nodes) if(n.popup && n.popup->options.selected_index>=0 &&
        (std::size_t(n.popup->options.selected_index)>=n.popup->entries.size() || impl_->nodes[n.popup->entries[std::size_t(n.popup->options.selected_index)]].kind!=WidgetKind::menu_item))
        return core::fail(core::ErrorCode::out_of_range,"Initial menu selection must name a menu item");
    auto weak=weak_from_this();
    auto surface=std::make_unique<WidgetSurface>(parent,WidgetSurfaceCallbacks{
        .frame=[weak](Size s) { auto owner=weak.lock(); return owner?owner->frame(s):CanvasFrame{}; },
        .input=[weak](WidgetInput event) { if (auto owner=weak.lock()) { auto invoke=owner->impl_->invoke; invoke([owner,event=std::move(event)] { auto r=owner->input(event); if (!r) throw std::runtime_error(r.error().message); }); } },
        .accessibility=[weak] { if (auto owner=weak.lock()) { auto r=owner->snapshots(); if (r) return *r; } return std::vector<WidgetSnapshot>{}; },
        .lookup=[weak](std::string_view id) { if(auto owner=weak.lock()) { auto w=owner->find(id); if(w) { auto s=w->snapshot(); if(s) return *s; } } return WidgetSnapshot{}; },
        .failure=[weak](std::exception_ptr error) { if(auto owner=weak.lock()) { auto invoke=owner->impl_->invoke; invoke([error] { std::rethrow_exception(error); }); } },
        .text_content=[weak](std::string_view id,TextRange range)->core::Result<TextContent>{if(auto owner=weak.lock()){auto w=owner->find(id);if(!w)return std::unexpected(w.error());return w->text_content(range);}return core::fail(core::ErrorCode::closed,"Text surface closed");},
        .text_position=[weak](std::string_view id,double x,double y)->core::Result<std::size_t>{if(auto owner=weak.lock()){auto w=owner->find(id);if(!w)return std::unexpected(w.error());return w->text_position(x,y);}return core::fail(core::ErrorCode::closed,"Text surface closed");},
        .renderer=impl_->frame_hooks.renderer});
    impl_->resolved_font_family=surface->resolve_font_family(impl_->appearance.font_family);
    impl_->resolved_compact_font_family=surface->resolve_font_family("Inter");
    const auto inherit_fonts=[&](auto&& self,Impl& target)->void{for(auto& n:target.nodes)if(auto child=n.content?n.content->view:n.popup?n.popup->content_view:nullptr){child->impl_->resolved_font_family=impl_->resolved_font_family;child->impl_->resolved_compact_font_family=impl_->resolved_compact_font_family;self(self,*child->impl_);}};inherit_fonts(inherit_fonts,*impl_);
    for(auto& n:impl_->nodes) n.metrics_dirty=true;impl_->layout_dirty=true;
    impl_->surface=std::move(surface); impl_->building=false; impl_->surface->flush(); return {};
}
core::Result<Widget> WidgetState::color_surface(Widget p,std::string_view id,ColorSurfaceOptions o,std::function<void(WidgetColor)> edited) {
    auto parent=prepare(p,id,WidgetKind::color_surface);if(!parent)return std::unexpected(parent.error());
    if(!o.model || !geometry(o.width) || !geometry(o.height) || o.width<12 || o.height<12 || o.width>1024 || o.height>1024 || unsigned(o.field)>unsigned(ColorField::brightness_rail) || !valid_title(o.accessibility_name))
        return core::fail(core::ErrorCode::invalid_argument,"Color surface requires a model, field and 12..1024 logical dimensions");
    if(auto state=o.model->state();!state)return std::unexpected(state.error());
    auto data=std::make_unique<ColorSurface>();data->options=std::move(o);data->edited=std::move(edited);
    auto model=data->options.model;const auto index=impl_->nodes.size();auto widget=publish(*parent,WidgetKind::color_surface,id);
    impl_->nodes[index].enabled=data->options.enabled;impl_->nodes[index].color=std::move(data);
    if(auto r=color_model(index,std::move(model));!r)return std::unexpected(r.error());return widget;
}
core::Result<Widget> WidgetState::color_button(Widget p,std::string_view id,ColorButtonOptions o,ControlEvents events) {
    if(!o.model || !std::isfinite(o.size) || o.size<8 || o.size>256 || !std::isfinite(o.border_opacity) || o.border_opacity<0 || o.border_opacity>1 || (o.show_hue_ring&&!o.hue_ring) || !valid_title(o.accessibility_name))return core::fail(core::ErrorCode::invalid_argument,"Color button requires valid geometry, border opacity and a model/image");
    if(auto state=o.model->state();!state)return std::unexpected(state.error());
    auto data=std::make_unique<ColorButton>();data->options=std::move(o);auto model=data->options.model;
    auto widget=add(WidgetKind::button,p,id,{}, {}, {.enabled=data->options.enabled,.tone=ButtonTone::normal,.accessibility_name=data->options.accessibility_name},events.clicked);
    if(!widget)return widget;auto& node=impl_->nodes[widget->index_];node.color_button=std::move(data);node.events=std::move(events);
    if(auto r=color_model(widget->index_,std::move(model));!r)return std::unexpected(r.error());return widget;
}
core::Result<void> WidgetState::color_button_size(std::size_t i,double size) {
    if(auto r=ready();!r)return r;if(i>=impl_->nodes.size() || !impl_->nodes[i].color_button || !std::isfinite(size)||size<8||size>256)return core::fail(core::ErrorCode::invalid_argument,"Invalid color button or size");
    auto& o=impl_->nodes[i].color_button->options;if(o.size==size)return {};o.size=size;impl_->invalidate(true,i);return {};
}
core::Result<void> WidgetState::show_hue_ring(std::size_t i,bool visible) {
    if(auto r=ready();!r)return r;if(i>=impl_->nodes.size() || !impl_->nodes[i].color_button || (visible&&!impl_->nodes[i].color_button->options.hue_ring))return core::fail(core::ErrorCode::invalid_argument,"Invalid color button or missing hue ring");
    auto& o=impl_->nodes[i].color_button->options;if(o.show_hue_ring==visible)return {};o.show_hue_ring=visible;impl_->invalidate();return {};
}
core::Result<void> WidgetState::color_model(std::size_t i,std::shared_ptr<ColorPickerModel> model) {
    if(auto r=ready();!r)return r;
    if(i>=impl_->nodes.size() || (!impl_->nodes[i].color&&!impl_->nodes[i].color_button) || !model)return core::fail(core::ErrorCode::invalid_argument,"Color model requires a color surface/button and nonnull model");
    auto state=model->state();if(!state)return std::unexpected(state.error());auto& node=impl_->nodes[i];
    auto& current=node.color?node.color->options.model:node.color_button->options.model;auto& observer=node.color?node.color->receiver:node.color_button->receiver;
    if(current==model && observer)return {};
    auto weak=weak_from_this();auto source=std::weak_ptr<ColorPickerModel>(model);
    auto receiver=std::make_shared<ColorChangePort>([weak,source,i](const ColorPickerChange& change) {
        auto self=weak.lock();auto model=source.lock();
        if(!self || !model || !self->ready() || i>=self->impl_->nodes.size())return;
        auto& n=self->impl_->nodes[i];if((n.color?n.color->options.model:n.color_button?n.color_button->options.model:nullptr)!=model)return;
        auto edited=change.edited&&n.color?n.color->edited:std::function<void(WidgetColor)>{};
        if(n.color_button&&change.color_changed){auto s=model->state();if(!s)throw std::runtime_error(s.error().message);n.color_button->current=s->color;}
        if(n.color?change.channels_changed:change.color_changed)self->impl_->invalidate();
        if(change.color_changed)self->impl_->accessibility_value_changed(n.id);
        if(edited){auto value=model->state();if(!value)throw std::runtime_error(value.error().message);auto invoke=self->impl_->invoke;invoke([edited=std::move(edited),color=value->color]{edited(color);});}
    });
    if(auto r=model->observe(receiver);!r)return r;
    current=std::move(model);observer=std::move(receiver);if(node.color)node.color->drag_part=-1;else node.color_button->current=state->color;
    if(impl_->pressed==i)impl_->pressed=absent;impl_->invalidate();return {};
}
core::Result<std::shared_ptr<ColorPickerModel>> WidgetState::color_model(std::size_t i) {
    if(auto r=ready();!r)return std::unexpected(r.error());if(i>=impl_->nodes.size() || (!impl_->nodes[i].color&&!impl_->nodes[i].color_button))return core::fail(core::ErrorCode::invalid_argument,"Widget is not a color surface/button");auto& n=impl_->nodes[i];return n.color?n.color->options.model:n.color_button->options.model;
}
core::Result<void> WidgetState::color_field(std::size_t i,ColorField field) {
    if(auto r=ready();!r)return r;if(i>=impl_->nodes.size() || !impl_->nodes[i].color || unsigned(field)>unsigned(ColorField::brightness_rail))return core::fail(core::ErrorCode::invalid_argument,"Invalid color surface or field");
    auto& color=*impl_->nodes[i].color;color.options.field=field;color.drag_part=-1;if(impl_->pressed==i)impl_->pressed=absent;impl_->invalidate();return {};
}
core::Result<void> WidgetState::retain(std::shared_ptr<void> owner) {
    if (auto r=ready();!r) return r;
    if (!impl_->building || !owner) return core::fail(core::ErrorCode::invalid_argument,"Scope requires an active factory and nonnull owner");
    impl_->owners.push_back(std::move(owner)); return {};
}
core::Result<Widget> WidgetState::action_surface(Widget parent,std::string_view id,ActionOptions options,runtime::Task task){
    auto p=prepare(parent,id,WidgetKind::action_surface);if(!p)return std::unexpected(p.error());if(!valid_action(options))return core::fail(core::ErrorCode::invalid_argument,"Invalid action surface options");
    auto widget=publish(*p,WidgetKind::action_surface,id);auto& n=impl_->nodes.back();n.text=options.text;n.enabled=options.enabled&&options.interaction_enabled;n.button.tone=ButtonTone::normal;n.button.accessibility_name=options.accessibility_name;n.checked=options.checked;n.action_surface=std::make_unique<ActionOptions>(std::move(options));n.action=std::move(task);return widget;
}
core::Result<ActionOptions> WidgetState::action_options(std::size_t i)const{if(auto r=ready();!r)return std::unexpected(r.error());if(i>=impl_->nodes.size()||!impl_->nodes[i].action_surface)return core::fail(core::ErrorCode::invalid_argument,"Widget is not an action surface");auto result=*impl_->nodes[i].action_surface;result.checked=impl_->nodes[i].checked;return result;}
core::Result<void> WidgetState::action_options(std::size_t i,ActionOptions options){
    if(auto r=ready();!r)return r;if(i>=impl_->nodes.size()||!impl_->nodes[i].action_surface||!valid_action(options))return core::fail(core::ErrorCode::invalid_argument,"Invalid action surface options");
    auto& n=impl_->nodes[i];const bool active=options.enabled;n.checked=options.checked;n.text=options.text;n.button.accessibility_name=options.accessibility_name;*n.action_surface=std::move(options);impl_->invalidate(true,i);impl_->accessibility_changed();return enabled(i,active);
}
core::Result<Widget> WidgetState::content_host(Widget parent,std::string_view id,Size size){
    auto p=prepare(parent,id);if(!p)return std::unexpected(p.error());
    if(!geometry(size.width)||!geometry(size.height)||size.width>8192||size.height>8192)return core::fail(core::ErrorCode::invalid_argument,"Content host requires bounded native dimensions");
    auto widget=publish(*p,WidgetKind::content_host,id);auto& node=impl_->nodes.back();node.content=std::make_unique<Impl::Node::Content>();node.extent=size;impl_->host_order_dirty=true;return widget;
}
core::Result<Widget> WidgetState::content_item(std::size_t i,std::string_view id){
    if(auto r=ready();!r)return std::unexpected(r.error());
    if(i>=impl_->nodes.size()||!impl_->nodes[i].content)return core::fail(core::ErrorCode::invalid_argument,"Widget is not a content host");
    auto child=impl_->nodes[i].content->view;if(!child)return core::fail(core::ErrorCode::closed,"Content host has no activation");
    if(id.empty())return Widget(child,0,impl_->owner);return child->find(id);
}
core::Result<void> WidgetState::content(std::size_t i,WidgetFactory factory,std::function<core::Result<void>()> commit){
    if(auto r=ready();!r)return r;
    if(i>=impl_->nodes.size()||!impl_->nodes[i].content)return core::fail(core::ErrorCode::invalid_argument,"Widget is not a content host");
    auto& host=*impl_->nodes[i].content;
    if(host.constructing)return core::fail(core::ErrorCode::busy,"Content host is already constructing a generation");
    auto staged=impl_->domain->staged.lock();bool within_staged=false;
    for(auto current=shared_from_this();current;current=current->impl_->embedding_owner.lock())if(current==staged){within_staged=true;break;}
    const bool nested=impl_->domain_transition();
    if(nested&&(!impl_->building||host.view||!within_staged))return core::fail(core::ErrorCode::busy,"Cannot replace another activation during content construction");
    if(host.epoch==std::numeric_limits<std::uint64_t>::max())return core::fail(core::ErrorCode::full,"Content generation exhausted");
    auto root=shared_from_this();std::size_t depth=0;while(auto parent=root->impl_->embedding_owner.lock()){root=parent;if(++depth>=8)return core::fail(core::ErrorCode::out_of_range,"Content activation depth limit is eight");}
    auto previous=host.view;const auto next_epoch=host.epoch+1;
    struct Guard final {
        Impl& root;Impl& owner;std::size_t index;bool transition;std::size_t allowance;std::weak_ptr<WidgetState> staged;
        Guard(Impl& r,Impl& o,std::size_t i,std::size_t replacing,bool nested):root(r),owner(o),index(i),transition(r.content_transition),allowance(r.domain->allowance),staged(r.domain->staged){root.content_transition=true;owner.nodes[index].content->constructing=true;if(!nested)root.domain->allowance=replacing;}
        ~Guard(){if(owner.active&&index<owner.nodes.size()&&owner.nodes[index].content)owner.nodes[index].content->constructing=false;root.content_transition=transition;root.domain->allowance=allowance;root.domain->staged=staged;}
    }guard{*root->impl_,*impl_,i,previous?previous->impl_->active_nodes():0,nested};
    // The owner may close while a factory runs. Do not retain references to its
    // node after calling user code; the guard's host flag is handled below.
    std::shared_ptr<WidgetState> child;
    if(factory){
        impl_->layout(impl_->surface?impl_->surface->size():impl_->viewport);
        child=std::make_shared<WidgetState>(impl_->invoke,impl_->appearance,impl_->frame_hooks);child->impl_->domain=impl_->domain;
        child->impl_->embedding_owner=weak_from_this();child->impl_->embedding_popup=i;child->impl_->fill_viewport=true;
        child->impl_->viewport={impl_->nodes[i].bounds.width,impl_->nodes[i].bounds.height};
        child->impl_->scope_prefix=impl_->scope_prefix+"@iiscope:"+std::to_string(i)+":"+std::to_string(next_epoch)+"/";
        child->impl_->resolved_font_family=impl_->resolved_font_family;child->impl_->resolved_compact_font_family=impl_->resolved_compact_font_family;impl_->domain->staged=child;
        WidgetBuilder builder(child);core::Result<void> built;bool completed=false;auto invoke=impl_->invoke;
        try{invoke([&]{try{built=factory(builder);}catch(...){built=core::fail(core::ErrorCode::failed,"Content factory failed",std::current_exception());}completed=true;});}
        catch(...){child->close();return core::fail(core::ErrorCode::failed,"Content factory admission failed",std::current_exception());}
        if(!completed||!impl_->active){child->close();return core::fail(core::ErrorCode::closed,"Content owner retired during construction");}
        if(!built){child->close();return built;}
        if(child->impl_->nodes.empty()){child->close();return core::fail(core::ErrorCode::invalid_argument,"Content factory must build one root View");}
        for(std::size_t j=0;j<child->impl_->nodes.size();++j)if(child->impl_->nodes[j].kind==WidgetKind::tab_bar)child->impl_->reconcile(j);
        for(const auto& node:child->impl_->nodes)if(node.popup&&node.popup->options.selected_index>=0&&(std::size_t(node.popup->options.selected_index)>=node.popup->entries.size()||child->impl_->nodes[node.popup->entries[std::size_t(node.popup->options.selected_index)]].kind!=WidgetKind::menu_item)){child->close();return core::fail(core::ErrorCode::out_of_range,"Initial content menu selection must name an item");}
        child->impl_->building=false;
    }
    if(commit){core::Result<void> result;bool completed=false;auto invoke=impl_->invoke;
        try{invoke([&]{try{result=commit();}catch(...){result=core::fail(core::ErrorCode::failed,"Content commit failed",std::current_exception());}completed=true;});}catch(...){result=core::fail(core::ErrorCode::failed,"Content commit admission failed",std::current_exception());}
        if(!completed||!impl_->active){if(child)child->close();return core::fail(core::ErrorCode::closed,"Content owner retired during commit");}
        if(!result){if(child)child->close();return result;}
    }
    if(!impl_->active){if(child)child->close();return core::fail(core::ErrorCode::closed,"Content owner retired before publication");}
    auto& current=*impl_->nodes[i].content;current.view=std::move(child);current.epoch=next_epoch;
    if(previous)previous->close();
    if(!impl_->active)return core::fail(core::ErrorCode::closed,"Content owner retired after publication");
    if(!impl_->nodes[i].content->view&&impl_->focused==i)impl_->focused=absent;
    impl_->hovered=absent;impl_->invalidate(true,i);impl_->accessibility_changed();return {};
}
void WidgetState::close() noexcept {
    if (!impl_->active) return;
    if (impl_->owner!=std::this_thread::get_id()) std::terminate();
    auto input_cancel=impl_->cancel_input_capture();auto invoke=impl_->invoke;impl_->active=false;
    auto watches=std::move(impl_->interaction_watches);
    for(const auto& watch:watches)if(auto port=watch->port.lock()){try{auto final=watch->delivered.value_or(WidgetInteraction{});final.alive=final.visible=final.enabled=final.hovered=final.pressed=final.focused=final.opened=false;(void)port->send(final);}catch(...){}}
    if(input_cancel)try{invoke(std::move(input_cancel));}catch(...){}
    if(impl_->motion_tick){(void)impl_->motion_frames->remove(*impl_->motion_tick);impl_->motion_tick.reset();}
    if(impl_->motion_active){impl_->motion_active=false;if(impl_->frame_hooks.activity)impl_->frame_hooks.activity(false);}
    for(auto& receiver:impl_->motion_receivers)(void)receiver.port->close();impl_->motion_receivers.clear();impl_->frame_hooks={};
    for(auto& n:impl_->nodes)if(auto child=n.content?n.content->view:n.popup?n.popup->content_view:nullptr)child->close();impl_->surface.reset();impl_->popups.clear();impl_->visual_popups.clear();impl_->domain->nodes-=impl_->nodes.size();impl_->nodes.clear(); impl_->ids.clear(); impl_->invoke={}; impl_->owners.clear();
}
core::Result<Widget> WidgetState::find(std::string_view id) {
    if (auto r=ready();!r) return std::unexpected(r.error());
    const auto qualified=id.starts_with(impl_->scope_prefix)?std::string(id):impl_->scope_prefix+std::string(id);
    if(!impl_->scope_prefix.empty()&&id.starts_with(impl_->scope_prefix))id.remove_prefix(impl_->scope_prefix.size());
    auto it=impl_->ids.find(id);if(it==impl_->ids.end()){for(const auto& n:impl_->nodes)if(auto child=n.content?n.content->view:n.popup?n.popup->content_view:nullptr;child&&qualified.starts_with(child->impl_->scope_prefix))return child->find(qualified);return core::fail(core::ErrorCode::not_found,"Widget id was not found");}
    return Widget(weak_from_this(),it->second,impl_->owner);
}
core::Result<WidgetInteraction> WidgetState::interaction(std::size_t i){
    if(auto r=ready();!r)return std::unexpected(r.error());
    if(i>=impl_->nodes.size())return core::fail(core::ErrorCode::not_found,"Interaction target was not found");
    if(auto parent=impl_->embedding_owner.lock())parent->impl_->layout(parent->impl_->surface?parent->impl_->surface->size():parent->impl_->viewport);
    impl_->layout(impl_->surface?impl_->surface->size():impl_->viewport);
    const auto area=impl_->popup_viewport(impl_->viewport);const auto offset=global_offset();
    auto bounds=impl_->visual_bounds(i);bounds.x+=float(offset.width);bounds.y+=float(offset.height);
    Rect clip{0,0,area.width,area.height};if(auto local=impl_->clip_rect(i)){local->x+=float(offset.width);local->y+=float(offset.height);clip=intersect(clip,*local);}
    const auto belongs=[&](std::size_t target){return target!=absent&&impl_->descendant(target,i);};
    return WidgetInteraction{bounds,clip,{area.width,area.height},impl_->measure(i),true,impl_->logical_visible(i),impl_->effective_enabled(i),belongs(impl_->hovered),belongs(impl_->pressed),belongs(impl_->focused),impl_->nodes[i].popup&&impl_->nodes[i].popup->opened,impl_->blocked(i)};
}
core::Result<Size> WidgetState::measure(std::size_t i,double width){
    if(auto r=ready();!r)return std::unexpected(r.error());
    if(i>=impl_->nodes.size()||!std::isfinite(width)||width<=0||width>8192)return core::fail(core::ErrorCode::invalid_argument,"Invalid native width constraint");
    try{auto measured=impl_->measure_width(i,width);measured.width=std::min(measured.width,width);return measured;}catch(const std::bad_alloc&){return core::fail(core::ErrorCode::full,"Native measurement allocation failed");}catch(...){return core::fail(core::ErrorCode::failed,"Native measurement failed",std::current_exception());}
}
core::Result<void> WidgetState::observe_interaction(std::size_t i,std::shared_ptr<const WidgetInteractionPort> port){
    if(auto r=ready();!r)return r;
    if(i>=impl_->nodes.size()||!port)return core::fail(core::ErrorCode::invalid_argument,"Interaction observer requires a live target and receiver");
    std::erase_if(impl_->interaction_watches,[](const auto& watch){return watch->port.expired();});
    for(const auto& watch:impl_->interaction_watches)if(watch->node==i&&watch->port.lock()==port)return {};
    if(impl_->interaction_watches.size()>=1024)return core::fail(core::ErrorCode::full,"Interaction observer limit is 1024 per generation");
    try{impl_->interaction_watches.push_back(std::make_shared<Impl::InteractionWatch>(Impl::InteractionWatch{i,port,{}}));}
    catch(const std::bad_alloc&){return core::fail(core::ErrorCode::full,"Interaction observer allocation failed");}
    impl_->invalidate();return {};
}
core::Result<void> WidgetState::flush_interactions(){
    if(!impl_->active||impl_->building||impl_->notifying_interactions||!impl_->interactions_dirty||impl_->interaction_watches.empty())return {};
    impl_->notifying_interactions=true;struct Reset{bool& value;~Reset(){value=false;}}reset{impl_->notifying_interactions};
    bool failed=false;std::exception_ptr failure;
    for(unsigned pass=0;pass<8;++pass){
        impl_->interactions_dirty=false;
        const auto targets=impl_->interaction_watches;
        for(const auto& watch:targets){
            if(!impl_->active)return {};
            auto port=watch->port.lock();if(!port)continue;
            auto state=interaction(watch->node);if(!state)return std::unexpected(state.error());
            if(watch->delivered&&*watch->delivered==*state)continue;watch->delivered=*state;
            const auto sent=port->send(*state);if(!sent&&sent.error().code!=runtime::QueryErrorCode::receiver_expired){if(!failed)failure=sent.error().cause;failed=true;}
        }
        if(!impl_->active)return {};
        std::erase_if(impl_->interaction_watches,[](const auto& watch){return watch->port.expired();});
        if(!impl_->interactions_dirty){if(failed)return core::fail(core::ErrorCode::failed,"Committed interaction observer failed",failure);return {};}
    }
    return core::fail(core::ErrorCode::busy,"Interaction observers did not settle after eight passes");
}
core::Result<WidgetPlacement> WidgetState::placement(std::size_t i)const{
    if(auto r=ready();!r)return std::unexpected(r.error());if(i>=impl_->nodes.size())return core::fail(core::ErrorCode::not_found,"Widget was not found");
    const auto parent=impl_->nodes[i].parent;std::size_t sibling=0;
    if(parent!=absent){const auto& siblings=impl_->nodes[parent].children;sibling=std::size_t(std::find(siblings.begin(),siblings.end(),i)-siblings.begin());}
    return WidgetPlacement{shared_from_this(),parent==absent?std::uint64_t(-1):std::uint64_t(parent),std::uint64_t(sibling),impl_->domain};
}
core::Result<WidgetSnapshot> WidgetState::snapshot(std::size_t i) {
    if (auto r=ready();!r) return std::unexpected(r.error());
    if (i>=impl_->nodes.size()) return core::fail(core::ErrorCode::not_found,"Widget was not found");
    if(auto parent=impl_->embedding_owner.lock())parent->impl_->layout(parent->impl_->surface?parent->impl_->surface->size():parent->impl_->viewport);
    impl_->layout(impl_->surface?impl_->surface->size():impl_->viewport);
    if(auto r=flush_viewports();!r)return std::unexpected(r.error());
    if(auto r=ready();!r)return std::unexpected(r.error());
    const auto& n=impl_->nodes[i]; auto run=impl_->text_run(n);
    constexpr const char* names[]{"VStack","HStack","Label","Button","ZStack","Spacer","CheckBox","RadioButton","ToggleSwitch","InputField","Slider","ProgressBar","TabBar","Tab","ComboBox","Stepper","SegmentedControl","Menu","MenuItem","MenuDivider","ColorPickerSurface","AbsoluteStack","Image","Popover","ScrollView","ContentHost","ActionSurface"};
    static_assert(std::size(names)==static_cast<unsigned>(WidgetKind::action_surface)+1);
    std::size_t segment_count=0;
    if(n.kind==WidgetKind::segmented_control) for(auto child:n.children) {
        const auto k=impl_->nodes[child].kind;if(k==WidgetKind::button || k==WidgetKind::stepper || k==WidgetKind::combo_box) ++segment_count;
    }
    auto result=WidgetSnapshot{.kind=n.kind,.id=n.id,.text=n.text,.native_class=std::string("iiGUI.")+names[static_cast<unsigned>(n.kind)],.size={n.bounds.width,n.bounds.height},
        .enabled=impl_->effective_enabled(i),.attached=impl_->presentation_surface()!=nullptr,.styled=impl_->appearance.styled,.font_family=n.menu_item && *n.menu_item->compact?impl_->resolved_compact_font_family:impl_->resolved_font_family,.font_size=run.font_size,.bounds=n.bounds,
        .hovered=impl_->hovered==i,.pressed=impl_->pressed==i,.focused=impl_->focused==i&&!(impl_->embedded_view(i)&&impl_->embedded_view(i)->impl_->focused!=absent),.checked=n.checked,.read_only=n.field.read_only,
        .value=n.kind==WidgetKind::slider?n.slider.value:n.progress.value,.minimum=n.kind==WidgetKind::slider?n.slider.minimum:n.progress.minimum,
        .maximum=n.kind==WidgetKind::slider?n.slider.maximum:n.progress.maximum,.placeholder=n.field.placeholder,.renderer="iiGUI",
        .icon_name=n.menu_item?n.menu_item->icon_name:n.kind==WidgetKind::tab?n.tab.icon_name:n.button.icon_name,.accessibility_name=n.popup&&n.popup->popover?n.popup->popover->accessibility_name:n.color?n.color->options.accessibility_name:n.kind==WidgetKind::stepper?n.stepper.accessibility_name:n.kind==WidgetKind::input_field?n.field.accessibility_name:n.kind==WidgetKind::slider?n.slider.accessibility_name:n.kind==WidgetKind::image?n.image.accessibility_name:n.button.accessibility_name,
        .selected=n.menu_item?impl_->item_selected(i):n.tab.selected,.current_index=n.popup?n.popup->options.selected_index:n.kind==WidgetKind::tab_bar?n.tabbar.current_index:-1,
        .tone=n.kind==WidgetKind::combo_box?n.combo.tone:n.kind==WidgetKind::stepper?n.stepper.tone:n.button.tone,
        .arrow=n.kind==WidgetKind::combo_box?n.combo.arrow:n.stepper.arrow,.segment_count=segment_count,
        .visible=impl_->visible(i),.opened=n.popup && n.popup->opened,.input_blocked=impl_->blocked(i),.compact=n.menu_item && *n.menu_item->compact,
        .menu_index=n.menu_index,.menu_count=n.popup?n.popup->entries.size():0,.parent_id=n.parent==absent?std::string{}:impl_->nodes[n.parent].id,
        .popup_id=impl_->owning_popup(i)==absent?std::string{}:impl_->nodes[impl_->owning_popup(i)].id,.menu_state=n.menu_item?n.menu_item->state:MenuItemState::normal,
        .horizontal_direction=n.popup?n.popup->horizontal:MenuDirection::right,.vertical_direction=n.popup?n.popup->vertical:MenuDirection::down,
        .scroll_offset=n.popup?n.popup->scroll:n.scroll?n.scroll->offset:0,.content_height=n.popup?n.popup->content_height:n.scroll?n.scroll->content_height:0,
        .clip_bounds=impl_->clip_rect(i),
        .has_context_menu=n.context_menu!=absent,.color_state=n.color?std::optional{*n.color->options.model->state()}:n.color_button?std::optional{*n.color_button->options.model->state()}:std::nullopt,
        .color_field=n.color?n.color->options.field:ColorField::wheel,.color_domain_rasterizations=n.color?n.color->rasterizations:0,
        .implicit_size=impl_->measure(i),.modal=n.popup&&n.popup->popover&&n.popup->popover->modal,.dim=n.popup&&n.popup->popover&&n.popup->popover->dim,
        .color_button=bool(n.color_button),.show_hue_ring=n.color_button&&n.color_button->options.show_hue_ring,.color_button_rasterizations=n.color_button?n.color_button->rasterizations:0,
        .visual_bounds=impl_->visual_bounds(i)};
    if(n.scroll){result.horizontal_scroll_offset=n.scroll->horizontal_offset;result.content_width=n.scroll->content_width;}
    if(n.content){result.has_content=bool(n.content->view);result.content_generation=n.content->epoch;}
    result.child_count=n.children.size();
    if(n.action_surface){result.accessibility_description=n.action_surface->accessibility_description;result.action_role=n.action_surface->role;result.custom_content=impl_->custom_action_content(n);result.text_underline=n.action_surface->underline;result.foreground_color=impl_->action_ink(n);result.hovered=impl_->hovered!=absent&&impl_->descendant(impl_->hovered,i);}
    if(n.kind==WidgetKind::input_field){
        result.password=n.field.password;
        const auto first=std::min(n.anchor,n.cursor),last=std::max(n.anchor,n.cursor);
        result.text_selection={utf16_count(std::string_view(n.text).substr(0,first)),utf16_count(std::string_view(n.text).substr(first,last-first))};
        if(!n.marked.empty()){
            result.marked_range=TextRange{result.text_selection.location,utf16_count(n.marked)};
            result.text_selection={result.marked_range->location+n.marked_selection.location,n.marked_selection.length};result.composition_text=n.marked;
        }
    }
    if(n.text_input||n.kind==WidgetKind::input_field){
        auto input=text_input_state(i);if(!input)return std::unexpected(input.error());
        result.text_input=true;result.multiline=input->multiline;result.text_length=input->length;result.read_only=input->read_only;
        result.text_selection=input->selection;result.marked_range=input->marked;
        result.caret_bounds=input->caret;result.caret_bounds.x+=n.bounds.x;result.caret_bounds.y+=n.bounds.y;
        if(auto popup=impl_->owning_popup(i);popup!=absent)result.caret_bounds=transform_rect(result.caret_bounds,impl_->transform(popup));
    }
    if(n.drawing){result.drawing_panels=n.drawing->panels.size();result.drawing_texts=n.drawing->texts.size();result.drawing_images=n.drawing->images.size();}
    if(auto p=impl_->owning_popup(i);p!=absent){const auto& popup=*impl_->nodes[p].popup;result.reveal_progress=popup.motion.progress;result.transitioning=popup.motion.active||popup.snap_active;result.exiting=!popup.opened&&popup.motion.active;result.enabled&=popup.opened;}
    result.visual_clip_bounds=result.clip_bounds;if(result.visual_clip_bounds){auto popup=impl_->owning_popup(i);if(popup!=absent)result.visual_clip_bounds=transform_rect(*result.visual_clip_bounds,impl_->transform(popup));}
    if(!impl_->scope_prefix.empty()){
        const auto offset=global_offset();result.bounds.x+=float(offset.width);result.bounds.y+=float(offset.height);result.visual_bounds.x+=float(offset.width);result.visual_bounds.y+=float(offset.height);
        if(result.text_input){result.caret_bounds.x+=float(offset.width);result.caret_bounds.y+=float(offset.height);}
        if(result.clip_bounds){result.clip_bounds->x+=float(offset.width);result.clip_bounds->y+=float(offset.height);}
        if(result.visual_clip_bounds){result.visual_clip_bounds->x+=float(offset.width);result.visual_clip_bounds->y+=float(offset.height);}
        result.id=impl_->scope_prefix+result.id;if(!result.parent_id.empty())result.parent_id=impl_->scope_prefix+result.parent_id;
        if(!result.popup_id.empty())result.popup_id=impl_->scope_prefix+result.popup_id;
        else if(auto parent=impl_->embedding_owner.lock()){
            const auto index=impl_->embedding_popup;const auto popup=parent->impl_->nodes[index].popup?index:parent->impl_->owning_popup(index);
            if(popup!=absent)result.popup_id=parent->impl_->scope_prefix+parent->impl_->nodes[popup].id;
        }
        if(auto parent=impl_->embedding_owner.lock()){
            const auto index=impl_->embedding_popup;const auto& owner=parent->impl_->nodes[index];
            if(result.parent_id.empty()){
                const auto anchor=owner.popup?owner.popup->content_anchor:absent;
                result.parent_id=parent->impl_->scope_prefix+parent->impl_->nodes[anchor==absent?index:anchor].id;
            }
            result.enabled&=owner.enabled;result.input_blocked|=parent->impl_->blocked(index)||!owner.enabled;
            if(impl_->owning_popup(i)==absent){
                auto current=shared_from_this();
                while(auto ancestor=current->impl_->embedding_owner.lock()){
                    const auto index=current->impl_->embedding_popup;const auto popup=ancestor->impl_->nodes[index].popup?index:ancestor->impl_->owning_popup(index);
                    if(popup!=absent){const auto& p=*ancestor->impl_->nodes[popup].popup;auto transform=ancestor->impl_->transform(popup);auto offset=ancestor->global_offset();transform.origin_x+=float(offset.width);transform.origin_y+=float(offset.height);
                        result.visual_bounds=transform_rect(result.visual_bounds,transform);if(result.visual_clip_bounds)result.visual_clip_bounds=transform_rect(*result.visual_clip_bounds,transform);if(result.text_input)result.caret_bounds=transform_rect(result.caret_bounds,transform);
                        result.reveal_progress=p.motion.progress;result.transitioning=p.motion.active;result.exiting=!p.opened&&p.motion.active;result.enabled&=p.opened;break;
                    }
                    current=ancestor;
                }
            }
        }
    }return result;
}
Size WidgetState::global_offset() const {
    if(auto parent=impl_->embedding_owner.lock();parent&&parent->impl_->active&&impl_->embedding_popup<parent->impl_->nodes.size()){
        auto offset=parent->global_offset();const auto body=parent->impl_->activation_rect(impl_->embedding_popup);return {offset.width+body.x,offset.height+body.y};
    }return {};
}
core::Result<std::vector<WidgetSnapshot>> WidgetState::snapshots() {
    if (auto r=ready();!r) return std::unexpected(r.error());
    std::vector<WidgetSnapshot> out; out.reserve(impl_->nodes.size()); for(std::size_t i=0;i<impl_->nodes.size();++i){auto item=snapshot(i);if(!item)return std::unexpected(item.error());out.push_back(std::move(*item));}
    for(const auto& n:impl_->nodes)if(auto child=n.content?n.content->view:n.popup?n.popup->content_view:nullptr){auto children=child->snapshots();if(!children)return std::unexpected(children.error());out.insert(out.end(),std::make_move_iterator(children->begin()),std::make_move_iterator(children->end()));}return out;
}
core::Result<void> WidgetState::fixed_frame(std::size_t i,Rect r) {
    if(auto result=ready();!result)return result;
    if(i>=impl_->nodes.size() || impl_->nodes[i].parent==absent || impl_->nodes[impl_->nodes[i].parent].kind!=WidgetKind::absolute_stack ||
       !geometry(r.x) || !geometry(r.y) || !geometry(r.width) || !geometry(r.height) || !geometry(r.x+r.width) || !geometry(r.y+r.height))return core::fail(core::ErrorCode::invalid_argument,"Fixed frame requires a direct absolute-stack child and finite nonnegative geometry");
    auto& n=impl_->nodes[i];if(n.fixed_frame==r)return {};n.fixed_frame=r;impl_->invalidate(true,i);return {};
}
core::Result<void> WidgetState::extent(std::size_t i,Size size) {
    if(auto r=ready();!r)return r;
    if(i>=impl_->nodes.size() || (!stack_kind(impl_->nodes[i].kind)&&!impl_->nodes[i].content) || !geometry(size.width) || !geometry(size.height))return core::fail(core::ErrorCode::invalid_argument,"Extent requires a stack with finite nonnegative dimensions");
    auto& n=impl_->nodes[i];if(n.extent==size)return {};n.extent=size;
    if(n.fixed_frame){n.fixed_frame->width=float(size.width);n.fixed_frame->height=float(size.height);}
    impl_->invalidate(true,i);return {};
}
core::Result<void> WidgetState::flex(std::size_t i,double value) {
    if(auto r=ready();!r)return r;
    if(i>=impl_->nodes.size()||!geometry(value)||value>4096)return core::fail(core::ErrorCode::invalid_argument,"Flex requires a finite nonnegative weight up to 4096");
    auto& n=impl_->nodes[i];if(n.flex==value)return {};n.flex=value;impl_->invalidate(true,i);return {};
}
core::Result<void> WidgetState::visibility(std::size_t i,bool shown) {
    if(auto r=ready();!r)return r;
    if(i>=impl_->nodes.size())return core::fail(core::ErrorCode::not_found,"Widget was not found");
    auto& n=impl_->nodes[i];if(n.shown==shown)return {};n.shown=shown;
    auto input_cancel=!shown&&impl_->input_capture!=absent&&impl_->descendant(impl_->input_capture,i)?impl_->cancel_input_capture():runtime::Task{};
    auto blur=!shown && impl_->focused!=absent && impl_->descendant(impl_->focused,i)?impl_->change_focus(absent):runtime::Task{};
    auto canceled=!shown && impl_->pressed!=absent && impl_->descendant(impl_->pressed,i)?impl_->nodes[impl_->pressed].events.canceled:runtime::Task{};
    if(!shown){if(impl_->pressed!=absent && impl_->descendant(impl_->pressed,i))impl_->pressed=absent;if(impl_->hovered!=absent && impl_->descendant(impl_->hovered,i))impl_->hovered=absent;}
    std::vector<runtime::Task> closed;
    if(!shown){auto opened=impl_->popups;for(auto p:opened)if(impl_->nodes[p].popup->opened && impl_->descendant(p,i)){auto callbacks=impl_->end_popup(p,false);for(auto& c:callbacks)closed.push_back(std::move(c));}}
    if(!shown){for(auto p:impl_->visual_popups)if(impl_->descendant(p,i)){auto& popup=*impl_->nodes[p].popup;popup.motion.active=false;popup.motion.progress=0;}std::erase_if(impl_->visual_popups,[&](auto p){return !impl_->nodes[p].popup->opened&&!impl_->nodes[p].popup->motion.active;});impl_->motion_activity();}
    impl_->invalidate(true,i);impl_->accessibility_changed();auto invoke=impl_->invoke;
    if(input_cancel)invoke(std::move(input_cancel));if(impl_->active&&canceled)invoke(std::move(canceled));if(impl_->active&&blur)invoke(std::move(blur));for(auto& c:closed){if(!impl_->active)break;invoke(std::move(c));}return {};
}
core::Result<void> WidgetState::scroll_options(std::size_t i,ScrollOptions value){
    if(auto r=ready();!r)return r;
    if(i>=impl_->nodes.size()||!impl_->nodes[i].scroll||!valid_scroll(value))return core::fail(core::ErrorCode::invalid_argument,"Invalid scroll options");
    auto& view=*impl_->nodes[i].scroll;if(view.options==value)return {};
    if(view.notifying)return core::fail(core::ErrorCode::busy,"Scroll options cannot change from this viewport's callback");
    view.options=value;impl_->invalidate(true,i);return flush_viewports();
}
core::Result<void> WidgetState::scroll_offset(std::size_t i,double value){
    if(auto r=ready();!r)return r;
    if(i>=impl_->nodes.size()||!impl_->nodes[i].scroll)return core::fail(core::ErrorCode::invalid_argument,"Scroll offset requires a viewport");
    return scroll_position(i,impl_->nodes[i].scroll->horizontal_offset,value);
}
core::Result<void> WidgetState::horizontal_scroll_offset(std::size_t i,double value){
    if(auto r=ready();!r)return r;
    if(i>=impl_->nodes.size()||!impl_->nodes[i].scroll)return core::fail(core::ErrorCode::invalid_argument,"Scroll offset requires a viewport");
    return scroll_position(i,value,impl_->nodes[i].scroll->offset);
}
core::Result<void> WidgetState::scroll_position(std::size_t i,double x,double y){
    if(auto r=ready();!r)return r;
    if(i>=impl_->nodes.size()||!impl_->nodes[i].scroll||!std::isfinite(x)||!std::isfinite(y))return core::fail(core::ErrorCode::invalid_argument,"Invalid scroll position");
    auto& view=*impl_->nodes[i].scroll;
    if(view.notifying&&(y!=view.offset||x!=view.horizontal_offset))return core::fail(core::ErrorCode::busy,"Scroll position cannot change from this viewport's callback");
    impl_->layout(impl_->surface?impl_->surface->size():impl_->viewport);
    const auto bounds=impl_->nodes[i].bounds;
    x=std::clamp(x,0.,std::max(0.,view.content_width-bounds.width));y=std::clamp(y,0.,std::max(0.,view.content_height-bounds.height));
    if(y!=view.offset||x!=view.horizontal_offset){view.offset=y;view.horizontal_offset=x;impl_->hovered=absent;impl_->invalidate(true);impl_->accessibility_changed();}
    return flush_viewports();
}
core::Result<void> WidgetState::scroll_handler(std::size_t i,std::function<void(const ScrollViewport&)> handler){
    if(auto r=ready();!r)return r;
    if(i>=impl_->nodes.size()||!impl_->nodes[i].scroll)return core::fail(core::ErrorCode::invalid_argument,"Scroll handler requires a viewport");
    auto& view=*impl_->nodes[i].scroll;view.changed=std::move(handler);view.delivered.reset();impl_->invalidate(true,i);return {};
}
core::Result<void> WidgetState::flush_viewports(){
    if(!impl_->active)return core::fail(core::ErrorCode::closed,"Widget tree closed during viewport delivery");
    if(impl_->building||impl_->notifying_viewports)return {};
    impl_->notifying_viewports=true;
    struct Reset final{bool& value;~Reset(){value=false;}} reset{impl_->notifying_viewports};
    for(unsigned pass=0;pass<8;++pass){
        impl_->layout(impl_->surface?impl_->surface->size():impl_->viewport);
        const auto revision=impl_->layout_revision;
        const bool inherited_interaction_change=impl_->interactions_dirty;
        for(std::size_t i=0;i<impl_->nodes.size();++i){
            auto child=impl_->nodes[i].content?impl_->nodes[i].content->view:impl_->nodes[i].popup?impl_->nodes[i].popup->content_view:nullptr;
            if(!child)continue;
            child->impl_->interactions_dirty|=inherited_interaction_change;
            auto delivered=child->flush_viewports();
            if(!impl_->active)return core::fail(core::ErrorCode::closed,"Embedded callback retired its widget domain");
            if(!delivered&&delivered.error().code!=core::ErrorCode::closed)return delivered;
        }
        if(impl_->layout_dirty||impl_->layout_revision!=revision)continue;
        if(impl_->delivered_layout_revision==revision){
            if(auto r=flush_interactions();!r)return r;
            if(!impl_->active)return core::fail(core::ErrorCode::closed,"Interaction callback retired its widget domain");
            if(!impl_->layout_dirty&&impl_->layout_revision==revision)return {};
            continue;
        }
        for(std::size_t i=0;i<impl_->nodes.size();++i){
            if(impl_->nodes[i].layout_changed&&impl_->logical_visible(i)&&impl_->nodes[i].delivered_bounds!=impl_->nodes[i].bounds){
                const auto bounds=impl_->nodes[i].bounds;auto callback=impl_->nodes[i].layout_changed;impl_->nodes[i].delivered_bounds=bounds;bool completed=false;auto invoke=impl_->invoke;
                try{invoke([&]{callback(bounds);completed=true;});}catch(...){return core::fail(core::ErrorCode::failed,"Layout callback failed",std::current_exception());}
                if(!impl_->active)return core::fail(core::ErrorCode::closed,"Layout callback retired its widget tree");
                if(!completed)return core::fail(core::ErrorCode::failed,"Layout callback did not complete");
            }
            auto* view=impl_->nodes[i].scroll.get();if(!view||!view->changed||!impl_->logical_visible(i))continue;
            ScrollViewport current{impl_->nodes[i].bounds,view->offset,view->content_height,view->horizontal_offset,view->content_width};
            if(view->delivered==current)continue;
            auto handler=view->changed;view->delivered=current;view->notifying=true;bool completed=false;
            auto invoke=impl_->invoke;
            try{invoke([&]{handler(current);completed=true;});}catch(...){if(impl_->active)impl_->nodes[i].scroll->notifying=false;return core::fail(core::ErrorCode::failed,"Viewport callback failed",std::current_exception());}
            if(!impl_->active)return core::fail(core::ErrorCode::closed,"Viewport callback retired its widget tree");
            impl_->nodes[i].scroll->notifying=false;
            if(!completed)return core::fail(core::ErrorCode::failed,"Viewport callback failed after scroll was committed");
        }
        if(!impl_->layout_dirty&&impl_->layout_revision==revision){
            impl_->delivered_layout_revision=revision;
            if(auto r=flush_interactions();!r)return r;
            if(!impl_->active)return core::fail(core::ErrorCode::closed,"Interaction callback retired its widget domain");
            if(!impl_->layout_dirty&&impl_->layout_revision==revision)return {};
        }
    }
    return core::fail(core::ErrorCode::busy,"Viewport callbacks did not settle after eight layout passes");
}
core::Result<void> WidgetState::layout_handler(std::size_t i,std::function<void(Rect)> handler){
    if(auto result=ready();!result)return result;if(i>=impl_->nodes.size())return core::fail(core::ErrorCode::not_found,"Layout owner not found");
    impl_->nodes[i].layout_changed=std::move(handler);impl_->nodes[i].delivered_bounds.reset();impl_->invalidate(true,i);return {};
}
core::Result<void> WidgetState::drawing(std::size_t i,WidgetDrawing value){
    if(auto r=ready();!r)return r;
    if(i>=impl_->nodes.size()||!stack_kind(impl_->nodes[i].kind))return core::fail(core::ErrorCode::invalid_argument,"Retained drawing requires a stack");
    CanvasFrame validation{.viewport={0,0,8192,8192},.overlays={{.panels=value.panels,.texts=value.texts,.images=value.images,.backdrop=value.backdrop,.shadow=value.shadow}}};
    if(!valid_canvas(validation))return core::fail(core::ErrorCode::invalid_argument,"Invalid retained widget drawing");
    auto& current=impl_->nodes[i].drawing;if(current&&*current==value)return {};
    current=std::move(value);impl_->nodes[i].paint_handler={};impl_->invalidate();return {};
}
core::Result<void> WidgetState::paint_handler(std::size_t i,WidgetPaintHandler callback,Widget source){
    if(auto r=ready();!r)return r;if(i>=impl_->nodes.size()||(!stack_kind(impl_->nodes[i].kind)&&!impl_->nodes[i].action_surface))return core::fail(core::ErrorCode::invalid_argument,"Paint provider requires a container");
    auto owner=i;if(auto state=source.state_.lock()){if(state.get()!=this||source.index_>=impl_->nodes.size()||!impl_->descendant(i,source.index_))return core::fail(core::ErrorCode::invalid_argument,"Paint interaction owner must be an ancestor in the same generation");owner=source.index_;}else if(source.owner_!=std::thread::id{})return core::fail(core::ErrorCode::closed,"Paint interaction owner retired");
    auto& n=impl_->nodes[i];n.paint_handler=std::move(callback);n.paint_source=owner;n.drawing.reset();impl_->invalidate();return {};
}
core::Result<void> WidgetState::repaint(std::size_t i){if(auto r=ready();!r)return r;if(i>=impl_->nodes.size())return core::fail(core::ErrorCode::not_found,"Paint owner not found");impl_->invalidate();return {};}
core::Result<void> WidgetState::input_handler(std::size_t i,WidgetInputHandler handler){
    if(auto r=ready();!r)return r;
    if(i>=impl_->nodes.size()||(!stack_kind(impl_->nodes[i].kind)&&!impl_->nodes[i].content))return core::fail(core::ErrorCode::invalid_argument,"Input interception requires a stack or content host");
    auto canceled=impl_->input_capture==i?impl_->cancel_input_capture():runtime::Task{};
    std::erase(impl_->input_hover_path,i);impl_->nodes[i].input_handler=std::move(handler);impl_->accessibility_changed();
    if(canceled){auto invoke=impl_->invoke;invoke(std::move(canceled));}return {};
}
core::Result<void> WidgetState::stack_options(std::size_t i,StackOptions o){if(auto r=ready();!r)return r;if(i>=impl_->nodes.size()||(impl_->nodes[i].kind!=WidgetKind::vertical_stack&&impl_->nodes[i].kind!=WidgetKind::horizontal_stack&&impl_->nodes[i].kind!=WidgetKind::overlay_stack)||!valid_stack(o))return core::fail(core::ErrorCode::invalid_argument,"Stack options require a valid ordinary stack");if(impl_->nodes[i].stack==o)return {};impl_->nodes[i].stack=std::move(o);impl_->invalidate(true,i);return {};}
core::Result<void> WidgetState::button_style(std::size_t i,ButtonStyle o){if(auto r=ready();!r)return r;if(i>=impl_->nodes.size()||impl_->nodes[i].kind!=WidgetKind::button||impl_->nodes[i].color_button||!valid_style(o))return core::fail(core::ErrorCode::invalid_argument,"Style requires an ordinary button and valid colors");impl_->nodes[i].button.style=std::move(o);impl_->invalidate();return {};}
core::Result<void> WidgetState::button_icon(std::size_t i,std::shared_ptr<const RasterImage> image,std::string_view name){
    if(auto r=ready();!r)return r;
    if(i>=impl_->nodes.size()||impl_->nodes[i].kind!=WidgetKind::button||impl_->nodes[i].color_button||!image||!valid_title(name)||name.size()>65536)return core::fail(core::ErrorCode::invalid_argument,"Icon requires an ordinary button and valid image/name");
    auto& button=impl_->nodes[i].button;
    if(button.icon==image&&button.icon_name==name)return {};
    button.icon=std::move(image);button.icon_name=name;impl_->invalidate(true,i);return {};
}
core::Result<void> WidgetState::control_events(std::size_t i,ControlEvents o){if(auto r=ready();!r)return r;if(i>=impl_->nodes.size()||!interactive(impl_->nodes[i].kind))return core::fail(core::ErrorCode::invalid_argument,"Control events require an interactive widget");std::swap(impl_->nodes[i].events,o);return {};}
core::Result<void> WidgetState::image_options(std::size_t i,ImageOptions o){if(auto r=ready();!r)return r;if(i>=impl_->nodes.size()||impl_->nodes[i].kind!=WidgetKind::image||!valid_image_options(o))return core::fail(core::ErrorCode::invalid_argument,"Invalid image options");impl_->nodes[i].image=std::move(o);impl_->invalidate(true,i);return {};}
core::Result<void> WidgetState::popover_options(std::size_t i,PopoverOptions o){
    if(auto r=ready();!r)return r;
    if(i>=impl_->nodes.size()||!impl_->nodes[i].popup||!impl_->nodes[i].popup->popover||!valid_popover_options(o)||!valid_transition(o.transition))return core::fail(core::ErrorCode::invalid_argument,"Invalid popover options");
    auto& popup=*impl_->nodes[i].popup;
    if(popup.content_factory&&popup.content_anchor==absent&&o.transition&&o.transition->enabled&&(o.transition->enter_duration>runtime::Clock::duration::zero()||o.transition->exit_duration>runtime::Clock::duration::zero()))return core::fail(core::ErrorCode::unsupported,"Animated popovers currently require static native content");
    if(auto result=prepare_motion(o);!result)return result;
    Impl::configure_popup(popup,std::move(o));
    if(popup.motion.active||popup.snap_active){if(!popup.popover->transition){popup.motion.active=popup.snap_active=false;popup.drag_offset=0;popup.motion.progress=popup.opened?1.:0.;}else impl_->advance_motion(runtime::Clock::now());}
    std::erase_if(impl_->visual_popups,[&](auto n){return !impl_->nodes[n].popup->opened&&!impl_->nodes[n].popup->motion.active;});impl_->motion_activity();
    impl_->invalidate(true,i);return {};
}
core::Result<void> WidgetState::image(std::size_t i,std::shared_ptr<const RasterImage> image) {
    if(auto r=ready();!r)return r;
    if(i>=impl_->nodes.size() || !image)return core::fail(core::ErrorCode::invalid_argument,"Invalid image mutation");
    auto& n=impl_->nodes[i];auto* target=n.kind==WidgetKind::image?&n.image.image:n.kind==WidgetKind::button?&n.button.background_image:n.kind==WidgetKind::slider?&n.slider.background_image:nullptr;
    if(!target)return core::fail(core::ErrorCode::invalid_argument,"Widget has no image property");
    if(*target==image)return {};*target=std::move(image);impl_->invalidate();return {};
}
core::Result<void> WidgetState::escape_action(std::size_t i,runtime::Task action) {
    if(auto r=ready();!r)return r;
    if(i>=impl_->nodes.size())return core::fail(core::ErrorCode::not_found,"Widget was not found");impl_->nodes[i].escape=std::move(action);return {};
}
core::Result<void> WidgetState::text(std::size_t i,std::string_view value) {
    if (auto r=ready();!r) return r;
    if (i>=impl_->nodes.size() || !valid_title(value) || value.size()>65536 || (impl_->nodes[i].kind!=WidgetKind::label && impl_->nodes[i].kind!=WidgetKind::button && !impl_->nodes[i].action_surface && !check_kind(impl_->nodes[i].kind) && impl_->nodes[i].kind!=WidgetKind::input_field && impl_->nodes[i].kind!=WidgetKind::tab && impl_->nodes[i].kind!=WidgetKind::combo_box && impl_->nodes[i].kind!=WidgetKind::menu_item)) return core::fail(core::ErrorCode::invalid_argument,"Invalid widget text mutation");
    auto& n=impl_->nodes[i];if(n.kind==WidgetKind::input_field && n.field.maximum_length)value=value.substr(0,scalar_prefix(value,n.field.maximum_length));if(n.text==value)return {};n.text=value;if(n.action_surface)n.action_surface->text=value;n.cursor=n.anchor=n.text.size();impl_->invalidate(true,i);return {};
}
core::Result<void> WidgetState::label_options(std::size_t i,LabelOptions o){if(auto r=ready();!r)return r;if(i>=impl_->nodes.size()||impl_->nodes[i].kind!=WidgetKind::label||!valid_label(o))return core::fail(core::ErrorCode::invalid_argument,"Invalid label options");auto& n=impl_->nodes[i];if(n.label==o&&n.text==o.text)return {};n.text=o.text;n.label=std::move(o);impl_->invalidate(true,i);return {};}
core::Result<void> WidgetState::input_options(std::size_t i,InputOptions o){if(auto r=ready();!r)return r;if(i>=impl_->nodes.size()||impl_->nodes[i].kind!=WidgetKind::input_field||!valid_input(o))return core::fail(core::ErrorCode::invalid_argument,"Invalid input options");if(o.maximum_length)o.text.resize(scalar_prefix(o.text,o.maximum_length));auto& n=impl_->nodes[i];if(n.text!=o.text){n.text=o.text;n.cursor=n.anchor=n.text.size();n.marked.clear();}const bool active=o.enabled;n.field=std::move(o);impl_->invalidate(true,i);return enabled(i,active);}
core::Result<void> WidgetState::clip_content(std::size_t i,bool value){if(auto r=ready();!r)return r;if(i>=impl_->nodes.size()||(!stack_kind(impl_->nodes[i].kind)&&!impl_->nodes[i].action_surface)||impl_->nodes[i].scroll)return core::fail(core::ErrorCode::invalid_argument,"Clip requires an ordinary container");impl_->nodes[i].clip=value;impl_->hovered=absent;impl_->invalidate();impl_->accessibility_changed();return {};}
core::Result<void> WidgetState::accessibility_name(std::size_t i,std::string_view value){if(auto r=ready();!r)return r;if(i>=impl_->nodes.size()||!valid_title(value)||value.size()>65536||(impl_->nodes[i].kind!=WidgetKind::button&&!impl_->nodes[i].action_surface&&impl_->nodes[i].kind!=WidgetKind::input_field&&!impl_->nodes[i].text_input))return core::fail(core::ErrorCode::invalid_argument,"Accessible name requires an action or editor");auto& n=impl_->nodes[i];if(n.kind==WidgetKind::input_field)n.field.accessibility_name=value;else n.button.accessibility_name=value;if(n.action_surface)n.action_surface->accessibility_name=value;impl_->accessibility_changed();return {};}
core::Result<void> WidgetState::enabled(std::size_t i,bool value) {
    if (auto r=ready();!r) return r;
    if (i>=impl_->nodes.size() || (!interactive(impl_->nodes[i].kind)&&!stack_kind(impl_->nodes[i].kind)&&!impl_->nodes[i].content&&impl_->nodes[i].kind!=WidgetKind::image)) return core::fail(core::ErrorCode::invalid_argument,"Widget has no enabled property");
    auto& n=impl_->nodes[i];
    if(n.menu_item) { n.menu_item->enabled=value;value=value && n.menu_item->state!=MenuItemState::inactive; }
    const bool requested=value;if(n.action_surface)value=value&&n.action_surface->interaction_enabled;
    value=value && (n.kind!=WidgetKind::button || n.button.tone!=ButtonTone::disabled) && (n.kind!=WidgetKind::stepper || n.stepper.tone!=ButtonTone::disabled);
    const auto affected=[&](std::size_t target){return target!=absent&&impl_->descendant(target,i);};
    auto canceled=!value&&affected(impl_->pressed)?impl_->nodes[impl_->pressed].events.canceled:runtime::Task{};
    auto capture=!value&&affected(impl_->input_capture)?impl_->cancel_input_capture():runtime::Task{};
    n.enabled=value;if(n.action_surface)n.action_surface->enabled=requested;auto blur=!value&&affected(impl_->focused)?impl_->change_focus(absent):runtime::Task{};if(!value){if(affected(impl_->pressed))impl_->pressed=absent;if(affected(impl_->hovered))impl_->hovered=absent;}
    const auto parent=impl_->nodes[i].parent;if(parent!=absent && impl_->nodes[parent].kind==WidgetKind::tab_bar) impl_->reconcile(parent);
    impl_->invalidate();auto invoke=impl_->invoke;if(capture)invoke(std::move(capture));if(canceled) invoke(std::move(canceled));if(blur)invoke(std::move(blur));return {};
}
core::Result<void> WidgetState::checked(std::size_t i,bool value) {
    if (auto r=ready();!r) return r;
    if (i>=impl_->nodes.size() || (!check_kind(impl_->nodes[i].kind)&&!impl_->nodes[i].action_surface)) return core::fail(core::ErrorCode::invalid_argument,"Widget has no checked property");
    auto& n=impl_->nodes[i]; n.checked=value;
    if (value && n.kind==WidgetKind::radio_button && !n.group.empty()) for(auto& sibling:impl_->nodes) if(&sibling!=&n && sibling.kind==WidgetKind::radio_button && sibling.group==n.group) sibling.checked=false;
    impl_->invalidate(); return {};
}
core::Result<void> WidgetState::value(std::size_t i,double value) {
    if (auto r=ready();!r) return r;
    if (i>=impl_->nodes.size() || !std::isfinite(value)) return core::fail(core::ErrorCode::invalid_argument,"Invalid widget value");
    auto& n=impl_->nodes[i];
    if(n.kind==WidgetKind::slider) n.slider.value=std::clamp(value,n.slider.minimum,n.slider.maximum);
    else if(n.kind==WidgetKind::progress) n.progress.value=std::clamp(value,n.progress.minimum,n.progress.maximum);
    else return core::fail(core::ErrorCode::invalid_argument,"Widget has no numeric value");
    impl_->invalidate(); return {};
}
core::Result<void> WidgetState::focus(std::size_t i) {
    if (auto r=ready();!r) return r;
    if (i>=impl_->nodes.size() || (!interactive(impl_->nodes[i].kind) && !stack_kind(impl_->nodes[i].kind) && !impl_->nodes[i].content && impl_->nodes[i].kind!=WidgetKind::popover) || !impl_->nodes[i].enabled || !impl_->logical_visible(i) || impl_->blocked(i)) return core::fail(core::ErrorCode::invalid_argument,"Widget cannot receive focus");
    if(auto parent=impl_->embedding_owner.lock()){auto r=parent->focus(parent->impl_->content_focus(impl_->embedding_popup));if(!r)return r;if(auto live=ready();!live)return live;}
    auto canceled=impl_->pressed!=absent && impl_->pressed!=i?impl_->nodes[impl_->pressed].events.canceled:runtime::Task{};
    if(impl_->pressed!=i) impl_->pressed=absent;
    const bool changed=impl_->focused!=i;auto blur=impl_->change_focus(i);impl_->reveal(i);impl_->invalidate();
    if(changed&&(impl_->owning_popup(i)!=absent||!impl_->scope_prefix.empty()))impl_->accessibility_changed();
    auto invoke=impl_->invoke;if(canceled) invoke(std::move(canceled));if(blur)invoke(std::move(blur));return {};
}
core::Result<void> WidgetState::selected(std::size_t i,bool value) {
    if(auto r=ready();!r) return r;
    if(i>=impl_->nodes.size() || impl_->nodes[i].kind!=WidgetKind::tab) return core::fail(core::ErrorCode::invalid_argument,"Widget has no selected property");
    auto& n=impl_->nodes[i];
    if(n.parent!=absent && impl_->nodes[n.parent].kind==WidgetKind::tab_bar) {
        const auto& bar=impl_->nodes[n.parent];
        if(value) return current_index(n.parent,int(std::ranges::find(bar.children,i)-bar.children.begin()));
        if(n.tab.selected) return current_index(n.parent,-1);
    }
    n.tab.selected=value;impl_->invalidate();return {};
}
core::Result<void> WidgetState::current_index(std::size_t i,int value) {
    if(auto r=ready();!r) return r;
    if(i>=impl_->nodes.size() || impl_->nodes[i].kind!=WidgetKind::tab_bar) return core::fail(core::ErrorCode::invalid_argument,"Widget has no current index");
    const auto& n=impl_->nodes[i];
    if(value < -1 || (value>=0 && (std::size_t(value)>=n.children.size() || !impl_->nodes[n.children[std::size_t(value)]].enabled)))
        return core::fail(core::ErrorCode::out_of_range,"Tab index must name an enabled child, or -1");
    impl_->select(i,value);return {};
}
core::Result<void> WidgetState::activate(std::size_t i) {
    if (auto r=ready();!r) return r;
    if(i>=impl_->nodes.size() || !interactive(impl_->nodes[i].kind)) return core::fail(core::ErrorCode::invalid_argument,"Widget has no action");
    auto& n=impl_->nodes[i]; if (!impl_->effective_enabled(i)) return {};
    if(!impl_->logical_visible(i) || impl_->blocked(i)) return core::fail(core::ErrorCode::invalid_argument,"Widget is hidden or blocked by a popup");
    if(n.kind==WidgetKind::stepper) return step(i,n.stepper.arrow==StepperArrow::down?-1:1);
    if(n.menu_item && n.parent!=absent && impl_->nodes[n.parent].popup && !impl_->nodes[n.parent].popup->popover) {
        const auto parent=n.parent;const auto index=n.menu_index;const auto& popup=*impl_->nodes[parent].popup;
        const auto epoch=popup.epoch;const bool close=n.menu_item->close_on_trigger.value_or(popup.options.auto_close_on_trigger && !(n.menu_item->show_chevron && n.menu_item->has_child_items));
        auto action=n.action;auto triggered=popup.events.triggered;auto invoke=impl_->invoke;auto self=shared_from_this();
        invoke([self,parent,index,epoch,close,action=std::move(action),triggered=std::move(triggered)] {
            if(action) action();if(!self->impl_->active) return;
            if(triggered) triggered(index);if(!self->impl_->active) return;
            if(close && self->impl_->nodes[parent].popup->opened && self->impl_->nodes[parent].popup->epoch==epoch) {
                auto r=self->close_popup(parent);if(!r) throw std::runtime_error(r.error().message);
            }
        });return {};
    }
    if(n.action_surface&&n.action_surface->checkable){n.checked=!n.checked;n.action_surface->checked=n.checked;impl_->invalidate();impl_->accessibility_changed();}
    runtime::Task action=n.action;
    if(n.kind==WidgetKind::tab && n.parent!=absent && impl_->nodes[n.parent].kind==WidgetKind::tab_bar) {
        auto& bar=impl_->nodes[n.parent];
        const auto index=int(std::ranges::find(bar.children,i)-bar.children.begin()),previous=bar.tabbar.current_index;
        if(bar.tabbar.auto_select) {
            impl_->select(n.parent,index);auto changed=bar.selection_changed;
            if(changed && index!=previous) action=[changed,index,action=std::move(action)] { changed(index);if(action) action(); };
        }
    }
    if(check_kind(n.kind)) {
        const bool next=n.kind==WidgetKind::radio_button?true:!n.checked; auto callback=n.toggled;
        auto result=checked(i,next); if(!result) return result;
        if(callback) action=[callback,next] { callback(next); };
    }
    auto invoke=impl_->invoke; if(action) invoke(std::move(action)); return {};
}
core::Result<void> WidgetState::step(std::size_t i,int direction) {
    if(auto r=ready();!r) return r;
    if(i>=impl_->nodes.size() || impl_->nodes[i].kind!=WidgetKind::stepper || (direction!=1 && direction!=-1))
        return core::fail(core::ErrorCode::invalid_argument,"Step requires a Stepper and direction +1 or -1");
    const auto& n=impl_->nodes[i];
    if(!n.enabled || (n.stepper.arrow==StepperArrow::up && direction<0) || (n.stepper.arrow==StepperArrow::down && direction>0)) return {};
    if(!impl_->logical_visible(i) || impl_->blocked(i)) return core::fail(core::ErrorCode::invalid_argument,"Stepper is blocked by a popup");
    auto clicked=n.action;auto stepped=n.stepped;auto invoke=impl_->invoke;
    if(clicked || stepped) invoke([clicked=std::move(clicked),stepped=std::move(stepped),direction] { if(clicked) clicked();if(stepped) stepped(direction); });
    return {};
}
core::Result<void> WidgetState::popup_factory(std::size_t i,WidgetFactory factory,Widget anchor) {
    if(auto r=ready();!r)return r;
    if(i>=impl_->nodes.size()||!impl_->nodes[i].popup||!impl_->nodes[i].popup->popover)return core::fail(core::ErrorCode::invalid_argument,"Activation content requires a Popover");
    auto& popup=*impl_->nodes[i].popup;if(popup.opened||impl_->domain_transition())return core::fail(core::ErrorCode::busy,"Change the content factory only while closed and idle");
    const bool anchored=anchor.owner_!=std::thread::id{};
    if(anchored){
        if(anchor.state_.lock().get()!=this||anchor.index_>=impl_->nodes.size()||!impl_->descendant(anchor.index_,i)||anchor.index_==i)return core::fail(core::ErrorCode::invalid_argument,"Body anchor must be a descendant in this popup generation");
        const auto& node=impl_->nodes[anchor.index_];
        if(node.kind!=WidgetKind::vertical_stack||!node.children.empty()||node.fixed_frame||impl_->owning_popup(anchor.index_)!=i||(node.activation_owner!=absent&&node.activation_owner!=i))return core::fail(core::ErrorCode::invalid_argument,"Body anchor requires an empty, unfixed VStack in this popup");
    }else{
        for(auto child:impl_->nodes[i].children)if(!impl_->nodes[child].popup)return core::fail(core::ErrorCode::invalid_argument,"Use an explicit body anchor when keeping static popup chrome");
        if(factory&&popup.popover->transition&&popup.popover->transition->enabled&&(popup.popover->transition->enter_duration>runtime::Clock::duration::zero()||popup.popover->transition->exit_duration>runtime::Clock::duration::zero()))return core::fail(core::ErrorCode::unsupported,"Animated activation content requires retained static chrome and a body anchor");
    }
    if(popup.content_anchor!=absent){auto& previous=impl_->nodes[popup.content_anchor];previous.activation_owner=absent;previous.shown=false;impl_->invalidate(true,popup.content_anchor);}
    popup.content_anchor=anchored?anchor.index_:absent;
    if(anchored){auto& node=impl_->nodes[anchor.index_];node.activation_owner=i;node.shown=false;impl_->invalidate(true,anchor.index_);}
    popup.content_factory=std::move(factory);impl_->invalidate(true,i);return {};
}
core::Result<Widget> WidgetState::popup_content(std::size_t i,std::string_view id) {
    if(auto r=ready();!r)return std::unexpected(r.error());
    if(i>=impl_->nodes.size()||!impl_->nodes[i].popup)return core::fail(core::ErrorCode::invalid_argument,"Widget is not a popup");
    auto& popup=*impl_->nodes[i].popup;if(!popup.opened)return core::fail(core::ErrorCode::closed,"No content activation is open");
    if(popup.content_view){auto view=popup.content_view;if(id.empty())return Widget(view,0,impl_->owner);auto found=view->find(id);if(found)return found;if(found.error().code!=core::ErrorCode::not_found)return found;}
    if(id.empty()){for(auto child:impl_->nodes[i].children)if(!impl_->nodes[child].popup)return Widget(weak_from_this(),child,impl_->owner);}
    else if(auto item=impl_->ids.find(id);item!=impl_->ids.end()&&item->second!=i&&impl_->owning_popup(item->second)==i)return Widget(weak_from_this(),item->second,impl_->owner);
    return core::fail(core::ErrorCode::not_found,"Popup content id was not found");
}
core::Result<void> WidgetState::create_popup_content(std::size_t i) {
    auto factory=impl_->nodes[i].popup->content_factory;if(!factory||impl_->nodes[i].popup->content_view)return {};
    auto root=shared_from_this();std::size_t depth=0;while(auto parent=root->impl_->embedding_owner.lock()){root=parent;if(++depth>=8)return core::fail(core::ErrorCode::out_of_range,"Content activation depth limit is eight");}
    const auto active_nodes=root->impl_->active_nodes();if(active_nodes>=4096)return core::fail(core::ErrorCode::out_of_range,"Active widget domain limit is 4096");
    auto child=std::make_shared<WidgetState>(impl_->invoke,impl_->appearance,impl_->frame_hooks);child->impl_->embedding_owner=weak_from_this();child->impl_->embedding_popup=i;child->impl_->domain=impl_->domain;child->impl_->node_capacity=4096-active_nodes;
    child->impl_->scope_prefix=impl_->scope_prefix+"@iiscope:"+std::to_string(i)+":"+std::to_string(impl_->nodes[i].popup->epoch+1)+"/";
    child->impl_->resolved_font_family=impl_->resolved_font_family;child->impl_->resolved_compact_font_family=impl_->resolved_compact_font_family;
    struct Guard final {bool& value;bool previous;explicit Guard(bool& v):value(v),previous(v){value=true;}~Guard(){value=previous;}} guard(root->impl_->content_transition);
    const auto previous_staged=impl_->domain->staged;impl_->domain->staged=child;struct StageReset final{Impl::Domain& domain;std::weak_ptr<WidgetState> previous;~StageReset(){domain.staged=previous;}}stage_reset{*impl_->domain,previous_staged};
    WidgetBuilder builder(child);core::Result<void> built;try{built=factory(builder);}catch(...){child->close();throw;}if(!built){child->close();return built;}
    if(!impl_->active){child->close();return core::fail(core::ErrorCode::closed,"Owner retired while constructing popup content");}
    if(child->impl_->nodes.empty()){child->close();return core::fail(core::ErrorCode::invalid_argument,"Content factory must construct one root View");}
    for(std::size_t j=0;j<child->impl_->nodes.size();++j)if(child->impl_->nodes[j].kind==WidgetKind::tab_bar)child->impl_->reconcile(j);
    for(const auto& n:child->impl_->nodes)if(n.popup&&n.popup->options.selected_index>=0&&(std::size_t(n.popup->options.selected_index)>=n.popup->entries.size()||child->impl_->nodes[n.popup->entries[std::size_t(n.popup->options.selected_index)]].kind!=WidgetKind::menu_item)){child->close();return core::fail(core::ErrorCode::out_of_range,"Initial content menu selection must name a menu item");}
    child->impl_->building=false;auto& popup=*impl_->nodes[i].popup;popup.content_view=std::move(child);if(popup.content_anchor!=absent)impl_->nodes[popup.content_anchor].shown=true;impl_->invalidate(true,impl_->content_focus(i));return {};
}
bool WidgetState::has_escape_handler() const {
    if(!impl_->popups.empty())return true;
    if(impl_->focused!=absent&&!impl_->nodes[impl_->focused].marked.empty())return true;
    for(auto p=impl_->focused;p!=absent;p=impl_->nodes[p].parent)if(impl_->nodes[p].escape)return true;return false;
}
core::Result<void> WidgetState::route_host_content(const WidgetInput& event,bool& handled){
    handled=false;
    const bool keyboard=event.kind==WidgetInputKind::key_down||event.kind==WidgetInputKind::key_up||event.kind==WidgetInputKind::text||event.kind==WidgetInputKind::composition;
    const auto scope=impl_->input_popup();const auto modal=impl_->modal_host();
    if(modal==absent&&(impl_->pressed!=absent||impl_->input_capture!=absent)&&event.kind!=WidgetInputKind::focus_lost&&!keyboard)return {};
    if(modal!=absent){if(auto cancel=impl_->cancel_input_capture()){auto invoke=impl_->invoke;invoke(std::move(cancel));if(!impl_->active)return {};}}
    const auto hosts=impl_->ordered_hosts();
    if(event.kind==WidgetInputKind::focus_lost){for(auto i:hosts)if(i!=impl_->focused)if(auto child=impl_->nodes[i].content->view){auto r=child->input(event);if(!r&&r.error().code!=core::ErrorCode::closed)return r;if(!impl_->active)return {};}return {};}
    for(auto it=hosts.rbegin();it!=hosts.rend();++it){
        const auto i=*it;const auto& host=impl_->nodes[i];auto child=host.content->view;
        if(!child||!host.enabled||!impl_->visible(i)||impl_->blocked(i)||impl_->owning_popup(i)!=scope)continue;
        const auto r=host.bounds;const auto point=impl_->logical_point(i,event.x,event.y);
        const bool inside=contains(r,point.width,point.height)&&(!impl_->clip_rect(i)||contains(*impl_->clip_rect(i),point.width,point.height));
        const bool capture=child->impl_->captures_input();
        const auto raw_x=event.x-r.x,raw_y=event.y-r.y;bool in_popup=false;
        for(auto popup:child->impl_->visual_popups)if(contains(child->impl_->visual_bounds(popup),raw_x,raw_y)){in_popup=true;break;}
        const bool child_modal=child->impl_->modal_content();
        if(keyboard){
            if(impl_->focused!=i&&!child_modal)continue;
            if(event.kind==WidgetInputKind::key_down&&event.key==WidgetKey::escape&&!child->has_escape_handler())return {};
            if(event.kind==WidgetInputKind::key_down&&event.key==WidgetKey::tab&&child->impl_->popups.empty()){
                const auto current=child->impl_->focused;const auto direction=event.shift?-1:1;const auto next=child->impl_->next_content(absent,current,direction);
                if(current!=absent&&(next==absent||(direction>0?next<=current:next>=current))){const auto outside=impl_->next_content(scope,i,direction);if(outside!=i){handled=true;return focus_content(outside,direction);}}
            }
        }else if(!capture&&!child_modal&&!in_popup&&(!inside||impl_->hit(event.x,event.y)!=i)){
            if(!child->impl_->popups.empty()&&(event.kind==WidgetInputKind::pointer_down||event.kind==WidgetInputKind::context_request)){auto local=event;local.x=raw_x;local.y=raw_y;auto result=child->input(local);if(!result)return result;if(!impl_->active)return {};}
            if(event.kind==WidgetInputKind::pointer_move&&(child->impl_->hovered!=absent||!child->impl_->input_hover_path.empty())){auto result=child->input({.kind=WidgetInputKind::pointer_leave});if(!result)return result;if(!impl_->active)return {};}
            continue;
        }
        if(!keyboard&&!capture&&!child_modal&&!in_popup){auto intercepted=route_input_handler(event);if(!intercepted)return std::unexpected(intercepted.error());if(*intercepted){handled=true;return {};}if(!impl_->active)return {};}
        auto local=event;local.x=(in_popup||child_modal||!child->impl_->popups.empty())?raw_x:point.width-r.x;local.y=(in_popup||child_modal||!child->impl_->popups.empty())?raw_y:point.height-r.y;
        impl_->hovered=absent;handled=true;return child->input(local);
    }
    return {};
}
core::Result<void> WidgetState::focus_content(std::size_t i,int direction) {
    if(!impl_->active)return {};
    if(auto result=ready();!result)return result;
    if(i>=impl_->nodes.size())return core::fail(core::ErrorCode::invalid_argument,"Focus target was not found");
    auto child=impl_->embedded_view(i);if(!child)return focus(i);
    const auto target=child->impl_->next_content(absent,absent,direction);
    if(target==absent)return focus(i);
    if(auto result=focus(i);!result)return result;
    if(!impl_->active||!child->impl_->active)return {};
    return child->focus_content(target,direction);
}
core::Result<void> WidgetState::route_content(const WidgetInput& event,bool& handled) {
    handled=false;
    if(!impl_->popups.empty()){
        const auto top=impl_->popups.back();const auto& popup=*impl_->nodes[top].popup;
        if(popup.popover&&popup.popover->preserve_input){
            const bool pointer=event.kind==WidgetInputKind::pointer_move||event.kind==WidgetInputKind::pointer_down||event.kind==WidgetInputKind::pointer_up||event.kind==WidgetInputKind::scroll||event.kind==WidgetInputKind::context_request;
            if((pointer&&impl_->popup_contains(top,event.x,event.y))||(!pointer&&impl_->focused!=absent&&impl_->descendant(impl_->focused,top)))return {};
        }
    }
    if(auto r=route_host_content(event,handled);!r||handled)return r;
    const auto top=impl_->input_popup();if(top==absent)return {};
    auto child=impl_->nodes[top].popup->content_view;if(!child)return {};
    const auto& popup=*impl_->nodes[top].popup;const auto r=impl_->activation_rect(top);
    if(impl_->pressed!=absent&&event.kind!=WidgetInputKind::focus_lost&&(event.kind==WidgetInputKind::pointer_move||event.kind==WidgetInputKind::pointer_up||event.kind==WidgetInputKind::pointer_leave))return {};
    const auto raw_x=event.x-r.x,raw_y=event.y-r.y;
    const auto point=impl_->logical_point(top,event.x,event.y);double x=point.width-r.x,y=point.height-r.y;
    const bool keyboard=event.kind==WidgetInputKind::key_down||event.kind==WidgetInputKind::key_up||event.kind==WidgetInputKind::text||event.kind==WidgetInputKind::composition;
    if(event.kind==WidgetInputKind::focus_lost)return child->input(event);
    if(keyboard){
        const auto owner=impl_->content_focus(top);if(impl_->focused!=absent&&impl_->focused!=owner)return {};
        if(event.kind==WidgetInputKind::key_down&&event.key==WidgetKey::escape&&!child->has_escape_handler())return {};
        if(popup.content_anchor!=absent&&child->impl_->popups.empty()&&event.kind==WidgetInputKind::key_down&&event.key==WidgetKey::tab){
            const auto current=child->impl_->focused;const auto direction=event.shift?-1:1;
            const auto next=child->impl_->next_content(absent,current,direction);
            if(current!=absent&&(next==absent||(direction>0?next<=current:next>=current))){
                const auto outside=impl_->next_content(top,popup.content_anchor,direction);
                if(outside!=popup.content_anchor){handled=true;return focus_content(outside,direction);}
            }
        }
    }else {
        bool in_child_popup=false;for(auto p:child->impl_->popups)if(contains(child->impl_->visual_bounds(p),raw_x,raw_y)){in_child_popup=true;break;}
        const bool nested_capture=child->impl_->pressed!=absent&&child->impl_->owning_popup(child->impl_->pressed)!=absent;
        if(in_child_popup||nested_capture||!child->impl_->popups.empty()){x=raw_x;y=raw_y;}
        const bool capture=child->impl_->pressed!=absent&&(event.kind==WidgetInputKind::pointer_move||event.kind==WidgetInputKind::pointer_up||event.kind==WidgetInputKind::pointer_leave);
        if(!contains(r,point.width,point.height)&&!in_child_popup&&!capture&&child->impl_->popups.empty()){
            if(event.kind==WidgetInputKind::pointer_move){auto result=child->input({.kind=WidgetInputKind::pointer_leave});if(!result)return result;}
            return {};
        }
        if(event.kind==WidgetInputKind::scroll&&popup.popover->clip_content&&popup.content_anchor==absent&&child->impl_->popups.empty())return {};
    }
    if(!keyboard&&impl_->hovered!=absent){impl_->hovered=absent;impl_->invalidate();}
    auto local=event;local.x=x;local.y=y;handled=true;return child->input(local);
}
core::Result<void> WidgetState::open_at(std::size_t i,double x,double y) {
    if(auto r=ready();!r) return r;
    if(i>=impl_->nodes.size() || !impl_->nodes[i].popup || !std::isfinite(x) || !std::isfinite(y)) return core::fail(core::ErrorCode::invalid_argument,"Popup requires a menu and finite coordinates");
    if(impl_->building) return core::fail(core::ErrorCode::busy,"Open a popup after its widget factory has attached");
    if(impl_->domain_transition())return core::fail(core::ErrorCode::busy,"Popup content construction or retirement is in progress");
    if(!impl_->nodes[i].shown)return core::fail(core::ErrorCode::invalid_argument,"Popup is hidden");
    for(auto p=impl_->nodes[i].parent;p!=absent;p=impl_->nodes[p].parent) if(!impl_->nodes[p].shown || (impl_->nodes[p].popup && !impl_->nodes[p].popup->opened))
        return core::fail(core::ErrorCode::invalid_argument,"Popup owner is hidden");
    auto& p=*impl_->nodes[i].popup;const bool already=p.opened;
    if(p.popover&&p.popover->sheet_layout){const auto& sheet=*p.popover->sheet_layout;const auto body=impl_->ids.find(sheet.viewport_id),grabber=impl_->ids.find(sheet.grabber_id);if(body==impl_->ids.end()||!impl_->nodes[body->second].scroll||!impl_->descendant(body->second,i)||(!sheet.grabber_id.empty()&&(grabber==impl_->ids.end()||!impl_->descendant(grabber->second,i))))return core::fail(core::ErrorCode::invalid_argument,"Sheet viewport/grabber must belong to its retained content");if(!already)impl_->nodes[body->second].scroll->offset=0;}
    auto domain=shared_from_this();while(auto parent=domain->impl_->embedding_owner.lock())domain=parent;
    if(!already && domain->impl_->active_popups()>=8) return core::fail(core::ErrorCode::out_of_range,"At most eight widget popups may be open in a domain");
    if(!already&&std::ranges::find(impl_->visual_popups,i)==impl_->visual_popups.end()&&domain->impl_->active_visual_popups()>=16)return core::fail(core::ErrorCode::full,"At most sixteen open or exiting popups per domain");
    if(!already){if(auto r=create_popup_content(i);!r)return r;}
    const bool preserve=p.popover&&p.popover->preserve_input;
    auto input_cancel=preserve?runtime::Task{}:impl_->cancel_input_capture();
    auto canceled=preserve||impl_->pressed==absent?runtime::Task{}:impl_->nodes[impl_->pressed].events.canceled;
    if(!already) {p.return_focus=impl_->focused;p.scroll=0;}
    p.opened=true;if(!already)impl_->begin_motion(p);p.anchor_x=x;p.anchor_y=y;p.anchor_widget=absent;const auto epoch=++p.epoch;
    std::erase(impl_->popups,i);impl_->popups.push_back(i);std::erase(impl_->visual_popups,i);impl_->visual_popups.push_back(i);if(!preserve)impl_->pressed=impl_->hovered=absent;impl_->motion_activity();
    impl_->invalidate(true);impl_->layout(impl_->surface?impl_->surface->size():impl_->viewport);
    auto selected=p.popover?(p.popover->focus_on_open?impl_->next_content(i,absent,1):impl_->focused):p.options.selected_index>=0?p.entries[std::size_t(p.options.selected_index)]:absent;
    if(!p.popover&&(selected==absent||!impl_->nodes[selected].enabled||!impl_->logical_visible(selected)))selected=impl_->next_entry(i,absent,1,true);
    auto blur=impl_->change_focus(selected);if(selected!=absent) impl_->reveal(selected);
    if(p.content_view&&p.popover->focus_on_open&&(p.content_anchor==absent||selected==p.content_anchor)){auto view=p.content_view;for(std::size_t j=0;j<view->impl_->nodes.size();++j)if(interactive(view->impl_->nodes[j].kind)&&view->impl_->nodes[j].enabled&&view->impl_->logical_visible(j)){auto r=view->focus(j);if(!r)return r;break;}}
    impl_->accessibility_changed();
    auto opened=p.events.opened;auto invoke=impl_->invoke;if(input_cancel)invoke(std::move(input_cancel));if(!impl_->active)return {};if(canceled) invoke(std::move(canceled));if(impl_->active&&blur)invoke(std::move(blur));
    if(impl_->active && !already && impl_->nodes[i].popup->epoch==epoch && opened) invoke(std::move(opened));return {};
}
core::Result<void> WidgetState::open_for(std::size_t i,Widget anchor,double x,double y) {
    if(auto r=ready();!r) return r;
    if(anchor.state_.lock().get()!=this || anchor.index_>=impl_->nodes.size() || !impl_->logical_visible(anchor.index_)) return core::fail(core::ErrorCode::invalid_argument,"Popup anchor must be visible in this generation");
    if(i>=impl_->nodes.size()||!impl_->nodes[i].popup||impl_->descendant(anchor.index_,i))return core::fail(core::ErrorCode::invalid_argument,"Popup cannot anchor to its own content");
    auto owner=impl_->owning_popup(anchor.index_);for(std::size_t hops=0;owner!=absent&&hops<impl_->nodes.size();++hops){if(owner==i)return core::fail(core::ErrorCode::cycle,"Cyclic popup anchor");auto next=impl_->nodes[owner].popup->anchor_widget;owner=next==absent?absent:impl_->owning_popup(next);}
    impl_->layout(impl_->surface?impl_->surface->size():impl_->viewport);auto r=impl_->nodes[anchor.index_].bounds;const auto epoch=impl_->nodes[i].popup->epoch+1;
    auto result=open_at(i,r.x+x,r.y+y);if(!result||!impl_->active)return result;
    auto& popup=*impl_->nodes[i].popup;if(popup.opened&&popup.epoch==epoch){popup.anchor_widget=anchor.index_;popup.anchor_offset_x=x;popup.anchor_offset_y=y;impl_->invalidate(true);}return {};
}
core::Result<void> WidgetState::close_popup(std::size_t i,bool canceled) {
    if(auto r=ready();!r) return r;
    if(i>=impl_->nodes.size() || !impl_->nodes[i].popup) return core::fail(core::ErrorCode::invalid_argument,"Widget is not a popup");
    if(!impl_->nodes[i].popup->opened) return {};
    auto callbacks=impl_->end_popup(i,true,canceled);auto invoke=impl_->invoke;
    for(auto& callback:callbacks) {if(!ready()) break;invoke(std::move(callback));}return {};
}
core::Result<void> WidgetState::selected_index(std::size_t i,int index) {
    if(auto r=ready();!r) return r;
    if(i>=impl_->nodes.size() || !impl_->nodes[i].popup || impl_->nodes[i].popup->popover) return core::fail(core::ErrorCode::invalid_argument,"Widget has no menu selection");
    auto& p=*impl_->nodes[i].popup;
    if(index < -1 || (index>=0 && (std::size_t(index)>=p.entries.size() || !impl_->nodes[p.entries[std::size_t(index)]].menu_item))) return core::fail(core::ErrorCode::out_of_range,"Menu selection must name an item, or -1");
    p.options.selected_index=index;impl_->invalidate();return {};
}
core::Result<void> WidgetState::menu_state(std::size_t i,MenuItemState state) {
    if(auto r=ready();!r) return r;
    if(i>=impl_->nodes.size() || !impl_->nodes[i].menu_item || static_cast<unsigned>(state)>2) return core::fail(core::ErrorCode::invalid_argument,"Invalid menu item state");
    auto& n=impl_->nodes[i];n.menu_item->state=state;return enabled(i,n.menu_item->enabled);
}
core::Result<void> WidgetState::menu_expanded(std::size_t i,bool expanded) {
    if(auto r=ready();!r) return r;
    if(i>=impl_->nodes.size() || !impl_->nodes[i].menu_item) return core::fail(core::ErrorCode::invalid_argument,"Widget is not a menu item");
    impl_->nodes[i].menu_item->expanded=expanded;impl_->invalidate();return {};
}
core::Result<void> WidgetState::context_menu(std::size_t i,Widget popup) {
    if(auto r=ready();!r) return r;
    if(i>=impl_->nodes.size()) return core::fail(core::ErrorCode::invalid_argument,"Context menu owner was not found");
    if(popup.owner_==std::thread::id{}) {impl_->nodes[i].context_menu=absent;return {};}
    if(popup.state_.lock().get()!=this || popup.index_>=impl_->nodes.size() || !impl_->nodes[popup.index_].popup)
        return core::fail(core::ErrorCode::invalid_argument,"Context menu must be a popup in this generation");
    impl_->nodes[i].context_menu=popup.index_;return {};
}
core::Result<bool> WidgetState::route_input_handler(const WidgetInput& event){
    const bool pointer=event.kind==WidgetInputKind::pointer_down||event.kind==WidgetInputKind::pointer_move||event.kind==WidgetInputKind::pointer_up||event.kind==WidgetInputKind::pointer_leave;
    const bool captured=impl_->input_capture!=absent&&(pointer||event.kind==WidgetInputKind::focus_lost);
    const bool pointed=pointer||event.kind==WidgetInputKind::scroll||event.kind==WidgetInputKind::context_request||event.kind==WidgetInputKind::drop;
    auto target=captured?impl_->input_capture:pointed?impl_->hit(event.x,event.y):impl_->focused;
    if(!captured&&(event.kind==WidgetInputKind::pointer_move||event.kind==WidgetInputKind::pointer_leave)){
        std::vector<std::size_t> next;
        if(event.kind==WidgetInputKind::pointer_move)for(auto i=target;i!=absent;i=impl_->nodes[i].parent)
            if(impl_->nodes[i].input_handler&&impl_->nodes[i].enabled&&impl_->logical_visible(i)&&!impl_->blocked(i))next.push_back(i);
        auto previous=std::exchange(impl_->input_hover_path,std::move(next));
        for(auto i:previous){
            if(std::ranges::find(impl_->input_hover_path,i)!=impl_->input_hover_path.end()||i>=impl_->nodes.size()||!impl_->nodes[i].input_handler)continue;
            auto handler=impl_->nodes[i].input_handler;auto local=event;local.kind=WidgetInputKind::pointer_leave;local.target=impl_->nodes[i].id;
            auto point=impl_->logical_point(i,event.x,event.y);local.x=point.width;local.y=point.height;
            core::Result<bool> result=false;bool completed=false;auto invoke=impl_->invoke;
            try{invoke([&]{result=handler(local);completed=true;});}catch(...){return core::fail(core::ErrorCode::failed,"Widget leave handler failed",std::current_exception());}
            if(!impl_->active||!completed)return true;if(!result)return std::unexpected(result.error());
        }
        if(event.kind==WidgetInputKind::pointer_leave)return false;
    }
    if(target==absent&&!pointed&&!impl_->nodes.empty())target=0;
    if(event.kind==WidgetInputKind::pointer_up||event.kind==WidgetInputKind::focus_lost)impl_->input_capture=absent;
    for(auto index=target;index!=absent;index=impl_->nodes[index].parent){
        if(!impl_->nodes[index].input_handler||!impl_->nodes[index].enabled||!impl_->logical_visible(index)||impl_->blocked(index))continue;
        auto handler=impl_->nodes[index].input_handler;auto local=event;auto point=impl_->logical_point(index,event.x,event.y);
        local.x=point.width;local.y=point.height;local.target=target==absent?std::string{}:impl_->nodes[target].id;
        local.text_input_active=impl_->focused!=absent&&(impl_->nodes[impl_->focused].kind==WidgetKind::input_field||impl_->nodes[impl_->focused].text_input);
        const auto previous_focus=impl_->focused;
        core::Result<bool> result=true;bool completed=false;auto invoke=impl_->invoke;
        try{invoke([&]{result=handler(local);completed=true;});}catch(...){return core::fail(core::ErrorCode::failed,"Widget input handler failed",std::current_exception());}
        if(!impl_->active||!completed)return true;
        if(!result)return std::unexpected(result.error());
        if(*result){if(event.kind==WidgetInputKind::pointer_down&&impl_->nodes[index].input_handler&&impl_->nodes[index].enabled&&impl_->logical_visible(index)&&!impl_->blocked(index)){
            impl_->input_capture=index;impl_->pressed=absent;bool auto_focus=true;
            if(impl_->nodes[index].text_input){auto state=text_input_state(index);if(!state)return std::unexpected(state.error());auto_focus=state->auto_focus_on_press;}
            auto changed=auto_focus&&impl_->focused==previous_focus?impl_->change_focus(index):runtime::Task{};impl_->invalidate();if(changed)invoke(std::move(changed));}return event.kind!=WidgetInputKind::focus_lost;}
        if(captured)return true;
    }
    return captured&&event.kind!=WidgetInputKind::focus_lost;
}
core::Result<void> WidgetState::input(const WidgetInput& incoming){
    auto result=input_impl(incoming);if(!result||!impl_->active)return result;
    return flush_interactions();
}
core::Result<void> WidgetState::input_impl(const WidgetInput& incoming) {
    auto event=incoming;
    if (auto r=ready();!r) return r;
    if (static_cast<unsigned>(event.kind)>static_cast<unsigned>(WidgetInputKind::commit_composition) || static_cast<unsigned>(event.key)>static_cast<unsigned>(WidgetKey::redo) || !std::isfinite(event.x) || !std::isfinite(event.y) || !std::isfinite(event.scroll_y) || !std::isfinite(event.scroll_x) || !valid_title(event.text) || event.text.size()>65536 || unsigned(event.device)>unsigned(PointerDevice::touch) || !std::isfinite(event.pressure) || event.pressure<0 || event.pressure>1 || !std::isfinite(event.tilt_x) || std::abs(event.tilt_x)>90 || !std::isfinite(event.tilt_y) || std::abs(event.tilt_y)>90 || !std::isfinite(event.rotation) || !std::isfinite(event.timestamp) || event.timestamp<0) return core::fail(core::ErrorCode::invalid_argument,"Invalid widget input");
    if(event.content&&!valid_clipboard(*event.content))return core::fail(core::ErrorCode::invalid_argument,"Invalid drop payload");
    if(event.kind==WidgetInputKind::commit_composition&&impl_->focused!=absent&&impl_->nodes[impl_->focused].kind==WidgetKind::input_field){auto& n=impl_->nodes[impl_->focused];if(n.marked.empty())return {};event.kind=WidgetInputKind::text;event.text=n.marked;}
    if(!event.target.empty()){
        auto target=find(event.target);if(!target)return std::unexpected(target.error());auto owner=target->state_.lock();if(owner.get()!=this){auto here=global_offset(),there=owner->global_offset();event.x+=here.width-there.width;event.y+=here.height-there.height;return owner->input(event);}event.target=impl_->nodes[target->index_].id;
    }
    if(event.kind==WidgetInputKind::activate || event.kind==WidgetInputKind::focus || event.kind==WidgetInputKind::increment || event.kind==WidgetInputKind::decrement || event.kind==WidgetInputKind::close_popup) {
        auto it=impl_->ids.find(event.target); if(it==impl_->ids.end()) return core::fail(core::ErrorCode::not_found,"Input target was not found");
        if(event.kind==WidgetInputKind::close_popup) return close_popup(it->second,true);
        if(event.kind==WidgetInputKind::increment || event.kind==WidgetInputKind::decrement) {
            const auto index=it->second;auto& n=impl_->nodes[index];
            if(n.color){if(!n.enabled || !impl_->logical_visible(index) || impl_->blocked(index))return {};if(n.color->options.field<ColorField::hue_rail)return core::fail(core::ErrorCode::invalid_argument,"Only color rails have an increment action");const bool increase=event.kind==WidgetInputKind::increment;return n.color->key((increase != (n.color->options.field==ColorField::hue_rail))?WidgetKey::up:WidgetKey::down,false);}
            return step(index,event.kind==WidgetInputKind::increment?1:-1);
        }
        return event.kind==WidgetInputKind::activate?activate(it->second):focus(it->second);
    }
    impl_->layout(impl_->surface?impl_->surface->size():impl_->viewport);
    if(auto r=flush_viewports();!r)return r;
    if(event.kind==WidgetInputKind::pointer_up&&impl_->consume_release){impl_->consume_release=false;return {};}
    if(!impl_->popups.empty()){
        const auto top=impl_->popups.back();const auto& popup=*impl_->nodes[top].popup;
        if(popup.popover&&popup.popover->preserve_input){
            if(event.kind==WidgetInputKind::key_down&&event.key==WidgetKey::escape&&popup.popover->dismiss_on_escape)return close_popup(top,true);
            if(event.kind==WidgetInputKind::pointer_down&&!impl_->popup_contains(top,event.x,event.y)&&popup.popover->dismiss_on_outside_press){auto r=close_popup(top,true);if(!r||!impl_->active)return r;}
        }
    }
    bool routed=false;if(auto r=route_content(event,routed);!r||routed)return r;if(!impl_->active)return {};
    if(event.kind==WidgetInputKind::scroll||event.kind==WidgetInputKind::context_request||event.kind==WidgetInputKind::focus_lost){
        if(auto handled=route_input_handler(event);!handled)return std::unexpected(handled.error());else if(*handled)return {};
    }
    if(!impl_->popups.empty()){
        const auto top=impl_->popups.back();auto& p=*impl_->nodes[top].popup;
        if(p.popover&&p.popover->sheet_layout){const auto& sheet=*p.popover->sheet_layout;
            const auto snap_back=[&]{p.dragging=false;p.snap_from=p.drag_offset;p.snap_start=std::max(runtime::Clock::now(),impl_->motion_now);p.snap_active=p.drag_offset!=0&&impl_->motion_duration(p)>runtime::Clock::duration::zero();if(!p.snap_active)p.drag_offset=0;impl_->invalidate();impl_->motion_activity();};
            if(p.dragging){
                if(event.kind==WidgetInputKind::pointer_move){p.drag_offset=std::max(0.,event.y-p.drag_start);impl_->invalidate();return {};}
                if(event.kind==WidgetInputKind::pointer_up){p.dragging=false;if(p.drag_offset>=std::min(120.,impl_->nodes[top].bounds.height*.2))return close_popup(top);snap_back();return {};}
                if(event.kind==WidgetInputKind::pointer_leave)return {};
                if(event.kind==WidgetInputKind::focus_lost)snap_back();
            }
            if(event.kind==WidgetInputKind::pointer_down&&sheet.mobile&&sheet.dismiss_on_drag&&!p.motion.active){const auto grabber=impl_->ids.find(sheet.grabber_id);const auto point=impl_->logical_point(top,event.x,event.y);if(grabber!=impl_->ids.end()&&impl_->logical_visible(grabber->second)&&contains(impl_->nodes[grabber->second].bounds,point.width,point.height)){p.dragging=true;p.snap_active=false;p.drag_start=event.y;p.drag_offset=0;impl_->pressed=impl_->hovered=absent;impl_->invalidate();impl_->motion_activity();return {};}}
        }
    }
    if(event.kind==WidgetInputKind::scroll){
        std::size_t target=absent;
        for(std::size_t n=impl_->nodes.size();n>0;--n){const auto i=n-1;const auto& node=impl_->nodes[i];if(!node.scroll||!node.enabled||!impl_->logical_visible(i)||impl_->blocked(i))continue;const auto point=impl_->logical_point(i,event.x,event.y);const auto clip=impl_->clip_rect(i);if(contains(node.bounds,point.width,point.height)&&(!clip||contains(*clip,point.width,point.height))){target=i;break;}}
        double dx=event.scroll_x,dy=event.scroll_y;if(event.shift&&dx==0){dx=dy;dy=0;}
        bool moved=false;
        for(auto i=target;i!=absent;i=impl_->nodes[i].parent){
            auto& node=impl_->nodes[i];if(!node.scroll||!node.scroll->options.enabled)continue;
            const auto x=std::clamp(node.scroll->horizontal_offset+dx,0.,std::max(0.,node.scroll->content_width-node.bounds.width));
            const auto y=std::clamp(node.scroll->offset+dy,0.,std::max(0.,node.scroll->content_height-node.bounds.height));
            const bool move_x=x!=node.scroll->horizontal_offset,move_y=y!=node.scroll->offset;
            if(!move_x&&!move_y)continue;
            if(move_x)dx=0;if(move_y)dy=0;moved=true;
            if(auto result=scroll_position(i,x,y);!result)return result;
            if(dx==0&&dy==0)break;
        }
        if(moved)return {};
    }
    const auto dismiss_outside=[&](bool context)->core::Result<void> {
        std::vector<std::pair<std::size_t,std::uint64_t>> targets;
        for(auto i:impl_->popups) if(!impl_->popup_contains(i,event.x,event.y) &&
            (context?impl_->nodes[i].popup->options.dismiss_on_context_request:impl_->nodes[i].popup->options.dismiss_on_outside_press)) targets.emplace_back(i,impl_->nodes[i].popup->epoch);
        for(auto it=targets.rbegin();it!=targets.rend();++it) {
            if(!impl_->active) return {};
            if(impl_->nodes[it->first].popup->opened && impl_->nodes[it->first].popup->epoch==it->second) {auto r=close_popup(it->first,true);if(!r) return r;}
        }
        return {};
    };
    if(event.kind==WidgetInputKind::context_request) {
        auto r=dismiss_outside(true);if(!r || !impl_->active) return r;
        impl_->layout(impl_->surface?impl_->surface->size():impl_->viewport);
        if(!event.target.empty()) {
            auto owner=impl_->ids.find(event.target);if(owner==impl_->ids.end()) return core::fail(core::ErrorCode::not_found,"Context target was not found");
            const auto& n=impl_->nodes[owner->second];if(n.context_menu!=absent && n.enabled && impl_->visible(owner->second) && !impl_->blocked(owner->second)) return open_at(n.context_menu,event.x,event.y);
        } else for(std::size_t i=impl_->nodes.size();i>0;--i) {
            const auto& n=impl_->nodes[i-1];if(n.context_menu!=absent && n.enabled && impl_->visible(i-1) && !impl_->blocked(i-1) && contains(impl_->visual_bounds(i-1),event.x,event.y)) return open_at(n.context_menu,event.x,event.y);
        }
        return {};
    }
    if(event.kind==WidgetInputKind::pointer_down) impl_->consume_release=false;
    if(event.kind==WidgetInputKind::pointer_up && impl_->consume_release) {impl_->consume_release=false;return {};}
    if(!impl_->popups.empty()&&impl_->nodes[impl_->popups.back()].popup->popover){
        const auto top=impl_->popups.back();const auto options=*impl_->nodes[top].popup->popover;
        const bool inside=impl_->popup_contains(top,event.x,event.y);
        if(options.preserve_input&&event.kind==WidgetInputKind::key_down&&event.key==WidgetKey::escape&&options.dismiss_on_escape)return close_popup(top,true);
        if(event.kind==WidgetInputKind::focus_lost){auto blur=impl_->change_focus(absent);auto canceled=impl_->pressed==absent?runtime::Task{}:impl_->nodes[impl_->pressed].events.canceled;impl_->pressed=impl_->hovered=absent;impl_->invalidate();auto invoke=impl_->invoke;if(canceled)invoke(std::move(canceled));if(impl_->active&&blur)invoke(std::move(blur));return {};}
        if(event.kind==WidgetInputKind::pointer_down&&!inside){
            bool in_other=false;for(auto p:impl_->popups)if(impl_->popup_contains(p,event.x,event.y)){in_other=true;break;}
            if(!in_other){std::vector<std::pair<std::size_t,std::uint64_t>> before;for(auto p:impl_->popups)before.emplace_back(p,impl_->nodes[p].popup->epoch);
                auto r=dismiss_outside(false);if(!r||!impl_->active)return r;
                bool reopened=false;for(auto p:impl_->popups)if(std::ranges::find(before,std::pair{p,impl_->nodes[p].popup->epoch})==before.end()){reopened=true;break;}
                if(options.modal||reopened){impl_->consume_release=true;return {};}
                impl_->layout(impl_->surface?impl_->surface->size():impl_->viewport);
            }
        }
        if(event.kind==WidgetInputKind::scroll){if(inside&&options.clip_content){auto& popup=*impl_->nodes[top].popup;popup.scroll=std::clamp(popup.scroll+event.scroll_y,0.0,std::max(0.0,popup.content_height-popup.content.height));impl_->invalidate(true);}return {};}
        const auto owner=impl_->focused==absent?absent:impl_->owning_popup(impl_->focused);
        if(event.kind==WidgetInputKind::key_down&&owner!=absent&&impl_->nodes[owner].popup->popover){
            if(event.key==WidgetKey::escape){bool handled=false;for(auto p=impl_->focused;p!=absent;p=impl_->nodes[p].parent)if(impl_->nodes[p].escape){handled=true;break;}
                if(!handled){if(impl_->nodes[owner].popup->popover->dismiss_on_escape)return close_popup(owner,true);return {};}}
            if(event.key==WidgetKey::tab)return focus_content(impl_->next_content(owner,impl_->focused,event.shift?-1:1),event.shift?-1:1);
        }
    }
    if(!impl_->popups.empty()&&!impl_->nodes[impl_->popups.back()].popup->popover) {
        const auto top=impl_->popups.back();auto& popup=*impl_->nodes[top].popup;
        const bool inside=contains(impl_->visual_bounds(top),event.x,event.y);
        if(event.kind==WidgetInputKind::focus_lost) {
            auto open=impl_->popups;
            for(auto i:open) {if(!impl_->active) return {};if(impl_->nodes[i].popup->opened&&!impl_->nodes[i].popup->popover) {auto r=close_popup(i);if(!r) return r;}}
            if(impl_->active) {auto blur=impl_->change_focus(absent);impl_->hovered=impl_->pressed=absent;impl_->invalidate();auto invoke=impl_->invoke;if(blur)invoke(std::move(blur));}return {};
        }
        if(event.kind==WidgetInputKind::pointer_down && !inside) {
            std::size_t behind=absent;for(auto it=impl_->popups.rbegin();it!=impl_->popups.rend();++it) if(contains(impl_->visual_bounds(*it),event.x,event.y)) {behind=*it;break;}
            auto r=dismiss_outside(false);if(!r || !impl_->active) return r;
            if(behind==absent || impl_->popups.empty() || impl_->popups.back()!=behind) {impl_->consume_release=true;return {};}
            impl_->layout(impl_->surface?impl_->surface->size():impl_->viewport);
        }
        if(event.kind==WidgetInputKind::pointer_up && !inside && impl_->pressed!=absent) {
            if(popup.options.dismiss_on_outside_press) return dismiss_outside(false);
        }
        if(event.kind==WidgetInputKind::scroll) {
            if(inside) {popup.scroll=std::clamp(popup.scroll+event.scroll_y,0.0,std::max(0.0,popup.content_height-popup.content.height));impl_->invalidate(true);}return {};
        }
        if(event.kind==WidgetInputKind::text || event.kind==WidgetInputKind::composition) return {};
        if(event.kind==WidgetInputKind::key_down) {
            if(event.key==WidgetKey::escape) return close_popup(top);
            if(event.key==WidgetKey::up || event.key==WidgetKey::down || event.key==WidgetKey::home || event.key==WidgetKey::end || event.key==WidgetKey::tab) {
                const auto direction=event.key==WidgetKey::up || event.key==WidgetKey::end || (event.key==WidgetKey::tab && event.shift)?-1:1;
                auto next=impl_->next_entry(top,impl_->focused,direction,event.key==WidgetKey::home || event.key==WidgetKey::end);
                return next==absent?core::Result<void>{}:focus(next);
            }
            if(event.key==WidgetKey::left && impl_->popups.size()>1) return close_popup(top);
            if(event.key==WidgetKey::right && impl_->focused!=absent && impl_->nodes[impl_->focused].menu_item && impl_->nodes[impl_->focused].menu_item->has_child_items) return activate(impl_->focused);
            if(event.key!=WidgetKey::enter && event.key!=WidgetKey::space) return {};
        }
    } else if(event.kind==WidgetInputKind::scroll || event.kind==WidgetInputKind::context_request) return {};
    if(event.kind!=WidgetInputKind::focus_lost){if(auto handled=route_input_handler(event);!handled)return std::unexpected(handled.error());else if(*handled)return {};}
    const auto hit=impl_->hit(event.x,event.y);
    const auto move_slider=[&](std::size_t index,double v) -> core::Result<void> {
        auto& n=impl_->nodes[index]; if(v==n.slider.value) return {}; auto callback=n.moved;
        auto result=value(index,v); if(!result) return result;
        if(callback) { auto invoke=impl_->invoke; invoke([callback,v] { callback(v); }); } return {};
    };
    if(event.kind==WidgetInputKind::pointer_move) {
        impl_->hovered=hit; impl_->invalidate();
        if(!impl_->popups.empty()&&!impl_->nodes[impl_->popups.back()].popup->popover&&hit!=absent&&impl_->focused!=hit){impl_->focused=hit;impl_->accessibility_changed();}
        if(impl_->pressed!=absent && impl_->nodes[impl_->pressed].kind==WidgetKind::slider) return move_slider(impl_->pressed,impl_->slider_value(impl_->pressed,event.x));
        if(impl_->pressed!=absent && impl_->nodes[impl_->pressed].color) {auto& n=impl_->nodes[impl_->pressed];const auto point=impl_->logical_point(impl_->pressed,event.x,event.y);return n.color->edit(n.bounds,point.width,point.height);}
    } else if(event.kind==WidgetInputKind::pointer_down) {
        if(hit!=absent && impl_->nodes[hit].color){
            auto canceled=impl_->pressed==absent?runtime::Task{}:impl_->nodes[impl_->pressed].events.canceled;
            auto& n=impl_->nodes[hit];const auto point=impl_->logical_point(hit,event.x,event.y);const bool accepted=n.color->begin(n.bounds,point.width,point.height);
            impl_->pressed=accepted?hit:absent;auto blur=accepted?impl_->pointer_focus(hit):runtime::Task{};if(accepted)impl_->hovered=hit;impl_->invalidate();
            auto invoke=impl_->invoke;if(canceled)invoke(std::move(canceled));if(blur)invoke(std::move(blur));
            if(!impl_->active || !accepted || impl_->pressed!=hit || impl_->focused!=hit)return {};
            const auto edited_point=impl_->logical_point(hit,event.x,event.y);return impl_->nodes[hit].color->edit(impl_->nodes[hit].bounds,edited_point.width,edited_point.height);
        }
        auto canceled=impl_->pressed==absent?runtime::Task{}:impl_->nodes[impl_->pressed].events.canceled;
        impl_->pressed=hit;auto blur=impl_->pointer_focus(hit);impl_->hovered=hit;impl_->invalidate();auto invoke=impl_->invoke;
        if(canceled)invoke(std::move(canceled));if(blur)invoke(std::move(blur));
        if(!impl_->active || impl_->pressed!=hit || impl_->focused!=hit)return {};
        if(hit!=absent && impl_->nodes[hit].kind==WidgetKind::slider) return move_slider(hit,impl_->slider_value(hit,event.x));
        auto pressed=hit==absent?runtime::Task{}:impl_->nodes[hit].events.pressed;
        if(pressed)invoke(std::move(pressed));
    } else if(event.kind==WidgetInputKind::pointer_up) {
        const auto down=impl_->pressed; impl_->pressed=absent; impl_->hovered=hit; impl_->invalidate();
        if(down!=absent) {
            const auto& n=impl_->nodes[down];const auto kind=n.kind;
            if(n.color){const auto point=impl_->logical_point(down,event.x,event.y);return n.color->edit(n.bounds,point.width,point.height);}
            const auto direction=n.stepper.arrow==StepperArrow::up?1:n.stepper.arrow==StepperArrow::down?-1:impl_->logical_point(down,event.x,event.y).height<n.bounds.y+n.bounds.height/2?1:-1;
            auto released=n.events.released;auto invoke=impl_->invoke;if(released) invoke(std::move(released));
            if(impl_->active && down==hit && impl_->focused==down && kind!=WidgetKind::slider && kind!=WidgetKind::input_field)
                return kind==WidgetKind::stepper?step(down,direction):activate(down);
        }
    } else if(event.kind==WidgetInputKind::pointer_leave) { impl_->hovered=absent; impl_->invalidate(); }
    else if(event.kind==WidgetInputKind::focus_lost || (event.kind==WidgetInputKind::key_down && event.key==WidgetKey::escape)) {
        auto canceled=impl_->pressed==absent?runtime::Task{}:impl_->nodes[impl_->pressed].events.canceled;
        runtime::Task escape;
        if(event.kind==WidgetInputKind::key_down)for(auto p=impl_->focused;p!=absent;p=impl_->nodes[p].parent)if(impl_->nodes[p].escape){escape=impl_->nodes[p].escape;break;}
        impl_->pressed=absent;auto blur=event.kind==WidgetInputKind::focus_lost?impl_->change_focus(absent):runtime::Task{};if(event.kind==WidgetInputKind::focus_lost)impl_->hovered=absent;
        impl_->invalidate();auto invoke=impl_->invoke;if(canceled)invoke(std::move(canceled));if(blur)invoke(std::move(blur));if(impl_->active && escape)invoke(std::move(escape));
    }
    else if(event.kind==WidgetInputKind::key_down && event.key==WidgetKey::tab) {
        const auto count=impl_->nodes.size(); auto current=impl_->focused;
        for(std::size_t k=0;k<count;++k) { current=current==absent?(event.shift?count-1:0):(current+count+(event.shift?-1:1))%count;
            if((interactive(impl_->nodes[current].kind)||impl_->nodes[current].input_handler) && impl_->nodes[current].enabled && impl_->logical_visible(current) && !impl_->blocked(current)) return focus(current); }
    } else if(impl_->focused!=absent) {
        const auto index=impl_->focused; auto& n=impl_->nodes[index];
        if(n.kind==WidgetKind::tab && n.parent!=absent && impl_->nodes[n.parent].kind==WidgetKind::tab_bar && event.kind==WidgetInputKind::key_down &&
            (event.key==WidgetKey::left || event.key==WidgetKey::right || event.key==WidgetKey::home || event.key==WidgetKey::end)) {
            const auto& children=impl_->nodes[n.parent].children;const auto count=int(children.size());
            int start=int(std::ranges::find(children,index)-children.begin()),direction=event.key==WidgetKey::left || event.key==WidgetKey::end?-1:1;
            if(event.key==WidgetKey::home) start=-1;else if(event.key==WidgetKey::end) start=count;
            for(int offset=1;offset<=count;++offset) { const auto next=children[std::size_t((start+offset*direction+count*2)%count)];if(impl_->nodes[next].enabled) return focus(next); }
        } else if(n.kind==WidgetKind::stepper && event.kind==WidgetInputKind::key_down && (event.key==WidgetKey::up || event.key==WidgetKey::down)) {
            return step(index,event.key==WidgetKey::up?1:-1);
        } else if(n.kind==WidgetKind::input_field) {
            if((event.kind==WidgetInputKind::composition||event.kind==WidgetInputKind::text)&&event.replacement_range){
                const auto range=*event.replacement_range;
                if(range.location>65536||range.length>65536-range.location)return core::fail(core::ErrorCode::out_of_range,"Invalid UTF-16 replacement range");
                const auto first=utf16_byte(n.text,range.location),last=utf16_byte(n.text,range.location+range.length);
                if(!first||!last)return core::fail(core::ErrorCode::out_of_range,"Replacement range must cover complete Unicode scalars");
                n.anchor=*first;n.cursor=*last;
            }
            if(event.kind==WidgetInputKind::composition) {
                const auto range=event.composition_selection.value_or(TextRange{utf16_count(event.text),0});
                const auto length=utf16_count(event.text);
                if(range.location>length||range.length>length-range.location||!utf16_byte(event.text,range.location)||!utf16_byte(event.text,range.location+range.length))return core::fail(core::ErrorCode::out_of_range,"Invalid composition selection");
                n.marked=event.text;n.marked_selection=range;impl_->invalidate();return {};
            }
            if(event.kind==WidgetInputKind::key_down && event.key==WidgetKey::enter && (!n.field.multiline||event.control||event.meta)) return activate(index);
            if(event.kind==WidgetInputKind::key_down && event.key==WidgetKey::select_all) { n.anchor=0; n.cursor=n.text.size(); impl_->invalidate(); return {}; }
            if(event.kind==WidgetInputKind::key_down && (event.key==WidgetKey::left || event.key==WidgetKey::right || event.key==WidgetKey::home || event.key==WidgetKey::end)) {
                n.cursor=event.key==WidgetKey::left?previous_scalar(n.text,n.cursor):event.key==WidgetKey::right?next_scalar(n.text,n.cursor):event.key==WidgetKey::home?0:n.text.size();
                if(!event.shift) n.anchor=n.cursor; impl_->invalidate(); return {};
            }
            std::string inserted; bool editing=false;
            if(event.kind==WidgetInputKind::text) { inserted=event.text; editing=true; }
            if(event.kind==WidgetInputKind::key_down && event.key==WidgetKey::enter && n.field.multiline){inserted="\n";editing=true;}
            if(event.kind==WidgetInputKind::key_down && event.key==WidgetKey::cut&&!n.field.password)editing=true;
            if(event.kind==WidgetInputKind::key_down && event.key==WidgetKey::space) { inserted=" "; editing=true; }
            if(event.kind==WidgetInputKind::key_down && (event.key==WidgetKey::backspace || event.key==WidgetKey::delete_forward)) {
                if(n.anchor==n.cursor) { if(event.key==WidgetKey::backspace) n.anchor=previous_scalar(n.text,n.cursor); else n.anchor=next_scalar(n.text,n.cursor); } editing=true;
            }
            if(editing && !n.field.read_only) {
                auto begin=std::min(n.anchor,n.cursor),end=std::max(n.anchor,n.cursor);
                if(n.field.maximum_length){const auto retained=scalar_count(n.text)-scalar_count(std::string_view(n.text).substr(begin,end-begin));inserted.resize(scalar_prefix(inserted,n.field.maximum_length>retained?n.field.maximum_length-retained:0));}
                if(n.text.size()-(end-begin)+inserted.size()>65536) return core::fail(core::ErrorCode::out_of_range,"Input text limit is 64 KiB");
                const auto old=n.text; n.text.replace(begin,end-begin,inserted); n.cursor=n.anchor=begin+inserted.size(); n.marked.clear();
                auto callback=n.edited; auto text=n.text; impl_->invalidate(); if(callback && text!=old) { auto invoke=impl_->invoke; invoke([callback,text=std::move(text)] { callback(text); }); }
            }
        } else if(event.kind==WidgetInputKind::key_down && n.color) {
            return n.color->key(event.key,event.shift);
        } else if(event.kind==WidgetInputKind::key_down && n.kind==WidgetKind::slider) {
            const auto step=n.slider.step>0?n.slider.step:(n.slider.maximum-n.slider.minimum)/100;
            auto next=n.slider.value;
            if(event.key==WidgetKey::left || event.key==WidgetKey::down) next-=step;
            else if(event.key==WidgetKey::right || event.key==WidgetKey::up) next+=step;
            else if(event.key==WidgetKey::home) next=n.slider.minimum; else if(event.key==WidgetKey::end) next=n.slider.maximum;
            return move_slider(index,std::clamp(next,n.slider.minimum,n.slider.maximum));
        } else if(event.kind==WidgetInputKind::key_down && (event.key==WidgetKey::space || event.key==WidgetKey::enter)) {
            if(impl_->pressed==absent) { impl_->pressed=index;impl_->invalidate();auto pressed=n.events.pressed;auto invoke=impl_->invoke;if(pressed) invoke(std::move(pressed)); }
        } else if(event.kind==WidgetInputKind::key_up && (event.key==WidgetKey::space || event.key==WidgetKey::enter) && impl_->pressed==index) {
            impl_->pressed=absent;impl_->invalidate();auto released=n.events.released;auto invoke=impl_->invoke;if(released) invoke(std::move(released));
            if(impl_->active) return activate(index);
        }
    }
    return {};
}
CanvasFrame WidgetState::frame(Size size,std::size_t* popup_start) {
    impl_->layout(size); CanvasFrame f{.viewport={0,0,float(size.width),float(size.height)},.background=color(impl_->appearance.window)};
    const auto white=WidgetColor{1,1,1,.9},track=WidgetColor{.193556845,.197807208,.199932396,1};
    auto layer_panel=[&](auto& layer,Rect r,WidgetColor c,float radius=0) {
        if(r.width>0 && r.height>0 && c.alpha>0) layer.panels.push_back({r,color(c),color(c),std::min(radius,std::min(r.width,r.height)/2)});
    };
    const auto primary=impl_->appearance.primary;
    const auto muted=WidgetColor{.1411764771*.75+primary.red*.25,.1411764771*.75+primary.green*.25,.1411764771*.75+primary.blue*.25,1};
    auto draw_menu_item=[&](auto& layer,std::size_t i) {
        const auto& n=impl_->nodes[i];const bool enabled=impl_->effective_enabled(i);const auto& o=*n.menu_item;const auto r=n.bounds;const bool compact=*o.compact,inactive=o.state==MenuItemState::inactive;
        const bool selected=impl_->item_selected(i) || (n.parent!=absent && impl_->nodes[n.parent].popup && impl_->focused==i);
        const auto background=selected?(compact?primary:muted):inactive?WidgetColor{.1370945126,.1402995437,.1419020444,1}:
            enabled && impl_->pressed==i?muted:enabled && impl_->hovered==i?WidgetColor{.1230252981,.1252803057,.1275353134,1}:WidgetColor{0,0,0,0};
        layer_panel(layer,r,background,compact?0:4);
        auto label=impl_->text_run(n);label.color=color({1,1,1,inactive || enabled?(compact?1:.9):.3});
        auto shortcut=label;shortcut.text=impl_->shortcut(n);shortcut.font_size=compact?12:13;shortcut.font_weight=compact?600:500;shortcut.font_family=impl_->appearance.font_family;shortcut.color=color({1,1,1,inactive || enabled?.6:.3});
        const auto width=std::max(0.f,r.width-8);const auto icon=o.show_icon_slot?float(o.icon_size):0;const auto gap=icon>0?8.f:0.f;
        const auto chevron=o.show_chevron && o.has_child_items?float(o.chevron_size):0;
        const auto key_width=shortcut.text.empty()?0.f:std::min(measure_text_width(shortcut),std::max(0.f,(width-icon-gap-chevron-16)*.45f));
        const auto trailing=key_width+(key_width>0 && chevron>0?8:0)+chevron;
        label.bounds={r.x+4+icon+gap,r.y,std::max(0.f,width-icon-gap-(trailing>0?8+trailing:0)),r.height};
        label.bounds.y+=(r.height-measure_text(label))/2;
        if(o.show_icon_slot) {
            Rect slot{r.x+4,r.y+(r.height-icon)/2,icon,icon};
            if(o.icon) layer.images.push_back({slot,o.icon,{},1});
            else {
                layer_panel(layer,slot,primary,icon/2);const auto border=icon/16;
                layer_panel(layer,{slot.x+border,slot.y+border,icon-2*border,icon-2*border},muted,(icon-2*border)/2);
                layer_panel(layer,{slot.x+icon*.375f,slot.y+icon*.375f,icon*.25f,icon*.25f},primary,icon*.125f);
            }
        }
        if(!label.text.empty() && label.bounds.width>0) layer.texts.push_back(std::move(label));
        if(key_width>0) {
            shortcut.bounds={r.x+4+width-trailing,r.y,key_width,r.height};shortcut.bounds.y+=(r.height-measure_text(shortcut))/2;layer.texts.push_back(std::move(shortcut));
        }
        if(chevron>0) {
            const auto direction=o.direction==MenuDirection::automatic?(o.expanded?MenuDirection::down:MenuDirection::right):o.direction;
            const auto image=direction==MenuDirection::left?o.chevron_left:direction==MenuDirection::up?o.chevron_up:direction==MenuDirection::down?o.chevron_down:o.chevron_right;
            Rect bounds{r.x+r.width-4-chevron,r.y+(r.height-chevron)/2,chevron,chevron};
            if(image) layer.images.push_back({bounds,image,{},inactive || enabled?1.f:.45f});
            else {
                auto glyph=impl_->text_run(n);glyph.text=direction==MenuDirection::left?"‹":direction==MenuDirection::up?"⌃":direction==MenuDirection::down?"⌄":"›";glyph.bounds=bounds;glyph.font_size=14;glyph.color=color({1,1,1,inactive || enabled?.6:.3});glyph.alignment=TextAlignment::center;
                glyph.bounds.y+=(bounds.height-measure_text(glyph))/2;layer.texts.push_back(std::move(glyph));
            }
        }
    };
    auto draw_divider=[&](auto& layer,const Impl::Node& n) {
        const auto& d=*n.divider;const auto r=n.bounds;const auto inset=float(*d.line_padding),thickness=float(d.thickness);
        layer_panel(layer,d.vertical?Rect{r.x+(r.width-thickness)/2,r.y+inset,thickness,std::max(0.f,r.height-2*inset)}:
            Rect{r.x+inset,r.y+(r.height-thickness)/2,std::max(0.f,r.width-2*inset),thickness},*d.color);
    };
    auto draw_stepper=[&](auto& target,Rect r,const StepperOptions& o,bool enabled,bool hovered,bool pressed) {
        auto panel=[&](Rect r,WidgetColor c,float radius=0){layer_panel(target,r,c,radius);};
        const bool borderless=o.tone==ButtonTone::borderless;
        auto base=borderless?WidgetColor{0,0,0,0}:impl_->appearance.primary;
        const auto overlay=!enabled?0.f:pressed?.12f:hovered?.06f:0.f;
        if(!enabled) base=impl_->appearance.disabled_background;
        else if(borderless && pressed) {
            const auto p=impl_->appearance.primary;
            base={.1411764771*.75+p.red*.25,.1411764771*.75+p.green*.25,.1411764771*.75+p.blue*.25,1};
        }
        else if(borderless && hovered) base={.1230252981,.1252803057,.1275353134,1};
        panel(r,base,float(o.corner_radius));
        if(!borderless && overlay>0) panel(r,{0,0,0,overlay},float(o.corner_radius));
        if(o.icon) {
            const auto shade=borderless?1.f:1-overlay;
            target.images.push_back({{r.x+(r.width-16)/2,r.y+(r.height-16)/2,16,16},o.icon,Color{shade,shade,shade,1},enabled?1.f:.38f});
        } else {
            TextRun glyph{.bounds=r,.text=o.arrow==StepperArrow::up?"⌃":o.arrow==StepperArrow::down?"⌄":"↕",.color=color({1,1,1,enabled?1:.38}),.font_size=13,.alignment=TextAlignment::center,.font_family=impl_->appearance.font_family};
            glyph.bounds.y+=(r.height-measure_text(glyph))/2;target.texts.push_back(std::move(glyph));
        }
    };
    auto draw_node=[&](auto& target,std::size_t i) {
        auto panel=[&](Rect r,WidgetColor c,float radius=0){layer_panel(target,r,c,radius);};
        const auto& n=impl_->nodes[i]; const auto r=n.bounds;const bool enabled=impl_->effective_enabled(i)&&impl_->logical_visible(i);if(n.popup || r.width<=0 || r.height<=0 || !impl_->visible(i)) return;
        if(n.drawing){
            for(auto item:n.drawing->panels){item.bounds.x+=r.x;item.bounds.y+=r.y;target.panels.push_back(std::move(item));}
            for(auto item:n.drawing->texts){item.bounds.x+=r.x;item.bounds.y+=r.y;if(item.clip){item.clip->x+=r.x;item.clip->y+=r.y;}target.texts.push_back(std::move(item));}
            for(auto item:n.drawing->images){item.bounds.x+=r.x;item.bounds.y+=r.y;target.images.push_back(std::move(item));}
            return;
        }
        if(n.kind==WidgetKind::image){
            const auto& o=n.image;auto bounds=r;if(o.circular){const auto side=std::min(r.width,r.height);bounds={r.x+(r.width-side)/2,r.y+(r.height-side)/2,side,side};}
            const auto border=float(std::min(o.border_width,double(std::min(bounds.width,bounds.height)/2))),radius=o.circular?std::min(bounds.width,bounds.height)/2:std::min(float(o.corner_radius),std::min(bounds.width,bounds.height)/2);
            if(o.background)panel(bounds,*o.background,radius);if(o.border&&border>0){auto c=color(*o.border);target.panels.push_back({bounds,c,c,radius,border});}
            if(!o.image)return;const auto pad=float(o.padding);Rect image{bounds.x+pad,bounds.y+pad,std::max(0.f,bounds.width-2*pad),std::max(0.f,bounds.height-2*pad)},uv{0,0,1,1};const auto fit=o.fit.value_or(o.preserve_aspect?ImageFit::contain:ImageFit::stretch);
            if(fit==ImageFit::contain){const auto scale=std::min(image.width/o.image->width(),image.height/o.image->height());const auto width=o.image->width()*scale,height=o.image->height()*scale;image.x+=(image.width-width)/2;image.y+=(image.height-height)/2;image.width=width;image.height=height;}
            if(fit==ImageFit::cover&&image.width>0&&image.height>0){const auto source=double(o.image->width())/o.image->height(),destination=double(image.width)/image.height;if(source>destination){uv.width=float(destination/source);uv.x=(1-uv.width)/2;}else{uv.height=float(source/destination);uv.y=(1-uv.height)/2;}}
            if(image.width>0&&image.height>0){ImageRun run{.bounds=image,.image=o.image,.source=uv};if(fit!=ImageFit::contain){run.radius=std::min(std::max(0.f,radius-pad),std::min(image.width,image.height)/2);if(pad==0&&o.border){run.border=color(*o.border);run.border_width=border;}}target.images.push_back(std::move(run));}return;
        }
        if(n.color){n.color->options.enabled=enabled;n.color->paint(target,r,impl_->presentation_surface()?impl_->presentation_surface()->backing_scale():1);return;}
        if(n.color_button){target.images.push_back({r,n.color_button->paint({r.width,r.height},enabled&&impl_->hovered==i,enabled&&impl_->pressed==i,enabled&&impl_->focused==i,impl_->appearance.primary),{},enabled?1.f:.32f});return;}
        if(n.menu_item) {draw_menu_item(target,i);return;}
        if(n.divider) {draw_divider(target,n);return;}
        if(n.kind==WidgetKind::segmented_control) {
            const auto radius=n.segments.cylinder?std::min(r.width,r.height)/2:float(n.segments.corner_radius);
            const auto border=std::min(float(n.segments.border_width),std::min(r.width,r.height)/2);
            panel(r,n.segments.border,radius);
            panel({r.x+border,r.y+border,r.width-2*border,r.height-2*border},n.segments.background,std::max(0.f,radius-border));return;
        }
        if(stack_kind(n.kind) || n.kind==WidgetKind::spacer) return;
        auto run=impl_->text_run(n);if(!enabled&&n.kind!=WidgetKind::label)run.color=color({1,1,1,impl_->appearance.text_alpha[7]});
        if(n.action_surface){
            const auto& o=*n.action_surface;const auto& p=o.padding;
            run.color=color(impl_->action_ink(n));run.bounds={r.x+float(p.left),r.y+float(p.top),std::max(0.f,r.width-float(p.left+p.right)),std::max(0.f,r.height-float(p.top+p.bottom))};
            run.bounds.y+=(run.bounds.height-measure_text(run))/2;
            if(impl_->custom_action_content(n))run.text.clear();
        }else if(n.kind==WidgetKind::combo_box) {
            panel(r,impl_->pressed==i?track:impl_->hovered==i?WidgetColor{.1753742844,.1819191724,.1841008067,1}:WidgetColor{.1555851549,.157072559,.1585599482,1},5);
            draw_stepper(target,{r.x+r.width-19,r.y+(r.height-18)/2,18,18},{.tone=n.combo.tone,.arrow=n.combo.arrow,.icon=n.combo.indicator},enabled,false,false);
            run.bounds.x+=8;run.bounds.width=std::max(0.f,r.width-27);run.bounds.y+=(r.height-measure_text(run))/2;run.color=color({1,1,1,1});
        } else if(n.kind==WidgetKind::stepper) {
            draw_stepper(target,r,n.stepper,enabled,impl_->hovered==i,impl_->pressed==i);run.text.clear();
        } else if(n.kind==WidgetKind::tab) {
            const auto base=n.tab.selected && n.tab.style==TabStyle::surface?WidgetColor{.155585,.157073,.15856,1}:WidgetColor{0,0,0,0};
            panel(r,!enabled?WidgetColor{0,0,0,0}:impl_->pressed==i?WidgetColor{.137095,.140300,.141902,1}:impl_->hovered==i?WidgetColor{.123025,.125280,.127535,1}:base,n.tab.style==TabStyle::surface?4:0);
            if(n.tab.selected && n.tab.style==TabStyle::underline) panel({r.x,r.y+r.height-2,r.width,2},impl_->appearance.primary);
            const auto icon=float(n.tab.icon_size);
            target.images.push_back({{r.x+12,r.y+(r.height-icon)/2,icon,icon},n.tab.icon,{},enabled?1.f:.3f});
            run.bounds.x+=12+icon+8;run.bounds.width=std::max(0.f,r.width-24-icon-8);
            run.bounds.y+=(r.height-measure_text(run))/2;
            run.color=color({1,1,1,enabled?(n.tab.selected?1.0:.9):.3});
            if(!n.tab.badge.empty()) {
                auto badge=run;badge.text=n.tab.badge;badge.font_size=11;badge.font_weight=400;
                badge.bounds.width=measure_text_width(badge);badge.bounds.x=r.x+r.width-12-badge.bounds.width;
                badge.color=color({1,1,1,enabled?.7:.3});badge.bounds.y=r.y+(r.height-measure_text(badge))/2;
                run.bounds.width=std::max(0.f,badge.bounds.x-8-run.bounds.x);target.texts.push_back(std::move(badge));
            }
        } else if(n.kind==WidgetKind::button) {
            auto base=impl_->appearance.primary,hover=impl_->appearance.hover,down=impl_->appearance.pressed;
            if(n.button.tone==ButtonTone::normal) { base=track; hover={.123025,.12528,.127535,1}; }
            if(n.button.tone==ButtonTone::borderless) { base={0,0,0,0}; hover={.123025,.12528,.127535,1}; run.color=color(impl_->appearance.primary); }
            if(n.button.tone==ButtonTone::destructive) { base={1,.276085496,.276085496,1}; hover={base.red/1.12,base.green/1.12,base.blue/1.12,1}; down={base.red/1.2,base.green/1.2,base.blue/1.2,1}; }
            if(n.button.help) base={.095164,.096551,.097938,1};
            auto disabled=impl_->appearance.disabled_background;
            if(n.button.style){const auto& s=*n.button.style;base=s.fill.value_or(base);hover=s.hover_fill.value_or(hover);down=s.pressed_fill.value_or(down);disabled=s.disabled_fill.value_or(disabled);auto text=enabled?s.text:s.disabled_text;if(text)run.color=color(*text);}
            if(n.button.background_image) {
                auto image_rect=r;
                if(impl_->focused==i){image_rect.x+=1;image_rect.y+=1;image_rect.width=std::max(0.f,r.width-2);image_rect.height=std::max(0.f,r.height-2);}
                target.images.push_back({image_rect,n.button.background_image,{},enabled?1.f:.32f});
            } else panel(r,!enabled?disabled:impl_->pressed==i?down:impl_->hovered==i?hover:base,n.button.help?r.height/2:float(n.button.corner_radius.value_or(impl_->appearance.button_radius)));
            if(n.button.style&&n.button.style->border&&n.button.style->border_width>0){const auto radius=std::min(n.button.help?r.height/2:float(n.button.corner_radius.value_or(impl_->appearance.button_radius)),std::min(r.width,r.height)/2);const auto c=color(*n.button.style->border);target.panels.push_back({r,c,c,radius,float(std::min(n.button.style->border_width,double(std::min(r.width,r.height)/2)))});}
            if(n.button.left_padding||n.button.right_padding||n.button.content_spacing||n.button.indicator_size){
                const auto& o=n.button;const float pad=float(o.horizontal_padding.value_or(o.icon_mode?2:impl_->appearance.button_padding));
                const float lp=float(o.left_padding.value_or(pad)),rp=float(o.right_padding.value_or(pad)),icon=o.icon_mode?float(o.icon_size):0,indicator=o.indicator?float(o.indicator_size.value_or(16)):0;
                const float gap=float(o.content_spacing.value_or(o.icon_mode&&!n.text.empty()&&o.tone!=ButtonTone::primary?7:0));
                const float text=n.text.empty()?0:measure_text_width(run),before_text=icon+(icon&&text?gap:0),main=before_text+text,total=std::max(0.f,main+(indicator?indicator+(main?gap:0):0));
                const float left=r.x+lp+std::max(0.f,(r.width-lp-rp-total)/2);
                if(icon)target.images.push_back({{left,r.y+(r.height-icon)/2,icon,icon},o.icon,{},enabled?1.f:.3f});
                run.bounds.x=left+before_text;run.bounds.width=std::min(text,std::max(0.f,r.x+r.width-rp-run.bounds.x-(indicator?indicator+gap:0)));
                if(indicator)target.images.push_back({{left+main+(main?gap:0),r.y+(r.height-indicator)/2,indicator,indicator},!enabled&&o.disabled_indicator?o.disabled_indicator:o.indicator,{},1});
            }else{
            if(n.button.icon_mode) {
                const float icon=float(n.button.icon_size),gap=n.text.empty()?0:(n.button.tone==ButtonTone::primary?0:7);
                const auto total=icon+gap+(n.text.empty()?0:measure_text_width(run))+(n.button.indicator?16:0);
                const auto left=r.x+std::max(0.f,(r.width-total)/2);
                target.images.push_back({{left,r.y+(r.height-icon)/2,icon,icon},n.button.icon,{},enabled?1.f:.3f});
                run.bounds.x=left+icon+gap;run.bounds.width=std::max(0.f,r.x+r.width-run.bounds.x-(n.button.indicator?18:2));
            }
            if(n.button.indicator) {
                target.images.push_back({{r.x+r.width-18,r.y+(r.height-16)/2,16,16},!enabled && n.button.disabled_indicator?n.button.disabled_indicator:n.button.indicator,{},1});
                if(!n.button.icon_mode) run.bounds.width=std::max(0.f,r.width-18);
            }
            }
            run.alignment=TextAlignment::center; run.bounds.y+=(r.height-measure_text(run))/2;
        } else if(check_kind(n.kind)) {
            const float width=n.kind==WidgetKind::toggle_switch?38:17,height=n.kind==WidgetKind::toggle_switch?22:17;
            Rect indicator{r.x,r.y+(r.height-height)/2,width,height};
            panel(indicator,enabled?(n.checked?impl_->appearance.primary:track):impl_->appearance.disabled_background,n.kind==WidgetKind::check_box?3.5f:height/2);
            if(n.kind==WidgetKind::toggle_switch) panel({indicator.x+(n.checked?width-19:2),indicator.y+2,18,18},white,9);
            else if(n.checked && n.kind==WidgetKind::radio_button) panel({indicator.x+4.5f,indicator.y+4.5f,8,8},white,4);
            else if(n.checked) { auto mark=run; mark.text="✓"; mark.bounds=indicator; mark.font_size=14; mark.font_weight=700; mark.alignment=TextAlignment::center; target.texts.push_back(mark); }
            run.bounds.x+=width+8; run.bounds.width=std::max(0.f,r.width-width-8); run.bounds.y+=(r.height-measure_text(run))/2;
        } else if(n.kind==WidgetKind::input_field) {
            const auto& style=n.field.style;const float inset=style?float(style->horizontal_padding):8;
            const auto base=style&&style->background?*style->background:WidgetColor{.155585,.157073,.15856,enabled?1:.36};
            const auto hover=style?style->hover_background.value_or(base):WidgetColor{.175374,.181919,.184101,1};
            panel(r,impl_->hovered==i?hover:base,style?float(style->corner_radius):8);
            run.bounds={r.x+inset,r.y,std::max(0.f,r.width-inset*2),r.height};run.bounds.y+=(r.height-measure_text(run))/2;
            if(n.text.empty() && n.marked.empty()) { run.text=n.field.placeholder; run.color=color({1,1,1,.5}); }
            else if(!n.marked.empty()){const auto begin=std::min(n.anchor,n.cursor),end=std::max(n.anchor,n.cursor);run.text.replace(begin,end-begin,n.marked);}
            if(n.field.password&&!run.text.empty()&&(!n.text.empty()||!n.marked.empty())){const auto bullets=utf16_count(run.text);run.text.clear();for(std::size_t bullet=0;bullet<bullets;++bullet)run.text+="\xe2\x80\xa2";}
            if(impl_->focused==i && !n.field.read_only) {
                auto prefix=run; prefix.text=n.text.substr(0,n.cursor);if(n.field.password){prefix.text.clear();for(std::size_t bullet=0;bullet<utf16_count(n.text.substr(0,n.cursor));++bullet)prefix.text+="\xe2\x80\xa2";} const auto x=std::max(r.x,std::min(r.x+r.width-inset-1,run.bounds.x+measure_text_width(prefix)));
                const auto vertical=std::min(6.f,r.height/4);panel({x,r.y+vertical,1,r.height-vertical*2},white);
                if(!n.marked.empty()) { auto marked=run; marked.text=n.marked; panel({x,r.y+r.height-2,std::max(0.f,std::min(measure_text_width(marked),r.width-inset*2)),1},impl_->appearance.primary); }
            }
        } else if(n.kind==WidgetKind::slider) {
            const auto inset=float(n.slider.range_inset),thumb=float(n.slider.thumb_diameter),height=float(n.slider.track_height);
            const auto extent=std::max(1.f,r.width-2*inset);const auto t=float((n.slider.value-n.slider.minimum)/(n.slider.maximum-n.slider.minimum));
            const Rect track_rect{r.x+inset,r.y+(r.height-height)/2,extent,height};
            if(n.slider.background_image)target.images.push_back({track_rect,n.slider.background_image,{},enabled?1.f:.32f});
            else {panel(track_rect,track,height/2);panel({track_rect.x,track_rect.y,extent*t,height},impl_->appearance.primary,height/2);}
            const Rect thumb_rect{r.x+inset+extent*t-thumb/2,r.y+(r.height-thumb)/2,thumb,thumb};
            if(n.slider.background_image)target.images.push_back({thumb_rect,slider_thumb_image(),{},enabled?1.f:1.f/3});
            else panel(thumb_rect,enabled?white:WidgetColor{1,1,1,.3},thumb/2);return;
        } else if(n.kind==WidgetKind::progress) {
            panel(r,{0,0,0,13.0/255},float(n.progress.height/2));
            const auto span=n.progress.maximum-n.progress.minimum;
            const auto a=span?std::clamp((n.progress.start-n.progress.minimum)/span,0.0,1.0):0;
            const auto b=span?std::clamp((n.progress.value-n.progress.minimum)/span,0.0,1.0):0;
            panel({r.x+float(std::min(a,b))*r.width,r.y,float(std::abs(b-a))*r.width,r.height},impl_->appearance.primary,float(n.progress.height/2)); return;
        }
        if(impl_->focused==i && interactive(n.kind) && (!n.action_surface||n.action_surface->focus_ring) && (n.kind!=WidgetKind::input_field||!n.field.style||n.field.style->focus_ring)) {
            const auto c=impl_->appearance.primary;
            if(n.action_surface){const auto ink=color(c);target.panels.push_back({r,ink,ink,float(std::min(n.action_surface->corner_radius,double(std::min(r.width,r.height)/2))),1.5f});}
            else if(n.kind==WidgetKind::button) {
                const auto radius=std::min(n.button.help?r.height/2:float(n.button.corner_radius.value_or(impl_->appearance.button_radius)),std::min(r.width,r.height)/2);
                const auto ink=color(c);
                target.panels.push_back({r,ink,ink,radius,1.5f});
            } else {
                panel({r.x,r.y,r.width,1},c); panel({r.x,r.y+r.height-1,r.width,1},c); panel({r.x,r.y,1,r.height},c); panel({r.x+r.width-1,r.y,1,r.height},c);
            }
        }
        if(!run.text.empty() && run.bounds.width>0 && run.bounds.height>0) target.texts.push_back(std::move(run));
    };
    std::vector<CanvasLayer> portals;
    const auto compose=[](ContentTransform outer,ContentTransform inner){
        const auto x=outer.origin_x*(1-outer.scale)+outer.offset_x+outer.scale*(inner.origin_x*(1-inner.scale)+inner.offset_x);
        const auto y=outer.origin_y*(1-outer.scale)+outer.offset_y+outer.scale*(inner.origin_y*(1-inner.scale)+inner.offset_y);
        return ContentTransform{.scale=outer.scale*inner.scale,.opacity=outer.opacity*inner.opacity,.offset_x=x,.offset_y=y};
    };
    const auto append_embedded=[&](const std::shared_ptr<WidgetState>& child,Rect body,ContentTransform inherited,std::optional<Rect> clip){
        std::size_t popup_start=0;auto scene=child->frame({body.width,body.height},&popup_start);
        const auto translate=[&](CanvasLayer& layer){
            for(auto& v:layer.panels){v.bounds.x+=body.x;v.bounds.y+=body.y;}
            for(auto& v:layer.texts){v.bounds.x+=body.x;v.bounds.y+=body.y;if(v.clip){v.clip->x+=body.x;v.clip->y+=body.y;}}
            for(auto& v:layer.images){v.bounds.x+=body.x;v.bounds.y+=body.y;}
            if(layer.clip){layer.clip->x+=body.x;layer.clip->y+=body.y;}
            if(layer.backdrop){layer.backdrop->bounds.x+=body.x;layer.backdrop->bounds.y+=body.y;if(layer.backdrop->underlay){layer.backdrop->underlay_bounds.x+=body.x;layer.backdrop->underlay_bounds.y+=body.y;}}
            if(layer.shadow){layer.shadow->bounds.x+=body.x;layer.shadow->bounds.y+=body.y;}
            layer.content_transform.origin_x+=body.x;layer.content_transform.origin_y+=body.y;
        };
        CanvasLayer base{.panels=std::move(scene.panels),.texts=std::move(scene.texts),.images=std::move(scene.images),.images_before_text=scene.images_before_text};translate(base);base.clip=clip;base.content_transform=compose(inherited,base.content_transform);
        if(!base.panels.empty()||!base.texts.empty()||!base.images.empty())f.overlays.push_back(std::move(base));
        for(std::size_t j=0;j<scene.overlays.size();++j){auto layer=std::move(scene.overlays[j]);translate(layer);
            if(j>=popup_start)portals.push_back(std::move(layer));
            else{if(clip)layer.clip=layer.clip?intersect(*layer.clip,*clip):clip;layer.content_transform=compose(inherited,layer.content_transform);f.overlays.push_back(std::move(layer));}
        }
    };
    const auto flush_portals=[&]{for(auto& layer:portals)f.overlays.push_back(std::move(layer));portals.clear();};
    // Consecutive nodes sharing a clip reuse one layer. A viewport boundary
    // starts/restores a hardware scissor without changing text/image sampling.
    auto draw_tree=[&](auto&& self,std::size_t i,std::optional<std::size_t>& batch,ContentTransform transform)->void {
        const auto& n=impl_->nodes[i];if(n.popup||!impl_->visible(i))return;
        auto use_clip=[&](std::optional<Rect> clip){if(batch&&*batch==f.overlays.size()){f.overlays.push_back({.clip=clip,.content_transform=transform});return;}const auto current=batch?f.overlays[*batch].clip:std::optional<Rect>{};if(current!=clip){batch=f.overlays.size();f.overlays.push_back({.clip=clip,.content_transform=transform});}};
        const bool provided=bool(n.paint_handler)||(n.drawing&&(n.drawing->images_before_text||n.drawing->backdrop||n.drawing->shadow));
        if(!provided)use_clip(impl_->clip_rect(i));
        if(provided){
            const auto source=n.paint_source==absent?i:n.paint_source;const auto& owner=impl_->nodes[source];
            const WidgetPaintState state{{n.bounds.width,n.bounds.height},impl_->effective_enabled(i),impl_->hovered!=absent&&impl_->descendant(impl_->hovered,source),impl_->pressed==source,impl_->focused==source,owner.checked,impl_->nodes[owner.default_content==absent?source:owner.default_content].children.size(),impl_->presentation_surface()?impl_->presentation_surface()->backing_scale():1};
            struct Guard{bool& value;explicit Guard(bool& v):value(v){value=true;}~Guard(){value=false;}}guard{impl_->painting};
            core::Result<std::shared_ptr<const WidgetDrawing>> drawing=n.paint_handler?n.paint_handler(state):core::Result<std::shared_ptr<const WidgetDrawing>>{std::make_shared<const WidgetDrawing>(*n.drawing)};if(!drawing){if(drawing.error().cause)std::rethrow_exception(drawing.error().cause);throw std::runtime_error(drawing.error().message);}
            if(*drawing){const auto& d=**drawing;CanvasFrame check{.viewport={0,0,8192,8192},.overlays={{.panels=d.panels,.texts=d.texts,.images=d.images,.backdrop=d.backdrop,.shadow=d.shadow}}};if(!valid_canvas(check))throw std::runtime_error("Paint provider returned invalid native drawing");
                CanvasLayer layer{.panels=d.panels,.texts=d.texts,.images=d.images,.clip=impl_->clip_rect(i),.backdrop=d.backdrop,.shadow=d.shadow,.content_transform=transform,.images_before_text=d.images_before_text};
                for(auto& p:layer.panels){p.bounds.x+=n.bounds.x;p.bounds.y+=n.bounds.y;}for(auto& t:layer.texts){t.bounds.x+=n.bounds.x;t.bounds.y+=n.bounds.y;if(t.clip){t.clip->x+=n.bounds.x;t.clip->y+=n.bounds.y;}}for(auto& image:layer.images){image.bounds.x+=n.bounds.x;image.bounds.y+=n.bounds.y;}
                const auto translate=[&](Rect& r){r.x+=n.bounds.x;r.y+=n.bounds.y;};
                if(layer.backdrop){translate(layer.backdrop->bounds);if(layer.backdrop->underlay)translate(layer.backdrop->underlay_bounds);}
                if(layer.shadow)translate(layer.shadow->bounds);
                f.overlays.push_back(std::move(layer));batch=f.overlays.size();
            }
        }else if(batch)draw_node(f.overlays[*batch],i);else draw_node(f,i);
        if(n.content&&n.content->view){auto clip=impl_->clip_rect(i);clip=clip?intersect(*clip,n.bounds):n.bounds;append_embedded(n.content->view,n.bounds,transform,clip);batch=f.overlays.size();}
        for(auto child:n.children)self(self,child,batch,transform);
        if(n.scroll&&n.scroll->options.enabled&&n.scroll->options.show_indicator&&n.bounds.width>0&&n.bounds.height>0&&(n.scroll->content_height>n.bounds.height||n.scroll->content_width>n.bounds.width)){
            auto clip=impl_->clip_rect(i);use_clip(clip?intersect(*clip,n.bounds):n.bounds);
            const bool vertical=n.scroll->content_height>n.bounds.height,horizontal=n.scroll->content_width>n.bounds.width;
            const auto indicator=[&](Rect thumb){const auto radius=std::min({1.5f,thumb.width/2,thumb.height/2});if(batch)layer_panel(f.overlays[*batch],thumb,{1,1,1,.35},radius);else layer_panel(f,thumb,{1,1,1,.35},radius);};
            if(vertical){const auto track=std::max(0.,double(n.bounds.height)-(horizontal?5:0));if(track>0){
                const auto height=std::min(track,std::max(18.,track*n.bounds.height/n.scroll->content_height));
                const auto y=n.bounds.y+(track-height)*n.scroll->offset/(n.scroll->content_height-n.bounds.height);
                indicator({n.bounds.x+std::max(0.f,n.bounds.width-5),float(y),std::min(3.f,n.bounds.width),float(height)});
            }}
            if(horizontal){const auto track=std::max(0.,double(n.bounds.width)-(vertical?5:0));if(track>0){
                const auto width=std::min(track,std::max(18.,track*n.bounds.width/n.scroll->content_width));
                const auto x=n.bounds.x+(track-width)*n.scroll->horizontal_offset/(n.scroll->content_width-n.bounds.width);
                indicator({float(x),n.bounds.y+std::max(0.f,n.bounds.height-5),float(width),std::min(3.f,n.bounds.height)});
            }}
        }
    };
    std::optional<std::size_t> base_batch;if(!impl_->nodes.empty()&&!impl_->nodes[0].popup)draw_tree(draw_tree,0,base_batch,{});
    if(popup_start)*popup_start=f.overlays.size();flush_portals();
    for(auto index:impl_->visual_popups) {
        const auto& n=impl_->nodes[index];const auto& p=*n.popup;if(!impl_->visible(index)||n.bounds.width<=0||n.bounds.height<=0)continue;
        const bool animated=p.popover&&p.popover->transition.has_value();
        if(p.popover&&p.popover->dim){auto tint=p.popover->backdrop_color;if(animated)tint.alpha*=std::clamp(p.motion.progress,0.,1.);auto& dim=f.overlays.emplace_back();layer_panel(dim,impl_->popup_viewport(size),tint);}
        auto background=p.options.background.value_or(impl_->appearance.window);background.alpha*=p.options.opacity;
        const auto radius=p.popover&&p.popover->cylinder?std::min(n.bounds.width,n.bounds.height)/2:std::min(float(p.options.corner_radius),std::min(n.bounds.width,n.bounds.height)*float(p.popover?p.popover->corner_limit:.5));
        auto& backdrop=f.overlays.emplace_back();
        if(p.popover&&p.popover->shadow){const auto& shadow=*p.popover->shadow;backdrop.shadow=BoxShadow{.bounds={n.bounds.x+float(shadow.offset_x),n.bounds.y+float(shadow.offset_y),n.bounds.width,n.bounds.height},.radius=radius,.blur_radius=float(shadow.blur_radius),.color=color(shadow.color)};}
        if(animated){
            const auto& o=*p.popover;backdrop.content_transform=impl_->transform(index);
            // The authored shadow and glass are unscaled siblings of the card.
            // Glass closes immediately; the retained card alone fades out.
            if(p.opened&&o.blur_radius)backdrop.backdrop=BackdropMaterial{.bounds=n.bounds,.radius=radius,.blur_radius=float(*o.blur_radius),.source=BackdropSource::content};
            if(o.opaque_base&&!o.blur_radius)layer_panel(backdrop,n.bounds,{20./255,20./255,20./255,1},radius);
            const auto border=float(std::min(o.border_width,double(std::min(n.bounds.width,n.bounds.height)/2)));
            if(border>0&&o.border_color.alpha>0){
                layer_panel(backdrop,{n.bounds.x+border,n.bounds.y+border,std::max(0.f,n.bounds.width-2*border),std::max(0.f,n.bounds.height-2*border)},background,std::max(0.f,radius-border));
                auto c=color(o.border_color);backdrop.panels.push_back({n.bounds,c,c,radius,border});
            }else layer_panel(backdrop,n.bounds,background,radius);
        }else if(p.popover&&(!p.popover->opaque_base||p.popover->blur_radius)){const auto& o=*p.popover;backdrop.backdrop=BackdropMaterial{.bounds=n.bounds,.radius=radius,.blur_radius=float(o.blur_radius.value_or(0)),.tint=color(background),.border=color(o.border_color),.border_width=float(o.border_width),.source=o.blur_radius?BackdropSource::content:BackdropSource::previous_layers};}
        else{layer_panel(backdrop,n.bounds,{20.0/255,20.0/255,20.0/255,1},radius);layer_panel(backdrop,n.bounds,background,radius);}
        const auto content_index=f.overlays.size();f.overlays.emplace_back();if(!p.popover||p.popover->clip_content)f.overlays[content_index].clip=p.content;if(animated)f.overlays[content_index].content_transform=impl_->transform(index);
        if(p.popover){std::optional<std::size_t> batch=content_index;for(auto child:n.children)if(!impl_->nodes[child].popup)draw_tree(draw_tree,child,batch,animated?impl_->transform(index):ContentTransform{});}
        else for(auto entry:p.entries)if(impl_->visible(entry)){const auto& node=impl_->nodes[entry];if(node.menu_item)draw_menu_item(f.overlays[content_index],entry);else if(node.divider)draw_divider(f.overlays[content_index],node);}
        if(p.content_view)append_embedded(p.content_view,impl_->activation_rect(index),animated?impl_->transform(index):ContentTransform{},(!p.popover||p.popover->clip_content)?std::optional{p.content}:std::nullopt);
        flush_portals();
    }
    const auto empty=[](const CanvasLayer& layer){return layer.panels.empty()&&layer.texts.empty()&&layer.images.empty()&&!layer.backdrop&&!layer.shadow;};
    if(popup_start){std::size_t removed=0;for(std::size_t i=0;i<*popup_start;++i)if(empty(f.overlays[i]))++removed;*popup_start-=removed;}
    std::erase_if(f.overlays,empty);
    return f;
}
core::Result<GraphicsInfo> WidgetState::graphics_info() {
    if(auto r=ready();!r) return std::unexpected(r.error());
    if(auto r=flush_viewports();!r)return std::unexpected(r.error());
    if(!impl_->surface) return core::fail(core::ErrorCode::not_found,"No widget surface");
    try { impl_->surface->flush(); return impl_->surface->info(); } catch(const std::exception& e) { return core::fail(core::ErrorCode::failed,e.what()); }
}
void WidgetState::present() { if(impl_->active && impl_->surface){if(auto r=flush_viewports();!r){if(r.error().code!=core::ErrorCode::closed)throw std::runtime_error(r.error().message);return;}if(impl_->active&&impl_->surface)impl_->surface->flush();} }
void WidgetState::set_frame_paced(bool value) { if(impl_->active && impl_->surface) impl_->surface->set_frame_paced(value); }
core::Result<PixelBuffer> WidgetState::capture() {
    if(auto r=ready();!r) return std::unexpected(r.error());
    if(auto r=flush_viewports();!r)return std::unexpected(r.error());
    if(!impl_->surface) return core::fail(core::ErrorCode::not_found,"No widget surface");
    try { impl_->surface->flush(); return impl_->surface->capture(); } catch(const std::exception& e) { return core::fail(core::ErrorCode::failed,e.what()); }
}
} // namespace ii::gui::detail

namespace ii::gui::detail { bool WidgetState::native_event(const void* event){return impl_->active&&impl_->surface?impl_->surface->native_event(event):false;} }
