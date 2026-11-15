#pragma once
#include <iiGUI/NativeWidgets.h>
#include <iiGUI/WindowNoiseTexture.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <exception>
#include <limits>
#include <memory>
#include <string_view>
#include <thread>
#include <utility>

namespace ii::gui {
struct WindowNoiseViewOptions final {
    double width=0,height=0;
    WindowNoiseOptions texture;
    friend bool operator==(const WindowNoiseViewOptions&,const WindowNoiseViewOptions&)=default;
};
class NativeWindowNoiseTexture final {
    struct State final {
        WindowNoiseViewOptions options;
        std::shared_ptr<const RasterImage> image;
        Widget root;
        std::uint64_t revision=0;
        bool retired=false;
        const std::thread::id owner=std::this_thread::get_id();
    };
    std::shared_ptr<State> state_;
    explicit NativeWindowNoiseTexture(std::shared_ptr<State> state):state_(std::move(state)){}
    [[nodiscard]] static bool extent(const WindowNoiseViewOptions& options)noexcept{
        return std::isfinite(options.width)&&options.width>=0&&options.width<=8192&&
               std::isfinite(options.height)&&options.height>=0&&options.height<=8192;
    }
    [[nodiscard]] static core::Result<void> check(const std::shared_ptr<State>& state){
        if(state->owner!=std::this_thread::get_id())return core::fail(core::ErrorCode::wrong_executor,"Window noise requires its owner thread");
        if(state->retired)return core::fail(core::ErrorCode::closed,"Window noise retired");
        if(auto active=state->root.snapshot();!active)return std::unexpected(active.error());return {};
    }
    [[nodiscard]] static core::Result<std::shared_ptr<const WidgetDrawing>> paint(const std::shared_ptr<State>& state,const WidgetPaintState& input){
        if(state->retired||!state->image)return core::fail(core::ErrorCode::closed,"Window noise retired");
        if(!std::isfinite(input.size.width)||!std::isfinite(input.size.height)||input.size.width<0||input.size.height<0||
           input.size.width>8192||input.size.height>8192||!std::isfinite(input.backing_scale)||input.backing_scale<=0)
            return core::fail(core::ErrorCode::invalid_argument,"Invalid window-noise paint extent");
        try{
            auto drawing=std::make_shared<WidgetDrawing>();
            if(float(input.size.width)==0||float(input.size.height)==0)return std::shared_ptr<const WidgetDrawing>(std::move(drawing));
            const auto scale=state->options.texture.grain_strength>0?(std::max)(1.,input.backing_scale):1.;
            // Bound draw commands for unusually large viewports while keeping
            // the normal device-pixel grain size at ordinary window sizes.
            const auto tile=(std::max)(256./scale,(std::max)(input.size.width,input.size.height)/64.);
            const auto columns=std::size_t(std::ceil(input.size.width/tile)),rows=std::size_t(std::ceil(input.size.height/tile));
            drawing->images.reserve(columns*rows);
            for(std::size_t row=0;row<rows;++row)for(std::size_t col=0;col<columns;++col){
                const auto x=double(col)*tile,y=double(row)*tile;
                const auto width=(std::min)(tile,input.size.width-x),height=(std::min)(tile,input.size.height-y);
                drawing->images.push_back({.bounds={float(x),float(y),float(width),float(height)},.image=state->image,
                    .source={0,0,float(width/tile),float(height/tile)}});
            }
            return std::shared_ptr<const WidgetDrawing>(std::move(drawing));
        }catch(...){return core::fail(core::ErrorCode::failed,"Window-noise drawing failed",std::current_exception());}
    }
public:
    [[nodiscard]] static core::Result<NativeWindowNoiseTexture> create(WidgetBuilder& builder,Widget parent,std::string_view id,const WindowNoiseViewOptions& options){
        if(!extent(options))return core::fail(core::ErrorCode::invalid_argument,"Invalid window-noise extent");
        try{
            auto image=window_noise_texture(options.texture);if(!image)return std::unexpected(image.error());
            auto state=std::make_shared<State>();state->options=options;state->image=*image;
            auto root=builder.absolute_stack(parent,id,{options.width,options.height});if(!root)return std::unexpected(root.error());state->root=*root;
            if(auto installed=root->set_paint_handler([state](const WidgetPaintState& input){return paint(state,input);});!installed)return std::unexpected(installed.error());
            std::weak_ptr<State> weak=state;
            if(auto retiring=builder.on_retire([weak]{if(auto state=weak.lock()){state->retired=true;state->image.reset();}});!retiring)return std::unexpected(retiring.error());
            return NativeWindowNoiseTexture(std::move(state));
        }catch(...){return core::fail(core::ErrorCode::failed,"Window-noise creation failed",std::current_exception());}
    }
    [[nodiscard]] Widget root()const{return state_->root;}
    [[nodiscard]] core::Result<void> set_options(const WindowNoiseViewOptions& options)const{
        if(auto valid=check(state_);!valid)return valid;
        if(options==state_->options)return {};
        if(!extent(options))return core::fail(core::ErrorCode::invalid_argument,"Invalid window-noise extent");
        try{
            auto image=state_->image;
            if(options.texture!=state_->options.texture){auto prepared=window_noise_texture(options.texture);if(!prepared)return std::unexpected(prepared.error());image=*prepared;}
            if(state_->revision==(std::numeric_limits<std::uint64_t>::max)())return core::fail(core::ErrorCode::out_of_range,"Window-noise revision exhausted");
            auto previous=state_->options;auto previous_image=state_->image;const auto revision=++state_->revision;
            state_->options=options;state_->image=std::move(image);
            if(auto resized=state_->root.set_extent({options.width,options.height});!resized){
                if(state_->revision==revision){state_->options=std::move(previous);state_->image=std::move(previous_image);}return resized;
            }
            if(state_->revision!=revision||state_->retired)return {};
            return state_->root.request_repaint();
        }catch(...){return core::fail(core::ErrorCode::failed,"Window-noise update failed",std::current_exception());}
    }
};
} // namespace ii::gui
