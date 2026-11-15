#pragma once
#include <iiGUI/AlertModel.h>
#include <iiGUI/NativeAlertButtonView.h>
#include <iiGUI/NativeImageView.h>
#include <iiGUI/NativeWidgets.h>
#include <iiGUI/Text.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <exception>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

namespace ii::gui {
struct AlertStyle final {
    LabelOptions title{.font_size=26,.selectable=false,.role=TextRole::title,.alignment=TextAlignment::center,.line_height=26,.font_weight=700};
    LabelOptions description{.font_size=13,.selectable=false,.role=TextRole::body,.alignment=TextAlignment::center,.line_height=13,.font_weight=500};
    ImageConfiguration icon;
    std::array<AlertButtonOptions,3> actions;
    friend bool operator==(const AlertStyle&,const AlertStyle&)=default;
};
struct AlertInput final {
    AlertConfiguration configuration;
    std::shared_ptr<const RasterImage> icon;
    std::string icon_name;
    std::shared_ptr<const ButtonIconSource> icon_source;
    std::int64_t icon_revision=0;
    AlertStyle style;
    friend bool operator==(const AlertInput&,const AlertInput&)=default;
};
struct AlertCallbacks final {
    std::array<std::function<void(const ButtonEvent&)>,3> methods;
    std::array<std::function<void()>,3> clicked;
    std::function<void()> opened,closed,canceled;
};
struct DefaultAlertHostPolicy final {
    using Model=AlertModel;using Input=AlertInput;using Configuration=AlertConfiguration;
    using Shape=AlertShape;using Rect=AlertRect;using Color=WidgetColor;using IconCache=ButtonIconCache;
    using Button=AlertButtonView;using ButtonConfiguration=AlertButtonOptions;
    using ButtonCallbacks=AlertButtonCallbacks;using ButtonEvent=ii::gui::ButtonEvent;using Callbacks=AlertCallbacks;
    [[nodiscard]] static std::uint64_t source_revision(const Input& input)noexcept {
        return input.configuration.show_icon&&!input.icon&&input.icon_source?input.icon_source->revision():0;
    }
    [[nodiscard]] static bool valid_input(const Input& input)noexcept {
        return input.icon_name.size()<=256&&DefaultButtonPolicy::valid_text(input.icon_name)&&input.icon_revision>=0&&
            !(input.icon&&!input.icon_name.empty())&&(!input.icon_source||!input.icon_name.empty());
    }
    [[nodiscard]] static core::Result<ImageInput> prepare_icon(const Input& input,const Model& model,IconCache& cache) {
        ImageInput result;result.image=input.icon;const auto& c=model.configuration();
        if(c.show_icon&&!result.image&&!input.icon_name.empty()){
            auto image=cache.resolve(input.icon_name,input.icon_source,c.icon_extent);
            if(!image)return std::unexpected(image.error());result.image=*image;
        }
        result.configuration=input.style.icon;result.configuration.width=result.configuration.height=c.icon_extent;
        result.configuration.allow_empty=true;return result;
    }
    [[nodiscard]] static core::Result<ButtonConfiguration> action(const Input& input,const Model& model,unsigned index) {
        auto result=input.style.actions[index];const auto item=model.action(index);const auto& c=model.configuration();
        result.text=std::string(item.text);result.enabled=item.enabled;result.tone=item.tone;
        result.primary_color=c.primary_color;
        result.height=index==2&&model.action_count()==3?c.metrics.tertiary_action_height:c.metrics.action_height;
        if(item.destructive)result.text_color=result.palette.danger;
        if(!AlertButtonModel::resolve(result))return core::fail(core::ErrorCode::invalid_argument,"Invalid alert action");
        return result;
    }
    [[nodiscard]] static LabelOptions title_options(const Input& input,const Configuration& config) {
        auto result=input.style.title;result.text=config.title;result.selectable=false;result.color=config.palette.title;return result;
    }
    [[nodiscard]] static LabelOptions description_options(const Input& input,const Model& model) {
        auto result=input.style.description;result.text=std::string(model.description());result.selectable=false;
        result.color=model.configuration().palette.description;return result;
    }
};
template<class Policy>
class BasicNativeAlert final {
    using AlertModel=typename Policy::Model;using AlertInput=typename Policy::Input;
    using AlertConfiguration=typename Policy::Configuration;using AlertShape=typename Policy::Shape;
    using AlertRect=typename Policy::Rect;using StyleColor=typename Policy::Color;
    using NativeIconCache=typename Policy::IconCache;using NativeAlertButton=typename Policy::Button;
    using AlertButtonConfiguration=typename Policy::ButtonConfiguration;
    using NativeAlertButtonCallbacks=typename Policy::ButtonCallbacks;using AlertButtonEvent=typename Policy::ButtonEvent;
    using NativeAlertCallbacks=typename Policy::Callbacks;
    struct Prepared final {
        AlertModel model;
        AlertInput input;
        ii::gui::LabelOptions title;
        ii::gui::LabelOptions description;
        ImageInput icon;
        std::array<AlertButtonConfiguration, 3> actions;
    };
    using Snapshot = std::shared_ptr<const Prepared>;
    struct State final {
        AlertInput authored;
        Snapshot prepared;
        std::shared_ptr<NativeIconCache> icons=std::make_shared<NativeIconCache>();
        ii::gui::Widget root, content, title, description;
        std::optional<NativeImage> icon;
        std::array<std::optional<NativeAlertButton>, 3> actions;
        NativeAlertCallbacks callbacks;
        std::string font_family;
        std::optional<ii::core::Error> layout_failure;
        std::optional<double> allocated_width;
        const std::thread::id owner = std::this_thread::get_id();
        std::uint64_t revision = 0, request_revision = 0, source_revision = 0, opening = 0;
        bool live = false, retired = false;
    };

    explicit BasicNativeAlert(std::shared_ptr<State> state) : state_(std::move(state)) {}

    [[nodiscard]] static ii::gui::WidgetColor host_color(StyleColor color) noexcept {
        return {color.red, color.green, color.blue, color.alpha};
    }

    [[nodiscard]] static ii::gui::PopoverOptions host_options(const AlertConfiguration& c) {
        ii::gui::PopoverOptions value;
        value.width = (std::min)(c.maximum_width, (std::max)(c.minimum_width, c.preferred_width));
        value.padding = 0;
        value.margins = c.side_padding;
        value.corner_radius = c.corner_radius;
        value.corner_limit = c.metrics.rounded_radius_fraction;
        value.surface_opacity = 1;
        value.surface_color = host_color(c.palette.frame);
        value.modal = true; value.dim = true; value.focus_on_open = true;
        value.dismiss_on_escape = c.dismiss_on_escape;
        value.dismiss_on_outside_press = c.dismiss_on_background;
        value.accessibility_name = c.accessibility_name;
        value.placement = ii::gui::PopoverPlacement::centered;
        value.minimum_width = c.minimum_width; value.maximum_width = c.maximum_width;
        value.fit_to_viewport = true; value.opaque_base = false;
        value.cylinder = c.shape == AlertShape::cylinder;
        value.backdrop_color = host_color(c.palette.backdrop);
        value.border_width = c.border_width; value.border_color = host_color(c.palette.border);
        if (c.glass_enabled) value.blur_radius = c.blur_radius;
        if (c.shadow_enabled) value.shadow = ii::gui::PopoverShadow{};
        ii::gui::PopoverTransition transition;
        transition.enter_duration = std::chrono::duration_cast<ii::runtime::Clock::duration>(
            std::chrono::duration<double, std::milli>(c.enter_duration_ms));
        transition.exit_duration = std::chrono::duration_cast<ii::runtime::Clock::duration>(
            std::chrono::duration<double, std::milli>(c.exit_duration_ms));
        transition.enter_easing = ii::animation::Easing::out_back;
        transition.exit_easing = ii::animation::Easing::in_cubic;
        transition.overshoot = c.overshoot; transition.collapsed_scale = c.initial_scale;
        transition.enabled = c.motion_enabled; value.transition = std::move(transition);
        return value;
    }

    [[nodiscard]] static ii::core::Result<Snapshot> prepare(const AlertInput& input,NativeIconCache& icons) {
        if(!AlertModel::validate(input.configuration)||!Policy::valid_input(input))
            return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid alert configuration");
        try {
            auto prepared=std::make_shared<Prepared>();prepared->input=input;
            if(!prepared->model.set_configuration(input.configuration))
                return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid alert configuration");
            prepared->title=Policy::title_options(input,prepared->model.configuration());
            prepared->description=Policy::description_options(input,prepared->model);
            auto icon=Policy::prepare_icon(input,prepared->model,icons);
            if(!icon)return std::unexpected(icon.error());prepared->icon=std::move(*icon);
            for(unsigned index=0;index<3;++index){
                auto action=Policy::action(input,prepared->model,index);
                if(!action)return std::unexpected(action.error());prepared->actions[index]=std::move(*action);
            }
            return Snapshot(std::move(prepared));
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Alert preparation failed",std::current_exception());}
    }

    [[nodiscard]] static ii::core::Result<double> measure(std::string_view text,double width,
            const ii::gui::LabelOptions& style,std::string_view family) {
        if(text.empty())return 0.;
        auto metrics=ii::gui::measure_wrapped_text_layout({.bounds={0,0,float((std::max)(1.,width)),8192},
            .text=std::string(text),.font_size=float(style.font_size),.font_weight=style.font_weight.value_or(400),
            .alignment=style.alignment,.font_family=std::string(family),.line_height=float(style.line_height)});
        if(!metrics)return std::unexpected(metrics.error());return metrics->height;
    }

    [[nodiscard]] static ii::core::Result<void> layout(const std::shared_ptr<State>& state,double width,std::uint64_t revision) {
        try {
        auto prepared=state->prepared;
        if(state->retired||!prepared||!state->icon)return {};
        const auto current=[&]{return !state->retired&&state->revision==revision&&state->prepared==prepared;};
        const auto& c=prepared->model.configuration();const auto text_width=(std::max)(0.,width-2*c.text_padding);
        auto title=measure(c.title,text_width,prepared->title,state->font_family);
        if(!title)return std::unexpected(title.error());
        auto description=measure(prepared->model.description(),text_width,prepared->description,state->font_family);
        if(!description)return std::unexpected(description.error());
        auto result=prepared->model.layout_for_width(width,*title,*description);
        if(!result)return ii::core::fail(ii::core::ErrorCode::out_of_range,"Alert content exceeds its layout limits");
        const auto rect=[](AlertRect value){return ii::gui::Rect{float(value.x),float(value.y),float(value.width),float(value.height)};};
        const auto desired=(std::min)(c.maximum_width,(std::max)(c.minimum_width,c.preferred_width));
        const auto step=[&](auto&& operation)->ii::core::Result<bool>{
            if(!current())return false;auto done=operation();if(!done)return std::unexpected(done.error());return current();
        };
        auto done=step([&]{return state->content.set_extent({desired,result->frame.height});});if(!done)return std::unexpected(done.error());if(!*done)return {};
        done=step([&]{return state->icon->root().set_frame(rect(result->icon));});if(!done)return std::unexpected(done.error());if(!*done)return {};
        done=step([&]{return state->title.set_frame(rect(result->title));});if(!done)return std::unexpected(done.error());if(!*done)return {};
        done=step([&]{return state->description.set_frame(rect(result->description));});if(!done)return std::unexpected(done.error());if(!*done)return {};
        for(unsigned index=0;index<3;++index){
            done=step([&]{return state->actions[index]->root().set_frame(rect(result->actions[index]));});
            if(!done)return std::unexpected(done.error());if(!*done)return {};
        }
        if(current()){state->allocated_width=width;state->layout_failure.reset();}return {};
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Alert layout failed",std::current_exception());}
    }

    [[nodiscard]] static ii::core::Result<bool> apply_host(const std::shared_ptr<State>& state,
                                                            Snapshot prepared, std::uint64_t revision) {
        try {
        const auto& model = prepared->model;
        const auto& c = model.configuration();
        const auto step = [&](auto&& operation) -> ii::core::Result<bool> {
            if(state->retired||!state->prepared||state->revision!=revision)return false;
            auto result=operation();
            if(!result)return std::unexpected(result.error());
            return !state->retired&&state->prepared&&state->revision==revision;
        };
        auto result = step([&] { return state->root.set_popover_options(host_options(c)); });
        if (!result || !*result) return result;
        result = step([&] { return state->title.set_label_options(prepared->title); });
        if (!result || !*result) return result;
        result = step([&] { return state->description.set_label_options(prepared->description); });
        if (!result || !*result) return result;
        result = step([&] { return state->title.set_visible(!c.title.empty()); });
        if (!result || !*result) return result;
        result = step([&] { return state->description.set_visible(!model.description().empty()); });
        if (!result || !*result) return result;
        result = step([&] { auto icon=state->icon;return icon->set_options(prepared->icon); });
        if (!result || !*result) return result;
        result = step([&] { return state->icon->root().set_visible(c.show_icon); });
        if (!result || !*result) return result;
        for (unsigned index = 0; index < 3; ++index) {
            result = step([&] { auto action=state->actions[index];return action->set_options(prepared->actions[index]); });
            if (!result || !*result) return result;
            result = step([&] { return state->actions[index]->root().set_visible(model.action(index).visible); });
            if (!result || !*result) return result;
        }
        const auto width = state->allocated_width.value_or((std::min)(c.maximum_width, (std::max)(c.minimum_width, c.preferred_width)));
        return step([&] { return layout(state, width, revision); });
        } catch (...) {
            return ii::core::fail(ii::core::ErrorCode::failed,"Alert application failed",std::current_exception());
        }
    }

public:
    [[nodiscard]] static ii::core::Result<BasicNativeAlert> create(
            ii::gui::WidgetBuilder& builder, ii::gui::Widget parent, std::string_view id,
            AlertInput input = {}, NativeAlertCallbacks callbacks = {}) {
        try {
            auto state=std::make_shared<State>();std::optional<Snapshot> prepared;std::uint64_t source_revision=0;
            for(unsigned attempt=0;attempt<4;++attempt){
                const auto before=Policy::source_revision(input);auto candidate=prepare(input,*state->icons);
                if(Policy::source_revision(input)!=before)continue;
                if(!candidate)return std::unexpected(candidate.error());
                prepared=std::move(*candidate);source_revision=before;break;
            }
            if(!prepared)return ii::core::fail(ii::core::ErrorCode::busy,"Alert icon source changed repeatedly");
            auto appearance=builder.appearance();
            if(!appearance)return std::unexpected(appearance.error());
            state->font_family=appearance->styled?appearance->font_family:std::string{};
            state->authored=std::move(input);state->source_revision=source_revision;
            state->prepared=std::move(*prepared);
            state->callbacks = std::move(callbacks);
            std::weak_ptr<State> weak = state;
            const auto event = [weak](auto member, bool opened) {
                return [weak, member, opened] {
                    if (auto state = weak.lock(); state && state->live) {
                        auto current = state->root.snapshot();
                        if (!current || current->opened != opened) return;
                        if (member != &NativeAlertCallbacks::canceled) ++state->opening;
                        auto callback = state->callbacks.*member;
                        if (callback) callback();
                    }
                };
            };
            auto root = builder.popover(parent, id, host_options(state->prepared->model.configuration()),
                {.opened = event(&NativeAlertCallbacks::opened, true), .closed = event(&NativeAlertCallbacks::closed, false),
                 .canceled = event(&NativeAlertCallbacks::canceled, false)});
            if (!root) return std::unexpected(root.error()); state->root = *root;
            const auto name = [&](std::string_view suffix) { return std::string(id) + "." + std::string(suffix); };
            auto content = builder.absolute_stack(*root, name("content"), {500, 300});
            if (!content) return std::unexpected(content.error()); state->content = *content;
            auto icon = NativeImage::create(builder, *content, name("icon"), state->prepared->icon);
            if (!icon) return std::unexpected(icon.error()); state->icon = *icon;
            auto title = builder.label(*content, name("title"), state->prepared->title);
            if (!title) return std::unexpected(title.error()); state->title = *title;
            auto description = builder.label(*content, name("description"), state->prepared->description);
            if (!description) return std::unexpected(description.error()); state->description = *description;
            for (unsigned index = 0; index < 3; ++index) {
                NativeAlertButtonCallbacks button_callbacks;
                button_callbacks.method = [weak, index](const AlertButtonEvent& event) {
                    auto state = weak.lock();
                    if (!state || !state->live || !state->prepared) return;
                    auto current = state->root.snapshot();
                    if (!current || !current->opened || !state->prepared->model.action(index).enabled) return;
                    const auto opening = state->opening;
                    auto method = state->callbacks.methods[index];
                    if (method) method(event);
                    if (!state->live || !state->prepared || state->opening != opening) return;
                    current = state->root.snapshot();
                    if (!current || !current->opened || !state->prepared->model.action(index).enabled) return;
                    auto clicked = state->callbacks.clicked[index];
                    if (clicked) clicked();
                };
                auto button = NativeAlertButton::create(builder, *content, name("action-" + std::to_string(index)),
                                                         state->prepared->actions[index], std::move(button_callbacks));
                if (!button) return std::unexpected(button.error()); state->actions[index] = *button;
            }
            if (auto applied = apply_host(state, state->prepared, state->revision); !applied) return std::unexpected(applied.error());
            if (auto installed = content->set_layout_handler([weak](ii::gui::Rect bounds) {
                if (auto state = weak.lock(); state && state->live && bounds.width > 0) {
                    auto result = layout(state, bounds.width, state->revision);
                    if (!result) state->layout_failure = result.error();
                }
            }); !installed) return std::unexpected(installed.error());
            if (auto retained = builder.retain(state); !retained) return std::unexpected(retained.error());
            if (auto published = builder.on_publish([weak] { if (auto state = weak.lock()) state->live = true; }); !published) return std::unexpected(published.error());
            if (auto retired = builder.on_retire([weak] { if (auto state = weak.lock()) {
                    state->live=false;state->retired=true;state->prepared.reset();state->icons.reset();state->authored=AlertInput{};
                    state->callbacks={};state->icon.reset();for(auto& action:state->actions)action.reset();
                }}); !retired) return std::unexpected(retired.error());
            return BasicNativeAlert(std::move(state));
        } catch (...) {
            return ii::core::fail(ii::core::ErrorCode::failed, "Alert construction failed", std::current_exception());
        }
    }

    [[nodiscard]] ii::gui::Widget root() const { return state_->root; }

    [[nodiscard]] ii::core::Result<AlertInput> options() const {
        auto state=state_;
        if(state->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"Alert requires its owner thread");
        if(auto valid=state->root.snapshot();!valid)return std::unexpected(valid.error());
        if(state->retired||!state->prepared)return ii::core::fail(ii::core::ErrorCode::closed,"Alert has retired");
        try{return state->authored;}catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Alert options failed",std::current_exception());}
    }
    [[nodiscard]] ii::core::Result<void> set_options(AlertInput input) const {
        auto state=state_;
        if(state->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"Alert requires its owner thread");
        try {
            if(auto valid=state->root.snapshot();!valid)return std::unexpected(valid.error());
            if(state->retired||!state->prepared)return ii::core::fail(ii::core::ErrorCode::closed,"Alert has retired");
            if(state->request_revision==(std::numeric_limits<std::uint64_t>::max)())return ii::core::fail(ii::core::ErrorCode::out_of_range,"Alert request revision exhausted");
            const auto request=++state->request_revision;
            if(state->authored==input&&state->source_revision==Policy::source_revision(input))return {};
            if(state->revision==(std::numeric_limits<std::uint64_t>::max)())return ii::core::fail(ii::core::ErrorCode::out_of_range,"Alert revision exhausted");
            if(!state->icons)state->icons=std::make_shared<NativeIconCache>();
            auto icons=state->icons;std::optional<Snapshot> next;std::uint64_t source_revision=0;
            for(unsigned attempt=0;attempt<4;++attempt){
                const auto before=Policy::source_revision(input);auto candidate=prepare(input,*icons);
                if(state->retired)return {};
                if(state->request_revision!=request){if(state->icons==icons)state->icons.reset();return {};}
                if(Policy::source_revision(input)!=before)continue;
                if(!candidate)return std::unexpected(candidate.error());
                next=std::move(*candidate);source_revision=before;break;
            }
            if(!next){if(state->icons==icons)state->icons.reset();return ii::core::fail(ii::core::ErrorCode::busy,"Alert icon source changed repeatedly");}
            auto old_input=state->authored;auto old=state->prepared;const auto old_source_revision=state->source_revision;
            const auto revision=++state->revision;
            state->authored=std::move(input);state->source_revision=source_revision;state->prepared=std::move(*next);
            auto applied=apply_host(state,state->prepared,revision);
            if(!applied){
                if(!state->retired&&state->revision==revision){state->authored=std::move(old_input);state->source_revision=old_source_revision;state->prepared=std::move(old);(void)apply_host(state,state->prepared,revision);}
                return std::unexpected(applied.error());
            }
            return {};
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Alert update failed",std::current_exception());}
    }
    [[nodiscard]] ii::core::Result<std::optional<ii::core::Error>> layout_failure() const {
        auto state=state_;
        if(state->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"Alert requires its owner thread");
        if(auto valid=state->root.snapshot();!valid)return std::unexpected(valid.error());
        return state->layout_failure;
    }

private:
    std::shared_ptr<State> state_;
};

using AlertView=BasicNativeAlert<DefaultAlertHostPolicy>;
} // namespace ii::gui
