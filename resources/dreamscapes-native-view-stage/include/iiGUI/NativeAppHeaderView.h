#pragma once
#include <iiGUI/AppHeaderModel.h>
#include <iiGUI/NativeWidgets.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <exception>
#include <functional>
#include <limits>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

namespace ii::gui {
// Retained action slot and focus/input lifecycle are owned by iiGUI.
class NativeAppHeader final {
    struct State final {
        AppHeaderModel view;
        ii::gui::Widget root,menu,title_group,title,subtitle,actions;
        std::function<void()> clicked;
        std::uint64_t revision=0;
        bool live=false,retired=false;
    };
    std::shared_ptr<State> state_;
    explicit NativeAppHeader(std::shared_ptr<State> state):state_(std::move(state)){}
    [[nodiscard]] static ii::gui::WidgetColor color(WidgetColor value)noexcept{return {value.red,value.green,value.blue,value.alpha};}
    [[nodiscard]] static ii::gui::Color linear(WidgetColor value)noexcept {
        const auto channel=[](double c){return c<=.04045?c/12.92:std::pow((c+.055)/1.055,2.4);};
        return {float(channel(value.red)),float(channel(value.green)),float(channel(value.blue)),float(value.alpha)};
    }
    [[nodiscard]] static ii::gui::StackLayoutOptions layout(const AppHeaderModel& view){
        const auto m=view.resolved();const auto& value=view.configuration();
        return {.stack={.spacing=m.row_spacing,.alignment=ii::gui::StackAlignment::center,
            .insets=ii::gui::Insets{m.horizontal_padding,m.vertical_padding,m.horizontal_padding,m.vertical_padding},.minimum_height=value.minimum_height,
            .implicit_insets=ii::gui::Insets{m.horizontal_padding,value.implicit_vertical_padding,m.horizontal_padding,value.implicit_vertical_padding}},.extent={value.width,value.height}};
    }
    [[nodiscard]] static ii::gui::ActionOptions menu_options(const AppHeaderModel& view){
        const auto m=view.resolved();const auto& value=view.configuration();
        return {.accessibility_name="Menu",.enabled=value.enabled,.focus_ring=false,.role=ii::gui::ActionRole::button,
            .text_role=ii::gui::TextRole::description,.padding={m.menu_padding,0,m.menu_padding,0},
            .minimum_height=m.menu_height,.corner_radius=value.menu_corner_radius,.width=2*m.menu_padding+value.menu_icon_width,.height=m.menu_height};
    }
    [[nodiscard]] static ii::gui::LabelOptions label(const std::string& text,WidgetColor ink,double font_size,unsigned weight,bool title){
        return {.text=text,.font_size=font_size,.selectable=false,.role=title?ii::gui::TextRole::title2:ii::gui::TextRole::description,
            .color=color(ink),.line_height=font_size,.font_weight=weight,.elide_right=true};
    }
    [[nodiscard]] static ii::core::Result<void> apply(const std::shared_ptr<State>& state,std::uint64_t revision,
                                                       const AppHeaderModel* previous=nullptr){
        const auto& value=state->view.configuration();const auto m=state->view.resolved();
        const auto* old=previous?&previous->configuration():nullptr;
        const auto old_metrics=previous?previous->resolved():AppHeaderMetrics{};
        const auto still_current=[&]{return state->revision==revision;};
        if(!old||old->width!=value.width||old->height!=value.height||old->minimum_height!=value.minimum_height||
           old->implicit_vertical_padding!=value.implicit_vertical_padding||old_metrics.horizontal_padding!=m.horizontal_padding||
           old_metrics.vertical_padding!=m.vertical_padding||old_metrics.row_spacing!=m.row_spacing){
            if(auto changed=state->root.set_stack_layout(layout(state->view));!changed)return changed;if(!still_current())return {};
        }
        if(!old||old->title_spacing!=value.title_spacing){
            if(auto changed=state->title_group.set_stack_options({.spacing=value.title_spacing,.alignment=ii::gui::StackAlignment::leading});!changed)return changed;if(!still_current())return {};
        }
        if(!old||old->title!=value.title||old->title_color!=value.title_color||old->title_size!=value.title_size||old->title_weight!=value.title_weight){
            if(auto changed=state->title.set_label_options(label(value.title,value.title_color,value.title_size,value.title_weight,true));!changed)return changed;if(!still_current())return {};
        }
        if(!old||old->subtitle!=value.subtitle||old->subtitle_color!=value.subtitle_color||old->subtitle_size!=value.subtitle_size||old->subtitle_weight!=value.subtitle_weight){
            if(auto changed=state->subtitle.set_label_options(label(value.subtitle,value.subtitle_color,value.subtitle_size,value.subtitle_weight,false));!changed)return changed;if(!still_current())return {};
        }
        if(!old||old->title.empty()!=value.title.empty()){
            if(auto changed=state->title.set_visible(!value.title.empty());!changed)return changed;if(!still_current())return {};
        }
        if(!old||old->subtitle.empty()!=value.subtitle.empty()){
            if(auto changed=state->subtitle.set_visible(!value.subtitle.empty());!changed)return changed;if(!still_current())return {};
        }
        if(!old||old->enabled!=value.enabled||old->menu_corner_radius!=value.menu_corner_radius||old->menu_icon_width!=value.menu_icon_width||
           old_metrics.menu_padding!=m.menu_padding||old_metrics.menu_height!=m.menu_height){
            if(auto changed=state->menu.set_action_options(menu_options(state->view));!changed)return changed;if(!still_current())return {};
        }
        if(!old||old->menu_visible!=value.menu_visible){
            if(auto changed=state->menu.set_visible(value.menu_visible);!changed)return changed;if(!still_current())return {};
        }
        if(!old||old_metrics.action_spacing!=m.action_spacing){
            if(auto changed=state->actions.set_stack_options({.spacing=m.action_spacing,.alignment=ii::gui::StackAlignment::center});!changed)return changed;if(!still_current())return {};
        }
        if(old&&(old->menu_focus!=value.menu_focus||old->menu_background!=value.menu_background||
                 old->menu_pressed_background!=value.menu_pressed_background||old->title_color!=value.title_color||
                 old->focus_ring_width!=value.focus_ring_width||old->menu_line_thickness!=value.menu_line_thickness||
                 old->menu_line_spacing!=value.menu_line_spacing)){
            if(auto changed=state->menu.request_repaint();!changed)return changed;if(!still_current())return {};
        }
        if(old&&old->background!=value.background){
            if(auto changed=state->root.request_repaint();!changed)return changed;
        }
        return {};
    }
public:
    [[nodiscard]] static ii::core::Result<NativeAppHeader> create(ii::gui::WidgetBuilder& builder,ii::gui::Widget parent,
            std::string_view id,const AppHeaderOptions& value={},std::function<void()> clicked={}){
        if(!AppHeaderModel::validate(value))return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid AppHeader configuration");
        try {
            auto state=std::make_shared<State>();(void)state->view.set_configuration(value);state->clicked=std::move(clicked);const std::string prefix(id);
            const auto m=state->view.resolved();std::weak_ptr<State> weak=state;
            auto root=builder.hstack(parent,id,{.spacing=m.row_spacing,.alignment=ii::gui::StackAlignment::center});
            if(!root)return std::unexpected(root.error());state->root=*root;
            auto menu=builder.action_surface(*root,prefix+"/@menu",menu_options(state->view),[weak]{if(auto state=weak.lock();state&&state->live){auto callback=state->clicked;if(callback)callback();}});
            if(!menu)return std::unexpected(menu.error());state->menu=*menu;
            auto group=builder.vstack(*root,prefix+"/@title-group",{.spacing=value.title_spacing,.alignment=ii::gui::StackAlignment::leading});
            if(!group)return std::unexpected(group.error());state->title_group=*group;
            if(auto flex=group->set_flex(1);!flex)return std::unexpected(flex.error());
            auto title=builder.label(*group,prefix+"/@title",label(value.title,value.title_color,value.title_size,value.title_weight,true));if(!title)return std::unexpected(title.error());state->title=*title;
            auto subtitle=builder.label(*group,prefix+"/@subtitle",label(value.subtitle,value.subtitle_color,value.subtitle_size,value.subtitle_weight,false));if(!subtitle)return std::unexpected(subtitle.error());state->subtitle=*subtitle;
            auto actions=builder.hstack(*root,prefix+"/@actions",{.spacing=m.action_spacing,.alignment=ii::gui::StackAlignment::center});
            if(!actions)return std::unexpected(actions.error());state->actions=*actions;
            if(auto clipped=root->set_clip_content(true);!clipped)return std::unexpected(clipped.error());
            if(auto installed=root->set_paint_handler([state](const ii::gui::WidgetPaintState& paint)->ii::core::Result<std::shared_ptr<const ii::gui::WidgetDrawing>>{
                auto drawing=std::make_shared<ii::gui::WidgetDrawing>();const auto ink=linear(state->view.configuration().background);
                if(paint.size.width>0&&paint.size.height>0&&ink.alpha>0)drawing->panels.push_back({{0,0,float(paint.size.width),float(paint.size.height)},ink,ink,0});
                return std::shared_ptr<const ii::gui::WidgetDrawing>(std::move(drawing));
            });!installed)return std::unexpected(installed.error());
            if(auto installed=menu->set_paint_handler([state](const ii::gui::WidgetPaintState& paint)->ii::core::Result<std::shared_ptr<const ii::gui::WidgetDrawing>>{
                auto drawing=std::make_shared<ii::gui::WidgetDrawing>();const auto& value=state->view.configuration();
                auto fill=linear(paint.pressed?value.menu_pressed_background:value.menu_background);
                if(paint.size.width>0&&paint.size.height>0&&fill.alpha>0)drawing->panels.push_back({{0,0,float(paint.size.width),float(paint.size.height)},fill,fill,float(value.menu_corner_radius)});
                if(paint.focused&&paint.enabled&&paint.size.width>0&&paint.size.height>0){
                    const auto focus=linear(value.menu_focus);const auto stroke=(std::min)({value.focus_ring_width,paint.size.width/2,paint.size.height/2});
                    if(stroke>0)drawing->panels.push_back({{0,0,float(paint.size.width),float(paint.size.height)},focus,focus,float(value.menu_corner_radius),float(stroke)});
                }
                auto ink=linear(value.title_color);if(!paint.enabled)ink.alpha*=.3f;
                const auto width=(std::min)(value.menu_icon_width,paint.size.width),height=(std::min)(value.menu_line_thickness,paint.size.height/6),left=(paint.size.width-width)/2;
                const auto line_spacing=(std::min)(value.menu_line_spacing,(std::max)(0.,(paint.size.height-height)/2));
                if(width>0&&height>0)for(int line=-1;line<=1;++line)drawing->panels.push_back({{float(left),float(paint.size.height/2+line*line_spacing-height/2),float(width),float(height)},ink,ink,0});
                return std::shared_ptr<const ii::gui::WidgetDrawing>(std::move(drawing));
            });!installed)return std::unexpected(installed.error());
            if(auto configured=apply(state,state->revision);!configured)return std::unexpected(configured.error());
            if(auto enabled=root->set_enabled(value.enabled);!enabled)return std::unexpected(enabled.error());
            if(auto registered=builder.on_publish([weak]{if(auto state=weak.lock();state&&!state->retired)state->live=true;});!registered)return std::unexpected(registered.error());
            if(auto registered=builder.on_retire([weak]{if(auto state=weak.lock()){state->retired=true;state->live=false;state->clicked={};}});!registered)return std::unexpected(registered.error());
            return NativeAppHeader(std::move(state));
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"AppHeader construction failed",std::current_exception());}
    }
    [[nodiscard]] ii::gui::Widget root()const{return state_->root;}
    [[nodiscard]] ii::gui::Widget menu()const{return state_->menu;}
    [[nodiscard]] ii::gui::Widget actions_root()const{return state_->actions;}
    [[nodiscard]] ii::core::Result<void> set_menu_clicked(std::function<void()> callback)const {
        if(auto current=state_->root.snapshot();!current)return std::unexpected(current.error());
        state_->clicked.swap(callback);return {};
    }
    [[nodiscard]] ii::core::Result<void> set_options(AppHeaderOptions value)const {
        if(!AppHeaderModel::validate(value))return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid AppHeader configuration");
        try {
            if(auto current=state_->root.snapshot();!current)return std::unexpected(current.error());
            if(state_->view.configuration()==value)return {};
            if(state_->revision==(std::numeric_limits<std::uint64_t>::max)())return ii::core::fail(ii::core::ErrorCode::out_of_range,"AppHeader revision exhausted");
            const auto revision=++state_->revision;auto previous=state_->view;(void)state_->view.set_configuration(std::move(value));
            if(auto configured=apply(state_,revision,&previous);!configured){if(state_->revision==revision)state_->view=std::move(previous);return configured;}
            if(state_->revision!=revision)return {};
            if(previous.configuration().enabled!=state_->view.configuration().enabled){
                if(auto enabled=state_->root.set_enabled(state_->view.configuration().enabled);!enabled)return enabled;
            }
            return {};
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"AppHeader update callback failed",std::current_exception());}
    }
};
} // namespace ii::gui
