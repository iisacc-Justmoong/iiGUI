#pragma once
#include <iiGUI/AppCardModel.h>
#include <iiGUI/NativeWidgets.h>
#include <cmath>
#include <cstdint>
#include <exception>
#include <limits>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

namespace ii::gui {
// Retained body, independent of the application's declaration language.
class NativeAppCard final {
    struct State final {
        AppCardModel view;
        ii::gui::Widget root,header,title,subtitle,separator,content;
        std::uint64_t revision=0;
    };
    std::shared_ptr<State> state_;
    explicit NativeAppCard(std::shared_ptr<State> state):state_(std::move(state)){}
    [[nodiscard]] static ii::gui::WidgetColor color(WidgetColor value)noexcept{return {value.red,value.green,value.blue,value.alpha};}
    [[nodiscard]] static ii::gui::Color linear(WidgetColor value)noexcept {
        const auto channel=[](double c){return c<=.04045?c/12.92:std::pow((c+.055)/1.055,2.4);};
        return {float(channel(value.red)),float(channel(value.green)),float(channel(value.blue)),float(value.alpha)};
    }
    [[nodiscard]] static ii::gui::StackLayoutOptions layout(const AppCardOptions& value){
        return {.stack={.spacing=value.spacing,.padding=value.padding,.alignment=ii::gui::StackAlignment::stretch,
                        .minimum_width=value.width?0.:value.minimum_width},.extent={value.width,value.height}};
    }
    [[nodiscard]] static ii::gui::LabelOptions label(const std::string& text,WidgetColor ink,double font_size,bool title){
        return {.text=text,.font_size=font_size,.selectable=false,
            .role=title?ii::gui::TextRole::header:ii::gui::TextRole::description,.color=color(ink),
            .line_height=font_size,.font_weight=600,.elide_right=true};
    }
    [[nodiscard]] static ii::core::Result<void> apply(const std::shared_ptr<State>& state,const AppCardOptions& value){
        if(auto changed=state->root.set_stack_layout(layout(value));!changed)return changed;
        if(auto changed=state->header.set_stack_options({.spacing=value.heading_spacing,.alignment=ii::gui::StackAlignment::stretch});!changed)return changed;
        if(auto changed=state->separator.set_extent({0,value.separator_height});!changed)return changed;
        if(auto changed=state->title.set_label_options(label(value.title,value.title_color,value.title_size,true));!changed)return changed;
        if(auto changed=state->subtitle.set_label_options(label(value.subtitle,value.subtitle_color,value.subtitle_size,false));!changed)return changed;
        if(auto changed=state->title.set_visible(!value.title.empty());!changed)return changed;
        if(auto changed=state->subtitle.set_visible(!value.subtitle.empty());!changed)return changed;
        const auto heading=!value.title.empty()||!value.subtitle.empty();
        if(auto changed=state->header.set_visible(heading);!changed)return changed;
        if(auto changed=state->separator.set_visible(heading);!changed)return changed;
        return {};
    }
public:
    [[nodiscard]] static ii::core::Result<NativeAppCard> create(ii::gui::WidgetBuilder& builder,ii::gui::Widget parent,
            std::string_view id,const AppCardOptions& value={}){
        if(!AppCardModel::validate(value))return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid AppCard configuration");
        auto state=std::make_shared<State>();(void)state->view.set_configuration(value);const std::string prefix(id);
        auto root=builder.vstack(parent,id,{.spacing=value.spacing,.padding=value.padding,.alignment=ii::gui::StackAlignment::stretch,
                                           .minimum_width=value.width?0.:value.minimum_width});
        if(!root)return std::unexpected(root.error());state->root=*root;
        auto header=builder.vstack(*root,prefix+"/@header",{.spacing=value.heading_spacing,.alignment=ii::gui::StackAlignment::stretch});
        if(!header)return std::unexpected(header.error());state->header=*header;
        auto title=builder.label(*header,prefix+"/@title",label(value.title,value.title_color,value.title_size,true));
        if(!title)return std::unexpected(title.error());state->title=*title;
        auto subtitle=builder.label(*header,prefix+"/@subtitle",label(value.subtitle,value.subtitle_color,value.subtitle_size,false));
        if(!subtitle)return std::unexpected(subtitle.error());state->subtitle=*subtitle;
        auto separator=builder.zstack(*root,prefix+"/@separator",{.spacing=0,.alignment=ii::gui::StackAlignment::stretch});
        if(!separator)return std::unexpected(separator.error());state->separator=*separator;
        if(auto sized=separator->set_extent({0,value.separator_height});!sized)return std::unexpected(sized.error());
        auto content=builder.zstack(*root,prefix+"/@content",{.spacing=0,.alignment=ii::gui::StackAlignment::leading,
            .minimum_height=1,.measure_hidden_children=true});
        if(!content)return std::unexpected(content.error());state->content=*content;
        if(auto clipped=root->set_clip_content(true);!clipped)return std::unexpected(clipped.error());
        if(auto painted=root->set_paint_handler([state](const ii::gui::WidgetPaintState& input)
                ->ii::core::Result<std::shared_ptr<const ii::gui::WidgetDrawing>>{
            auto geometry=state->view.geometry(input.size.width,input.size.height);
            if(!geometry)return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid AppCard paint size");
            auto drawing=std::make_shared<ii::gui::WidgetDrawing>();auto ink=linear(state->view.configuration().background);
            if(input.size.width>0&&input.size.height>0&&ink.alpha>0)
                drawing->panels.push_back({{0,0,float(input.size.width),float(input.size.height)},ink,ink,float(geometry->radius)});
            return std::shared_ptr<const ii::gui::WidgetDrawing>(std::move(drawing));
        });!painted)return std::unexpected(painted.error());
        if(auto painted=separator->set_paint_handler([state](const ii::gui::WidgetPaintState& input)
                ->ii::core::Result<std::shared_ptr<const ii::gui::WidgetDrawing>>{
            auto drawing=std::make_shared<ii::gui::WidgetDrawing>();auto ink=linear(state->view.configuration().divider_color);
            if(input.size.width>0&&input.size.height>0&&ink.alpha>0)
                drawing->panels.push_back({{0,0,float(input.size.width),float(input.size.height)},ink,ink,0});
            return std::shared_ptr<const ii::gui::WidgetDrawing>(std::move(drawing));
        });!painted)return std::unexpected(painted.error());
        if(auto configured=apply(state,value);!configured)return std::unexpected(configured.error());
        if(auto enabled=root->set_enabled(value.enabled);!enabled)return std::unexpected(enabled.error());
        return NativeAppCard(std::move(state));
    }
    [[nodiscard]] ii::gui::Widget root()const{return state_->root;}
    [[nodiscard]] ii::gui::Widget content_root()const{return state_->content;}
    [[nodiscard]] ii::core::Result<void> set_options(AppCardOptions value)const {
        if(!AppCardModel::validate(value))return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid AppCard configuration");
        try {
            if(auto current=state_->root.snapshot();!current)return std::unexpected(current.error());
            if(state_->view.configuration()==value)return {};
            if(state_->revision==(std::numeric_limits<std::uint64_t>::max)())return ii::core::fail(ii::core::ErrorCode::out_of_range,"AppCard revision exhausted");
            const auto revision=++state_->revision;auto previous=state_->view;(void)state_->view.set_configuration(value);
            if(auto changed=apply(state_,value);!changed){if(state_->revision==revision)state_->view=std::move(previous);return changed;}
            if(state_->revision!=revision)return {};
            if(auto enabled=state_->root.set_enabled(value.enabled);!enabled)return enabled;
            if(state_->revision!=revision)return {};
            return state_->root.request_repaint();
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"AppCard update callback failed",std::current_exception());}
    }
};
} // namespace ii::gui
