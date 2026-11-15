#pragma once
#include <iiGUI/Appearance.h>
#include <iiGUI/WidgetMotion.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
namespace ii::gui {
enum class MaterialKind {surface,panel,window};
enum class MaterialDensity {glass25,dense75};
enum class MaterialBackdrop {none,content,previous_layers};
[[nodiscard]] inline bool valid_material_outline(std::string_view path)noexcept {
    if(path.empty())return true;
    if(path.size()>65536)return false;
    const auto* cursor=path.data();const auto* end=cursor+path.size();
    const auto space=[](char c){return c==' '||c=='\t'||c=='\n'||c=='\r';};
    const auto letter=[](char c){return (c>='A'&&c<='Z')||(c>='a'&&c<='z');};
    const auto arity=[](char c){switch(c){
        case 'M':case 'm':case 'L':case 'l':case 'T':case 't':return 2;
        case 'H':case 'h':case 'V':case 'v':return 1;
        case 'C':case 'c':return 6;
        case 'S':case 's':case 'Q':case 'q':return 4;
        case 'A':case 'a':return 7;
        default:return 0;
    }};
    bool seen_move=false,previous_number=false;char command=0;
    const auto separator=[&](){
        while(cursor!=end&&space(*cursor))++cursor;
        if(cursor!=end&&*cursor==','){
            if(!previous_number)return false;
            ++cursor;while(cursor!=end&&space(*cursor))++cursor;
            if(cursor==end||*cursor==','||letter(*cursor))return false;
            previous_number=false;
        }
        return true;
    };
    const auto number=[&]()->std::optional<double>{
        bool negative=false;if(cursor!=end&&(*cursor=='+'||*cursor=='-')){negative=*cursor=='-';++cursor;}
        const auto digit=[](char c){return c>='0'&&c<='9';};
        int digits=0,before_decimal=0,first_nonzero=-1,saved=0;long double significand=0;
        const auto append=[&](char c){
            if(first_nonzero<0&&c!='0')first_nonzero=digits;
            if(first_nonzero>=0&&saved<16){significand=significand*10+(c-'0');++saved;}
            ++digits;
        };
        while(cursor!=end&&digit(*cursor)){append(*cursor++);++before_decimal;}
        if(cursor!=end&&*cursor=='.'){++cursor;while(cursor!=end&&digit(*cursor))append(*cursor++);}
        if(digits==0)return std::nullopt;
        int exponent=0;
        if(cursor!=end&&(*cursor=='e'||*cursor=='E')){
            ++cursor;bool exponent_negative=false;
            if(cursor!=end&&(*cursor=='+'||*cursor=='-')){exponent_negative=*cursor=='-';++cursor;}
            if(cursor==end||!digit(*cursor))return std::nullopt;
            while(cursor!=end&&digit(*cursor)){exponent=(std::min)(100000,exponent*10+(*cursor-'0'));++cursor;}
            if(exponent_negative)exponent=-exponent;
        }
        if(first_nonzero<0)return 0.;
        const auto power=before_decimal-first_nonzero-saved+exponent;
        if(power>400)return std::nullopt;
        const auto scaled=power< -400?0.L:significand*std::pow(10.L,power);
        if(!std::isfinite(scaled)||scaled>1e9L)return std::nullopt;
        return static_cast<double>(negative?-scaled:scaled);
    };
    while(true){
        if(!separator())return false;
        if(cursor==end)return seen_move;
        if(letter(*cursor)){
            const auto next=*cursor++;
            if(next=='Z'||next=='z'){
                if(!seen_move)return false;command=0;previous_number=false;continue;
            }
            if(!arity(next)||(!seen_move&&next!='M'&&next!='m'))return false;
            command=next;previous_number=false;
        }else if(!command)return false;
        const auto count=arity(command);
        for(int i=0;i<count;++i){
            if(!separator()||cursor==end)return false;
            if((command=='A'||command=='a')&&(i==3||i==4)){
                if(*cursor!='0'&&*cursor!='1')return false;++cursor;
            }else{
                auto value=number();if(!value||((command=='A'||command=='a')&&i<2&&*value<0))return false;
            }
            previous_number=true;
        }
        if(command=='M'||command=='m'){seen_move=true;command=command=='M'?'L':'l';}
    }
}

struct MaterialConfiguration final {
    MaterialKind kind=MaterialKind::surface;std::optional<MaterialDensity> density;
    std::optional<WidgetColor> primary_color,color,border_color,backdrop_base_color;
    std::optional<double> tint_opacity,blur_radius,radius,inner_highlight_opacity,inner_highlight_offset_y,border_width,intense_opacity,faint_opacity,
        shadow_radius,shadow_offset_y,shadow_opacity,width,height;
    std::string outline_path;bool blur_enabled=true,motion_enabled=true,enabled=true;
    MaterialBackdrop backdrop=MaterialBackdrop::none;WidgetMotion motion{};
    friend bool operator==(const MaterialConfiguration&,const MaterialConfiguration&)=default;
};
struct ResolvedMaterial final {MaterialDensity density;MaterialBackdrop backdrop;WidgetColor primary,tint,border,base;
    double blur_radius,radius,inner_highlight_opacity,inner_highlight_offset_y,border_width,intense_opacity,faint_opacity,
        shadow_radius,shadow_offset_y,shadow_opacity,width,height;};
struct MaterialGradientWeights final {double primary=0,edge=0,alpha=0;};
struct DefaultMaterialModelPolicy final {
    using Configuration=MaterialConfiguration;using Resolved=ResolvedMaterial;using Kind=MaterialKind;using Density=MaterialDensity;
    using Backdrop=MaterialBackdrop;using Weights=MaterialGradientWeights;using Color=WidgetColor;
    struct Field final {double x_factor,x_offset,y_factor,y_offset,radius_x,radius_y;bool intense;};
    inline static constexpr std::array<Field,1> fields{{{.5,0,.5,0,200,200,true}}};
    inline static constexpr double first_position=.38,second_position=.72,first_opacity=.55,second_opacity=.13;
    [[nodiscard]] static Color default_tint()noexcept{return {0,0,0,1};}
    [[nodiscard]] static Color default_primary()noexcept{return {0,0,0,1};}
    [[nodiscard]] static bool valid_extra(const Configuration& value)noexcept {
        return valid_material_outline(value.outline_path)&&valid_widget_motion(value.motion);
    }
};
// The framework owns material resolution, frame transitions and radial geometry.
template<class Policy>
class BasicMaterial final {
    using MaterialConfiguration=typename Policy::Configuration;using ResolvedMaterial=typename Policy::Resolved;
    using MaterialKind=typename Policy::Kind;using MaterialDensity=typename Policy::Density;using MaterialBackdrop=typename Policy::Backdrop;
    using MaterialGradientWeights=typename Policy::Weights;using StyleColor=typename Policy::Color;
public:
    using TimePoint=std::chrono::steady_clock::time_point;
    [[nodiscard]] const MaterialConfiguration& configuration()const noexcept{return configuration_;}
    [[nodiscard]] bool animating()const noexcept{return colors_.active();}
    [[nodiscard]] static bool validate(const MaterialConfiguration& c)noexcept {
        const auto range=[](double n,double low,double high){return std::isfinite(n)&&n>=low&&n<=high;};
        const auto color=[&](const std::optional<StyleColor>& v){return !v||(range(v->red,0,1)&&range(v->green,0,1)&&range(v->blue,0,1)&&range(v->alpha,0,1));};
        const auto optional=[&](std::optional<double> v,double low,double high){return !v||range(*v,low,high);};
        return unsigned(c.kind)<=unsigned(MaterialKind::window)&&(!c.density||unsigned(*c.density)<=unsigned(MaterialDensity::dense75))&&
            unsigned(c.backdrop)<=unsigned(MaterialBackdrop::previous_layers)&&color(c.primary_color)&&color(c.color)&&
            color(c.border_color)&&color(c.backdrop_base_color)&&optional(c.tint_opacity,0,1)&&optional(c.blur_radius,0,64)&&
            optional(c.radius,0,8192)&&optional(c.inner_highlight_opacity,0,1)&&optional(c.inner_highlight_offset_y,-8192,8192)&&
            optional(c.border_width,0,8192)&&optional(c.intense_opacity,0,1)&&optional(c.faint_opacity,0,1)&&
            optional(c.shadow_radius,0,64)&&optional(c.shadow_offset_y,-8192,8192)&&optional(c.shadow_opacity,0,1)&&
            optional(c.width,0,8192)&&optional(c.height,0,8192)&&Policy::valid_extra(c);
    }
    [[nodiscard]] static ResolvedMaterial resolve(const MaterialConfiguration& c)noexcept {
        const bool window=c.kind==MaterialKind::window;
        const auto density=c.density.value_or(window?MaterialDensity::dense75:MaterialDensity::glass25);
        const bool dense=density==MaterialDensity::dense75;
        constexpr bool neutral_window_policy=[] {
            if constexpr(requires { Policy::neutral_window; }) return bool(Policy::neutral_window);
            return false;
        }();
        const bool neutral_window=window&&neutral_window_policy;
        auto tint=c.color.value_or(Policy::default_tint());
        tint.alpha*=c.tint_opacity.value_or(dense?.75:.25);
        return {density,c.backdrop,neutral_window?Policy::default_tint():c.primary_color.value_or(Policy::default_primary()),tint,
            c.border_color.value_or(StyleColor{1,1,1,dense?.12:.22}),c.backdrop_base_color.value_or(StyleColor{0,0,0,0}),
            c.blur_enabled?c.blur_radius.value_or(dense?64:16):0,c.radius.value_or(window?16:12),
            c.inner_highlight_opacity.value_or(dense?.06:.14),c.inner_highlight_offset_y.value_or(1),c.border_width.value_or(1),
            neutral_window?0:c.intense_opacity.value_or(.40),neutral_window?0:c.faint_opacity.value_or(.11),c.shadow_radius.value_or(window?48:24),
            c.shadow_offset_y.value_or(window?16:8),c.shadow_opacity.value_or(window?.30:.22),
            c.width.value_or(window?500:(c.kind==MaterialKind::panel?380:0)),
            c.height.value_or(window?272:(c.kind==MaterialKind::panel?204:0))};
    }
    [[nodiscard]] ResolvedMaterial resolved()const noexcept{return resolve(configuration_);}
    bool set_configuration(MaterialConfiguration next,TimePoint now) {
        if(!validate(next))return false;
        const auto style=resolve(next);auto colors=colors_;
        const WidgetMotionTransition<8>::Value target{style.primary.red,style.primary.green,style.primary.blue,style.primary.alpha,
            style.tint.red,style.tint.green,style.tint.blue,style.tint.alpha};
        if(!initialized_){if(!colors.reset(target))return false;}
        else if(!colors.retarget(target,widget_motion_value(next.motion),next.enabled&&next.motion_enabled,now))return false;
        configuration_=std::move(next);colors_=std::move(colors);initialized_=true;return true;
    }
    [[nodiscard]] bool advance(TimePoint now)noexcept{return colors_.advance(now);}
    void finish_animation()noexcept{colors_.settle();}
    [[nodiscard]] StyleColor displayed_primary()const noexcept{return channels(0);}
    [[nodiscard]] StyleColor displayed_tint()const noexcept{return channels(4);}

    // Analytic source-over coverage of the eight authored elliptical gradients.
    // Positions are in logical pixels so edge- and centre-relative accents follow resize.
    [[nodiscard]] double gradient_alpha(double x,double y,double width,double height)const noexcept {
        if(!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(width)||!std::isfinite(height)||width<=0||height<=0)return 0;
        const auto s=resolved();
        const auto accent_alpha=displayed_primary().alpha;
        double remaining=1;
        for(const auto& f:Policy::fields){
            const double d=std::hypot((x-(width*f.x_factor+f.x_offset))/f.radius_x,
                                      (y-(height*f.y_factor+f.y_offset))/f.radius_y);
            const auto sample=radial_sample(d);
            remaining*=1-accent_alpha*(f.intense?s.intense_opacity:s.faint_opacity)*sample.opacity;
        }
        return std::clamp(1-remaining,0.,1.);
    }
    [[nodiscard]] MaterialGradientWeights gradient_weights(double x,double y,double width,double height)const noexcept {
        if(!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(width)||!std::isfinite(height)||width<=0||height<=0)return {};
        const auto s=resolved();const auto accent_alpha=displayed_primary().alpha;MaterialGradientWeights result;
        for(const auto& f:Policy::fields){
            const double d=std::hypot((x-(width*f.x_factor+f.x_offset))/f.radius_x,
                                      (y-(height*f.y_factor+f.y_offset))/f.radius_y);
            const auto sample=radial_sample(d);const auto alpha=accent_alpha*(f.intense?s.intense_opacity:s.faint_opacity)*sample.opacity;
            const auto previous=1-alpha;
            result.primary=sample.primary*alpha+result.primary*previous;
            result.edge=sample.edge*alpha+result.edge*previous;
            result.alpha=alpha+result.alpha*previous;
        }
        return result;
    }
private:
    struct RadialSample final {double opacity=0,primary=0,edge=0;};
    [[nodiscard]] static RadialSample radial_sample(double distance)noexcept {
        if(distance<Policy::first_position){const auto t=distance/Policy::first_position;
            return {1+(Policy::first_opacity-1)*t,1-t,t};}
        if(distance<Policy::second_position){const auto t=(distance-Policy::first_position)/(Policy::second_position-Policy::first_position);
            return {Policy::first_opacity+(Policy::second_opacity-Policy::first_opacity)*t,0,1};}
        if(distance<1){const auto t=(distance-Policy::second_position)/(1-Policy::second_position);
            return {Policy::second_opacity*(1-t),0,1-t};}
        return {};
    }
    [[nodiscard]] StyleColor channels(std::size_t offset)const noexcept{
        const auto& v=colors_.value();const auto c=[&](std::size_t i){return std::clamp(v[offset+i],0.,1.);};
        return {c(0),c(1),c(2),c(3)};
    }
    MaterialConfiguration configuration_;
    WidgetMotionTransition<8> colors_;
    bool initialized_=false;
};
using MaterialModel=BasicMaterial<DefaultMaterialModelPolicy>;
} // namespace ii::gui
