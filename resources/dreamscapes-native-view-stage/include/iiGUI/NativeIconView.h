#pragma once

#include <iiGUI/NativeButtonView.h>
#include <iiGUI/NativeImageView.h>
#include <cstdint>
#include <exception>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

namespace ii::gui {
// The application supplies a replaceable image source and publishes revisions.
struct IconViewInput final {
    std::string name;
    double size=18;
    std::shared_ptr<const ButtonIconSource> source;
    std::int64_t revision=0;
    std::string accessibility_name;
    friend bool operator==(const IconViewInput&,const IconViewInput&)=default;
};

class IconView final {
    struct State final {
        const std::thread::id owner=std::this_thread::get_id();
        std::uint64_t revision=0;
        bool retired=false;
        std::optional<IconViewInput> current;
        ButtonIconCache icons;
    };
    NativeImage image_;
    std::shared_ptr<State> state_;
    IconView(NativeImage image,std::shared_ptr<State> state):image_(std::move(image)),state_(std::move(state)){}

    [[nodiscard]] static core::Result<ImageInput> prepare(const std::shared_ptr<State>& state,const IconViewInput& input){
        if(input.name.empty()||input.name.size()>256||input.name.find_first_not_of(" \t\r\n")==std::string::npos||
           !DefaultButtonPolicy::valid_text(input.name)||!DefaultButtonPolicy::valid_text(input.accessibility_name)||input.revision<0||
           !std::isfinite(input.size)||input.size<1||input.size>256||!input.source)
            return core::fail(core::ErrorCode::invalid_argument,"Invalid icon input");
        auto pixels=state->icons.resolve(input.name,input.source,input.size);
        if(!pixels)return std::unexpected(pixels.error());
        ImageConfiguration configuration;
        configuration.width=input.size;configuration.height=input.size;configuration.allow_empty=false;
        configuration.accessibility_name=input.accessibility_name.empty()?input.name:input.accessibility_name;
        return ImageInput{.image=std::move(*pixels),.configuration=std::move(configuration)};
    }
public:
    [[nodiscard]] static core::Result<IconView> create(WidgetBuilder& builder,Widget parent,std::string_view id,const IconViewInput& input){
        try {
            auto state=std::make_shared<State>();auto prepared=prepare(state,input);
            if(!prepared)return std::unexpected(prepared.error());
            auto image=NativeImage::create(builder,parent,id,*prepared);if(!image)return std::unexpected(image.error());
            state->current=input;
            std::weak_ptr<State> weak=state;
            if(auto retained=builder.retain(state);!retained)return std::unexpected(retained.error());
            if(auto retired=builder.on_retire([weak]{if(auto state=weak.lock()){
                    state->retired=true;state->current.reset();state->icons=ButtonIconCache{};
                }});!retired)return std::unexpected(retired.error());
            return IconView(std::move(*image),std::move(state));
        }catch(...){return core::fail(core::ErrorCode::failed,"Icon construction failed",std::current_exception());}
    }
    [[nodiscard]] Widget root()const{return image_.root();}
    [[nodiscard]] core::Result<IconViewInput> options()const{
        if(state_->owner!=std::this_thread::get_id())return core::fail(core::ErrorCode::wrong_executor,"Icon requires its owner thread");
        if(auto current=root().snapshot();!current)return std::unexpected(current.error());
        if(state_->retired||!state_->current)return core::fail(core::ErrorCode::closed,"Icon has retired");
        try{return *state_->current;}catch(...){return core::fail(core::ErrorCode::failed,"Icon options failed",std::current_exception());}
    }
    [[nodiscard]] core::Result<void> set_options(const IconViewInput& input)const{
        // Keep the operation alive even if a source callback destroys the public handle.
        auto state=state_;
        auto image=image_;
        try {
            if(state->owner!=std::this_thread::get_id())return core::fail(core::ErrorCode::wrong_executor,"Icon requires its owner thread");
            if(auto current=image.root().snapshot();!current)return std::unexpected(current.error());
            if(state->retired||!state->current)return core::fail(core::ErrorCode::closed,"Icon has retired");
            if(state->revision==(std::numeric_limits<std::uint64_t>::max)())return core::fail(core::ErrorCode::out_of_range,"Icon revision exhausted");
            // Even an unchanged reentrant request supersedes an in-flight preparation.
            const auto revision=++state->revision;
            if(*state->current==input)return {};
            struct RetiredCacheCleanup final {
                State& state;
                ~RetiredCacheCleanup(){if(state.retired)state.icons=ButtonIconCache{};}
            } cleanup{*state};
            auto prepared=prepare(state,input);
            if(state->revision!=revision||state->retired)return {};
            if(!prepared)return std::unexpected(prepared.error());
            auto previous=state->current;state->current=input;
            if(auto applied=image.set_options(*prepared);!applied){
                if(state->revision==revision&&!state->retired)state->current=std::move(previous);
                return applied;
            }
            return {};
        }catch(...){return core::fail(core::ErrorCode::failed,"Icon update failed",std::current_exception());}
    }
};
} // namespace ii::gui
