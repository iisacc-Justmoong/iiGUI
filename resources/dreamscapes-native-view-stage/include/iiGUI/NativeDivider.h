#pragma once
#include <iiGUI/Divider.h>
#include <iiGUI/NativeWidgets.h>
#include <cctype>
#include <cstdint>
#include <exception>
#include <functional>
#include <limits>
#include <memory>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

namespace ii::gui {
inline DividerAxis divider_axis(DividerAxis value) noexcept { return value; }
inline DividerAxis divider_axis(std::string_view value) {
    std::string text(value);
    for(auto& c:text)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if(text=="horizontal")return DividerAxis::horizontal;
    if(text=="vertical")return DividerAxis::vertical;
    return static_cast<DividerAxis>(-1);
}
inline WidgetColor divider_color(ii::gui::WidgetColor color) noexcept {
    return {color.red,color.green,color.blue,color.alpha};
}
struct DividerGroupPort final {
    std::function<ii::core::Result<DividerConfiguration>()> options;
    std::function<ii::core::Result<bool>()> visible;
    std::function<ii::core::Result<void>(std::function<ii::core::Result<void>()>)> changed;
    std::function<ii::core::Result<ii::gui::WidgetSnapshot>()> snapshot;
    std::function<bool()> retired;
    std::function<ii::core::Result<ii::gui::Widget>()> widget;
};

class NativeDivider final {
    using Snapshot=std::shared_ptr<const Divider>;
    struct State final {
        Snapshot prepared;
        ii::gui::Widget root;
        std::function<ii::core::Result<void>()> changed;
        const std::thread::id owner=std::this_thread::get_id();
        std::uint64_t revision=0;
        bool visible=true;
    };
    std::shared_ptr<State> state_;
    explicit NativeDivider(std::shared_ptr<State> state):state_(std::move(state)){}
    [[nodiscard]] static ii::core::Result<void> check(const std::shared_ptr<State>& state){
        if(state->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"Divider requires its owner thread");
        if(auto current=state->root.snapshot();!current)return std::unexpected(current.error());return {};
    }
    [[nodiscard]] static ii::core::Result<Snapshot> prepare(const DividerConfiguration& value){
        auto next=std::make_shared<Divider>();
        if(!next->set_configuration(value))return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid Divider configuration");
        return Snapshot(std::move(next));
    }
    [[nodiscard]] static ii::core::Result<std::shared_ptr<const ii::gui::WidgetDrawing>>
    paint(const Snapshot& snapshot,const ii::gui::WidgetPaintState& state){
        const auto line=snapshot->geometry(state.size.width,state.size.height);
        if(!line)return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid Divider extent");
        auto drawing=std::make_shared<ii::gui::WidgetDrawing>();
        const auto color=snapshot->configuration().color;
        if(line->width>0&&line->height>0&&color.alpha>0){
            const auto channel=[](double c){return c<=.04045?c/12.92:std::pow((c+.055)/1.055,2.4);};
            const ii::gui::Color linear{float(channel(color.red)),float(channel(color.green)),float(channel(color.blue)),float(color.alpha)};
            drawing->panels.push_back({{float(line->x),float(line->y),float(line->width),float(line->height)},linear,linear,0});
        }
        return std::shared_ptr<const ii::gui::WidgetDrawing>(std::move(drawing));
    }
public:
    [[nodiscard]] static ii::core::Result<NativeDivider> create(ii::gui::WidgetBuilder& builder,ii::gui::Widget parent,
            std::string_view id,const DividerConfiguration& value){
        auto prepared=prepare(value);if(!prepared)return std::unexpected(prepared.error());
        auto state=std::make_shared<State>();state->prepared=std::move(*prepared);const auto size=state->prepared->extent();
        auto root=builder.absolute_stack(parent,id,{size.width,size.height});if(!root)return std::unexpected(root.error());
        state->root=*root;
        if(auto installed=root->set_paint_handler([state](const ii::gui::WidgetPaintState& paint_state){return paint(state->prepared,paint_state);});!installed)
            return std::unexpected(installed.error());
        std::weak_ptr<State> weak=state;
        if(auto retiring=builder.on_retire([weak]{if(auto value=weak.lock()){
                auto observer=std::move(value->changed);if(observer)try{(void)observer();}catch(...){}
            }});!retiring)return std::unexpected(retiring.error());
        return NativeDivider(std::move(state));
    }
    [[nodiscard]] ii::gui::Widget root()const{return state_->root;}
    [[nodiscard]] ii::core::Result<void> set_options(const DividerConfiguration& value)const{
        if(auto valid=check(state_);!valid)return valid;
        try{
            if(state_->prepared->configuration()==value)return {};
            auto prepared=prepare(value);if(!prepared)return std::unexpected(prepared.error());
            if(state_->revision==(std::numeric_limits<std::uint64_t>::max)())return ii::core::fail(ii::core::ErrorCode::out_of_range,"Divider revision exhausted");
            auto previous=state_->prepared;const auto revision=++state_->revision;state_->prepared=std::move(*prepared);
            const auto size=state_->prepared->extent();if(auto resized=state_->root.set_extent({size.width,size.height});!resized){
                if(state_->revision==revision)state_->prepared=std::move(previous);return resized;}
            if(state_->revision!=revision)return {};
            if(auto painted=state_->root.request_repaint();!painted)return painted;
            auto callback=state_->changed;if(callback)if(auto notified=callback();!notified){
                if(state_->revision==revision&&state_->revision!=(std::numeric_limits<std::uint64_t>::max)()){
                    const auto restore=++state_->revision;state_->prepared=std::move(previous);const auto original=state_->prepared->extent();
                    if(auto resized=state_->root.set_extent({original.width,original.height});resized&&state_->revision==restore){
                        (void)state_->root.request_repaint();(void)callback();}}
                return notified;
            }
            return {};
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Divider update failed",std::current_exception());}
    }
    [[nodiscard]] ii::core::Result<void> set_visible(bool value)const{
        if(auto valid=check(state_);!valid)return valid;
        try{
            if(state_->visible==value)return {};
            if(state_->revision==(std::numeric_limits<std::uint64_t>::max)())return ii::core::fail(ii::core::ErrorCode::out_of_range,"Divider revision exhausted");
            const auto previous=state_->visible;const auto revision=++state_->revision;state_->visible=value;
            if(auto result=state_->root.set_visible(value);!result){if(state_->revision==revision)state_->visible=previous;return result;}
            if(state_->revision!=revision)return {};auto callback=state_->changed;
            if(callback)if(auto notified=callback();!notified){
                if(state_->revision==revision&&state_->revision!=(std::numeric_limits<std::uint64_t>::max)()){
                    const auto restore=++state_->revision;state_->visible=previous;
                    if(auto updated=state_->root.set_visible(previous);updated&&state_->revision==restore)(void)callback();}
                return notified;
            }
            return {};
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Divider visibility update failed",std::current_exception());}
    }
    [[nodiscard]] DividerGroupPort weak_group_port()const{
        std::weak_ptr<State> weak=state_;
        return {
            .options=[weak]()->ii::core::Result<DividerConfiguration>{auto state=weak.lock();if(!state)return ii::core::fail(ii::core::ErrorCode::closed,"Divider has retired");
                if(auto valid=check(state);!valid)return std::unexpected(valid.error());return state->prepared->configuration();},
            .visible=[weak]()->ii::core::Result<bool>{auto state=weak.lock();if(!state)return ii::core::fail(ii::core::ErrorCode::closed,"Divider has retired");
                if(auto valid=check(state);!valid)return std::unexpected(valid.error());return state->visible;},
            .changed=[weak](std::function<ii::core::Result<void>()> callback)->ii::core::Result<void>{auto state=weak.lock();if(!state)return ii::core::fail(ii::core::ErrorCode::closed,"Divider has retired");
                if(auto valid=check(state);!valid)return valid;if(callback&&state->changed)return ii::core::fail(ii::core::ErrorCode::already_exists,"Divider already has a group observer");state->changed=std::move(callback);return {};},
            .snapshot=[weak]()->ii::core::Result<ii::gui::WidgetSnapshot>{auto state=weak.lock();if(!state)return ii::core::fail(ii::core::ErrorCode::closed,"Divider has retired");return state->root.snapshot();},
            .retired=[weak]{auto state=weak.lock();if(!state)return true;if(state->owner!=std::this_thread::get_id())return false;
                auto snapshot=state->root.snapshot();return !snapshot&&snapshot.error().code==ii::core::ErrorCode::closed;},
            .widget=[weak]()->ii::core::Result<ii::gui::Widget>{auto state=weak.lock();
                if(!state)return ii::core::fail(ii::core::ErrorCode::closed,"Divider has retired");
                if(auto valid=check(state);!valid)return std::unexpected(valid.error());return state->root;}
        };
    }
};
} // namespace ii::gui
