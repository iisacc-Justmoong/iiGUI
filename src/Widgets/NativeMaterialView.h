#pragma once
#include <iiGUI/MaterialModel.h>
#include <iiGUI/MaterialGradient.h>
#include <iiGUI/Image.h>
#include <iiGUI/NativeWidgets.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <iomanip>
#include <locale>
#include <optional>
#include <string>
#include <sstream>
#include <string_view>
#include <utility>
#include <vector>
namespace ii::gui {
// The framework owns the material surface, cached resources and frame demand.
template<class Policy>
class BasicNativeMaterial final {
    using MaterialModel=typename Policy::Model;using MaterialConfiguration=typename Policy::Options;
    using MaterialBackdrop=typename Policy::Backdrop;using StyleColor=typename Policy::Color;
    using NativeMaterialGradient=typename Policy::Gradient;
    struct OutlineKey final {
        double width,height,scale,border_width;
        std::string path;
        friend bool operator==(const OutlineKey&,const OutlineKey&)=default;
    };
    struct OutlineImages final {
        std::shared_ptr<const ii::gui::RasterImage> mask,stroke;
    };
    struct State final {
        MaterialModel view;
        ii::gui::Widget root;
        ii::gui::WidgetFrameHandler frame;
        std::shared_ptr<NativeMaterialGradient> gradient=std::make_shared<NativeMaterialGradient>();
        std::optional<OutlineKey> outline_key;
        OutlineImages outline;
        bool has_sources=false;
    };
    std::shared_ptr<State> state_;
    explicit BasicNativeMaterial(std::shared_ptr<State> state):state_(std::move(state)){}
    [[nodiscard]] static ii::gui::Color linear(StyleColor v)noexcept {
        const auto c=[](double x){return x<=.04045?x/12.92:std::pow((x+.055)/1.055,2.4);};
        return {float(c(v.red)),float(c(v.green)),float(c(v.blue)),float(v.alpha)};
    }
    [[nodiscard]] static std::string svg_number(double value) {
        std::ostringstream output;output.imbue(std::locale::classic());output<<std::setprecision(12)<<value;return output.str();
    }
    [[nodiscard]] static ii::core::Result<OutlineImages> outline(const std::shared_ptr<State>& state,
            double width,double height,double backing_scale,double border_width) {
        const auto& path=state->view.configuration().outline_path;if(path.empty())return OutlineImages{};
        const auto scale=NativeMaterialGradient::raster_scale(width,height,backing_scale);
        const OutlineKey key{width,height,scale,border_width,path};
        if(state->outline_key&&*state->outline_key==key)return state->outline;
        const auto w=(std::max)(1u,static_cast<unsigned>(std::ceil(width*scale)));
        const auto h=(std::max)(1u,static_cast<unsigned>(std::ceil(height*scale)));
        // Path validation permits only SVG path syntax, so it cannot inject XML.
        const auto start="<svg xmlns='http://www.w3.org/2000/svg' width='"+svg_number(width)+"' height='"+svg_number(height)+
            "' viewBox='0 0 "+svg_number(width)+" "+svg_number(height)+"'><path d='"+path+"' ";
        auto mask=ii::gui::svg_image(start+"fill='white'/></svg>",w,h);if(!mask)return std::unexpected(mask.error());
        OutlineImages result{};result.mask=*mask;
        if(border_width>0){
            auto stroke=ii::gui::svg_image(start+"fill='none' stroke='white' stroke-width='"+svg_number(border_width)+"'/></svg>",w,h);
            if(!stroke)return std::unexpected(stroke.error());result.stroke=*stroke;
        }
        state->outline_key=key;state->outline=result;return result;
    }
    [[nodiscard]] static ii::core::Result<std::shared_ptr<const ii::gui::WidgetDrawing>>
    paint(const std::shared_ptr<State>& state,const ii::gui::WidgetPaintState& input) {
        const auto width=input.size.width,height=input.size.height;
        if(!std::isfinite(width)||!std::isfinite(height)||width<0||height<0||width>8192||height>8192||
           !std::isfinite(input.backing_scale)||input.backing_scale<=0)
            return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid material extent or scale");
        auto drawing=std::make_shared<ii::gui::WidgetDrawing>();
        if(width==0||height==0)return std::shared_ptr<const ii::gui::WidgetDrawing>(std::move(drawing));
        const auto s=state->view.resolved();const auto radius=(std::min)({s.radius,width/2,height/2});
        const ii::gui::Rect bounds{0,0,float(width),float(height)};
        // A completely opaque, unadorned rectangle needs no offscreen material
        // textures. Keep capture, gradients, outlines and effects on their
        // normal path, including during a partially transparent transition.
        const auto tint=state->view.displayed_tint();
        if(!state->has_sources&&s.backdrop==MaterialBackdrop::none&&s.blur_radius==0&&s.radius==0&&s.base.alpha==0&&
           s.intense_opacity==0&&s.faint_opacity==0&&s.border_width==0&&s.inner_highlight_opacity==0&&
           s.shadow_opacity==0&&state->view.configuration().outline_path.empty()&&tint.alpha==1){
            const auto color=linear(tint);drawing->panels.push_back({bounds,color,color,0});
            return std::shared_ptr<const ii::gui::WidgetDrawing>(std::move(drawing));
        }
        auto gradient=state->gradient;if(!gradient)return ii::core::fail(ii::core::ErrorCode::closed,"Material has retired");
        const auto [padding_x,padding_y]=NativeMaterialGradient::blur_padding(width,height,s.blur_radius);
        auto underlay=gradient->render_colored(state->view,width,height,input.backing_scale,padding_x,padding_y);if(!underlay)return std::unexpected(underlay.error());
        const auto border_width=(std::min)({s.border_width,width/2,height/2});
        auto shape=outline(state,width,height,input.backing_scale,border_width);if(!shape)return std::unexpected(shape.error());
        drawing->backdrop=ii::gui::BackdropMaterial{.bounds=bounds,.radius=float(radius),.blur_radius=float(s.blur_radius),
            .tint=linear(state->view.displayed_tint()),.border=linear(s.border),.border_width=float(shape->mask?0:border_width),
            .source=s.backdrop==MaterialBackdrop::previous_layers?ii::gui::BackdropSource::previous_layers:ii::gui::BackdropSource::content,
            .mask=shape->mask,.underlay=*underlay,.underlay_bounds={float(-padding_x),float(-padding_y),float(width+2*padding_x),float(height+2*padding_y)},
            .base=linear(s.base),.capture=s.backdrop!=MaterialBackdrop::none,
            .inner_highlight_opacity=float(s.inner_highlight_opacity),.inner_highlight_offset_y=float(s.inner_highlight_offset_y)};
        if(s.shadow_opacity>0)drawing->shadow=ii::gui::BoxShadow{.bounds={0,float(s.shadow_offset_y),float(width),float(height)},
            .radius=float(radius),.blur_radius=float(s.shadow_radius),.color={0,0,0,float(s.shadow_opacity)},.mask=shape->mask};
        if(shape->stroke&&s.border.alpha>0){auto ink=linear(s.border);ink.alpha=1;
            ii::gui::ImageRun stroke{};stroke.bounds=bounds;stroke.image=shape->stroke;stroke.tint=ink;stroke.opacity=float(s.border.alpha);
            drawing->images.push_back(std::move(stroke));}
        return std::shared_ptr<const ii::gui::WidgetDrawing>(std::move(drawing));
    }
    static void schedule(const std::shared_ptr<State>& state) {
        if(state->view.animating()){
            if(auto started=state->root.request_frames();!started){state->view.finish_animation();(void)state->root.set_frame_handler(state->frame,false);}
        }else (void)state->root.set_frame_handler(state->frame,false);
    }
public:
    [[nodiscard]] static ii::core::Result<BasicNativeMaterial> create(ii::gui::WidgetBuilder& builder,
            ii::gui::Widget parent,std::string_view id,const MaterialConfiguration& input) {
        auto state=std::make_shared<State>();
        if(!state->view.set_configuration(input,std::chrono::steady_clock::now()))
            return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid material configuration");
        const auto style=state->view.resolved();
        ii::gui::StackOptions stack{};stack.spacing=0;stack.padding=0;stack.alignment=ii::gui::StackAlignment::stretch;
        auto root=builder.zstack(parent,id,stack);
        if(!root)return std::unexpected(root.error());state->root=*root;
        if(auto sized=root->set_extent({style.width,style.height});!sized)return std::unexpected(sized.error());
        if(auto enabled=root->set_enabled(input.enabled);!enabled)return std::unexpected(enabled.error());
        std::weak_ptr<State> weak=state;
        state->frame=[weak](ii::runtime::Clock::time_point now)->ii::core::Result<bool>{
            auto state=weak.lock();if(!state)return false;const auto more=state->view.advance(now);
            if(auto result=state->root.request_repaint();!result)return std::unexpected(result.error());return more;
        };
        if(auto installed=root->set_frame_handler(state->frame,false);!installed)return std::unexpected(installed.error());
        if(auto installed=root->set_paint_handler([state](const ii::gui::WidgetPaintState& paint_state){return paint(state,paint_state);});!installed)
            return std::unexpected(installed.error());
        if(auto retired=builder.on_retire([weak]{if(auto state=weak.lock()){
            state->view.finish_animation();state->frame={};state->gradient.reset();state->outline_key.reset();state->outline={};
        }});!retired)return std::unexpected(retired.error());
        return BasicNativeMaterial(std::move(state));
    }
    [[nodiscard]] ii::gui::Widget root()const{return state_->root;}
    [[nodiscard]] ii::core::Result<void> set_sources(ii::gui::Widget background,ii::gui::Widget content)const {
        const auto previous=state_->has_sources;state_->has_sources=!background.is_null()||!content.is_null();
        try {
            if(auto changed=state_->root.set_backdrop_sources(background,content);!changed){state_->has_sources=previous;return changed;}
        } catch (...) {state_->has_sources=previous;throw;}
        if(previous!=state_->has_sources)return state_->root.request_repaint();
        return {};
    }
    [[nodiscard]] ii::core::Result<void> set_options(MaterialConfiguration value)const {
        if(!MaterialModel::validate(value))return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid material configuration");
        if(auto current=state_->root.snapshot();!current)return std::unexpected(current.error());
        auto next=state_->view;if(!next.set_configuration(std::move(value),std::chrono::steady_clock::now()))
            return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid material configuration");
        const auto style=next.resolved();
        if(auto resized=state_->root.set_extent({style.width,style.height});!resized)return resized;
        if(auto enabled=state_->root.set_enabled(next.configuration().enabled);!enabled)return enabled;
        state_->view=std::move(next);schedule(state_);return state_->root.request_repaint();
    }
};
} // namespace ii::gui
