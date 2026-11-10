#pragma once
#include <iiGUI/NativeWidgets.h>
#include <iiGUI/MobileTabPainter.h>
#include <iiGUI/Motion.h>
#include <iiGUI/Text.h>
#include <algorithm>
#include <array>
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
template<class Policy>class BasicNativeMobileNavigation final {
    using MobileNavigationModel=typename Policy::Model;using MobileNavigationMotion=typename Policy::Motion;
    using MobileNavigationConfiguration=typename Policy::Configuration;using MobileNavigationItem=typename Policy::Item;
    using MobileNavigationPlatform=typename Policy::Platform;using StyleColor=typename Policy::Color;
    using NativeMaterial=typename Policy::Material;using MaterialConfiguration=typename Policy::MaterialConfiguration;
    using MaterialDensity=typename Policy::MaterialDensity;using MaterialBackdrop=typename Policy::MaterialBackdrop;
    using NativeMobileNavigationCallbacks=typename Policy::Callbacks;using Tokens=typename Policy::Tokens;using Colors=typename Policy::Colors;
    struct State final {
        MobileNavigationConfiguration options;
        NativeMobileNavigationCallbacks callbacks;
        ii::gui::Widget root,viewport,body,search;
        std::array<ii::gui::Widget,5> tabs;
        std::array<ii::gui::Widget,5> badges;
        std::array<double,5> badge_widths{};
        std::optional<NativeMaterial> material,search_material;
        MobileNavigationMotion indicator;
        std::shared_ptr<ii::gui::MotionPreferences> motion;
        ii::gui::WidgetFrameHandler frame;
        bool indicator_visible=false;
        std::array<std::shared_ptr<typename Policy::IconCache>,6> caches;
        std::array<std::shared_ptr<const ii::gui::RasterImage>,6> icons;
        double width=0;
        std::uint64_t request_revision=0;unsigned updates=0;bool retired=false;
        bool initialized=false;
        bool applying_automatic=false;
        int authored_index=-1;
        const std::thread::id owner=std::this_thread::get_id();
    };
    std::shared_ptr<State> state_;
    static void release(const std::shared_ptr<State>& state)noexcept{state->motion.reset();state->material.reset();state->search_material.reset();state->frame={};}
    struct UpdateGuard final {std::shared_ptr<State> state;explicit UpdateGuard(std::shared_ptr<State> value):state(std::move(value)){++state->updates;}
        ~UpdateGuard(){if(--state->updates==0&&state->retired)release(state);}};
    explicit BasicNativeMobileNavigation(std::shared_ptr<State> state):state_(std::move(state)){}
    [[nodiscard]] static ii::gui::Color color(StyleColor c)noexcept{
        const auto linear=[](double v){return v<=.04045?v/12.92:std::pow((v+.055)/1.055,2.4);};
        return {float(linear(c.red)),float(linear(c.green)),float(linear(c.blue)),float(c.alpha)};
    }
    [[nodiscard]] static bool android(const MobileNavigationConfiguration& c)noexcept{
        if(c.platform!=MobileNavigationPlatform::automatic)return c.platform==MobileNavigationPlatform::android;
#if defined(__ANDROID__)
        return true;
#else
        return false;
#endif
    }
    [[nodiscard]] static ii::core::Result<void> check(const std::shared_ptr<State>& state){
        if(state->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"Mobile navigation requires its owner thread");
        if(state->retired)return ii::core::fail(ii::core::ErrorCode::closed,"Mobile navigation retired");
        if(auto active=state->root.snapshot();!active)return std::unexpected(active.error());return {};
    }
    [[nodiscard]] static ii::gui::ActionOptions action(const MobileNavigationItem& item,bool selected,bool enabled,double width){
        [[maybe_unused]] const auto outer=Tokens::outer,gap=Tokens::gap,inset=Tokens::inset,group_height=Tokens::group_height,tab_width=Tokens::tab_width,touch_size=Tokens::touch_size,ios_radius=Tokens::ios_radius,ios_selected_radius=Tokens::ios_selected_radius,android_radius=Tokens::android_radius,android_selected_radius=Tokens::android_selected_radius,icon_size=Tokens::icon_size,label_size=Tokens::label_size,label_line_height=Tokens::label_line_height,icon_label_gap=Tokens::icon_label_gap,border_width=Tokens::border_width,badge_dot=Tokens::badge_dot,badge_height=Tokens::badge_height,badge_padding=Tokens::badge_padding,badge_offset=Tokens::badge_offset;
        return {.accessibility_name=item.accessibility_name.empty()?item.text:item.accessibility_name,
            .enabled=enabled&&item.enabled,.focus_ring=false,.role=ii::gui::ActionRole::tab,
            .width=width,.height=touch_size,.checked=selected,
            .accessibility_description=item.badge_accessibility_name.empty()?item.badge:item.badge_accessibility_name};
    }
    [[nodiscard]] static ii::core::Result<std::shared_ptr<const ii::gui::WidgetDrawing>> paint_root(
            const std::shared_ptr<State>& state,const ii::gui::WidgetPaintState& input){
        [[maybe_unused]] const auto outer=Tokens::outer,gap=Tokens::gap,inset=Tokens::inset,group_height=Tokens::group_height,tab_width=Tokens::tab_width,touch_size=Tokens::touch_size,ios_radius=Tokens::ios_radius,ios_selected_radius=Tokens::ios_selected_radius,android_radius=Tokens::android_radius,android_selected_radius=Tokens::android_selected_radius,icon_size=Tokens::icon_size,label_size=Tokens::label_size,label_line_height=Tokens::label_line_height,icon_label_gap=Tokens::icon_label_gap,border_width=Tokens::border_width,badge_dot=Tokens::badge_dot,badge_height=Tokens::badge_height,badge_padding=Tokens::badge_padding,badge_offset=Tokens::badge_offset;
        const auto layout=MobileNavigationModel::layout(state->options,input.size.width);
        const auto radius=android(state->options)?android_radius:ios_radius;
        auto result=std::make_shared<ii::gui::WidgetDrawing>();
        const auto fill=color(Colors::panel_window_alt),stroke=color(Colors::panel_surface_panel);
        const auto panel=[&](double x,double w){if(w<=0)return;const ii::gui::Rect r{float(x),float(layout.group_y),float(w),float(group_height)};
            const auto fitted_radius=float((std::min)({radius,w/2,group_height/2}));
            result->panels.push_back({r,fill,fill,fitted_radius});result->panels.push_back({r,stroke,stroke,fitted_radius,float((std::min)(border_width,w/2))});};
        if(!state->options.material_enabled||android(state->options)){
            panel(layout.group_x,layout.group_width);
            if(state->options.search)panel(layout.search_x,(std::min)(group_height,(std::max)(0.,input.size.width-layout.search_x)));
        }
        return std::shared_ptr<const ii::gui::WidgetDrawing>(std::move(result));
    }
    [[nodiscard]] static ii::core::Result<std::shared_ptr<const ii::gui::WidgetDrawing>> paint_item(
            const std::shared_ptr<State>& state,std::size_t index,const ii::gui::WidgetPaintState& input){
        const bool search=index==5;
        if((search&&!state->options.search)||(!search&&index>=state->options.items.size()))
            return std::shared_ptr<const ii::gui::WidgetDrawing>(std::make_shared<ii::gui::WidgetDrawing>());
        const auto& item=search?*state->options.search:state->options.items[index];
        return BasicMobileTabPainter<Policy>::item(item,!search&&state->options.current_index==int(index),
            state->options.enabled,android(state->options),!search,state->options.focus_color,state->icons[index],input);
    }
    [[nodiscard]] static ii::core::Result<std::shared_ptr<const ii::gui::WidgetDrawing>> paint_indicator(
            const std::shared_ptr<State>& state,const ii::gui::WidgetPaintState& input){
        [[maybe_unused]] const auto outer=Tokens::outer,gap=Tokens::gap,inset=Tokens::inset,group_height=Tokens::group_height,tab_width=Tokens::tab_width,touch_size=Tokens::touch_size,ios_radius=Tokens::ios_radius,ios_selected_radius=Tokens::ios_selected_radius,android_radius=Tokens::android_radius,android_selected_radius=Tokens::android_selected_radius,icon_size=Tokens::icon_size,label_size=Tokens::label_size,label_line_height=Tokens::label_line_height,icon_label_gap=Tokens::icon_label_gap,border_width=Tokens::border_width,badge_dot=Tokens::badge_dot,badge_height=Tokens::badge_height,badge_padding=Tokens::badge_padding,badge_offset=Tokens::badge_offset;auto out=std::make_shared<ii::gui::WidgetDrawing>();
        if(state->indicator_visible){auto g=state->indicator.geometry();
            const auto left=std::clamp((std::min)(g.left,g.right),0.,input.size.width);
            const auto right=std::clamp((std::max)(g.left,g.right),0.,input.size.width);
            const auto height=std::clamp(g.height,0.,input.size.height);
            if(right>left&&height>0){const auto ink=color(Colors::panel_surface_input);
                const auto radius=(std::min)({android(state->options)?android_selected_radius:ios_selected_radius,(right-left)/2,height/2});
                out->panels.push_back({{float(left),float((input.size.height-height)/2),float(right-left),float(height)},ink,ink,float(radius)});}}
        return std::shared_ptr<const ii::gui::WidgetDrawing>(std::move(out));
    }
    [[nodiscard]] static ii::core::Result<std::shared_ptr<const ii::gui::WidgetDrawing>> paint_badge(
            const std::shared_ptr<State>& state,std::size_t index,const ii::gui::WidgetPaintState& input){
        if(index>=state->options.items.size())return std::shared_ptr<const ii::gui::WidgetDrawing>(std::make_shared<ii::gui::WidgetDrawing>());
        return BasicMobileTabPainter<Policy>::badge(state->options.items[index],input);
    }
    [[nodiscard]] static MaterialConfiguration material_options(double width,double height,double radius){
        MaterialConfiguration value;value.density=MaterialDensity::glass25;value.color=Colors::material_glass_fill;
        value.primary_color=StyleColor{0,0,0,0};value.border_color=Colors::panel_surface_panel;
        value.tint_opacity=1;value.radius=radius;value.border_width=Tokens::border_width;
        value.intense_opacity=0;value.faint_opacity=0;value.width=width;value.height=height;
        value.backdrop=MaterialBackdrop::previous_layers;value.motion_enabled=false;return value;
    }
    [[nodiscard]] static ii::core::Result<void> layout(const std::shared_ptr<State>& s,double width){
        [[maybe_unused]] const auto outer=Tokens::outer,gap=Tokens::gap,inset=Tokens::inset,group_height=Tokens::group_height,tab_width=Tokens::tab_width,touch_size=Tokens::touch_size,ios_radius=Tokens::ios_radius,ios_selected_radius=Tokens::ios_selected_radius,android_radius=Tokens::android_radius,android_selected_radius=Tokens::android_selected_radius,icon_size=Tokens::icon_size,label_size=Tokens::label_size,label_line_height=Tokens::label_line_height,icon_label_gap=Tokens::icon_label_gap,border_width=Tokens::border_width,badge_dot=Tokens::badge_dot,badge_height=Tokens::badge_height,badge_padding=Tokens::badge_padding,badge_offset=Tokens::badge_offset;
        const bool resized=s->width!=width;const auto g=MobileNavigationModel::layout(s->options,width);s->width=width;
        if(resized&&s->options.current_index>=0){(void)s->indicator.reset(s->options.current_index,g.tab_width);
            if(auto r=s->body.set_frame_handler(s->frame,false);!r)return r;}
        const bool glass=s->options.material_enabled&&!android(s->options);
        const auto update_material=[&](std::optional<NativeMaterial>& m,double x,double w,bool visible)->ii::core::Result<void>{
            if(!m)return {};if(auto r=m->root().set_visible(visible&&w>0);!r)return r;
            if(visible&&w>0){if(auto r=m->set_options(material_options(w,group_height,(std::min)(ios_radius,w/2)));!r)return r;
                if(auto r=m->root().set_frame({float(x),float(g.group_y),float(w),float(group_height)});!r)return r;}return {};
        };
        if(auto r=update_material(s->material,g.group_x,g.group_width,glass);!r)return r;
        if(auto r=update_material(s->search_material,g.search_x,(std::min)(group_height,(std::max)(0.,width-g.search_x)),glass&&s->options.search.has_value());!r)return r;
        if(auto r=s->viewport.set_frame({float(g.group_x+inset),float(g.group_y+inset),float(g.viewport_width),float(touch_size)});!r)return r;
        if(auto r=s->viewport.set_scroll_options({.width=g.viewport_width,.height=touch_size,.show_indicator=false,.horizontal_enabled=true});!r)return r;
        if(auto r=s->body.set_extent({g.content_width,touch_size});!r)return r;
        for(std::size_t i=0;i<s->tabs.size();++i){
            if(auto r=s->tabs[i].set_frame({float(double(i)*g.tab_width),0,float(g.tab_width),float(touch_size)});!r)return r;
            const bool dot=i<s->options.items.size()&&s->options.items[i].badge_dot;
            const auto h=dot?badge_dot:badge_height;const auto w=(std::min)(g.tab_width,s->badge_widths[i]);
            const auto x=double(i)*g.tab_width+(std::max)(0.,(g.tab_width+icon_size)/2-badge_offset);
            if(auto r=s->badges[i].set_frame({float((std::min)(x,double(i+1)*g.tab_width-w)),0,float(w),float(h)});!r)return r;
        }
        const auto search_width=(std::min)(touch_size,(std::max)(0.,width-g.search_x-2*inset));
        if(auto r=s->search.set_frame({float(g.search_x+inset),float(g.group_y+inset),float(search_width),float(touch_size)});!r)return r;
        return s->root.request_repaint();
    }
    [[nodiscard]] static ii::core::Result<void> activate(const std::shared_ptr<State>& s,std::size_t index){
        if(auto r=check(s);!r)return r;
        if(!s->options.enabled||index>=s->options.items.size()||!s->options.items[index].enabled)return {};
        if(s->options.auto_select&&s->options.current_index!=int(index)){
            auto next=s->options;next.current_index=int(index);
            s->applying_automatic=true;auto result=BasicNativeMobileNavigation(s).set_options(std::move(next));s->applying_automatic=false;
            if(!result)return result;
        }
        auto callback=s->callbacks.activated;if(callback)callback(int(index));return {};
    }
    [[nodiscard]] static ii::core::Result<bool> keyboard(const std::shared_ptr<State>& s,std::size_t index,const ii::gui::WidgetInput& e){
        using ii::gui::WidgetInputKind;using ii::gui::WidgetKey;
        if(e.kind!=WidgetInputKind::key_down||s->options.items.empty())return false;
        int direction=0;std::size_t target=index;
        if(e.key==WidgetKey::right)direction=1;else if(e.key==WidgetKey::left)direction=-1;
        else if(e.key==WidgetKey::home){target=0;direction=1;}
        else if(e.key==WidgetKey::end){target=s->options.items.size()-1;direction=-1;}
        else return false;
        for(std::size_t step=0;step<s->options.items.size();++step){
            if(e.key==WidgetKey::left||e.key==WidgetKey::right)target=std::size_t((int(target)+direction+int(s->options.items.size()))%int(s->options.items.size()));
            else if(step)target=std::size_t((int(target)+direction+int(s->options.items.size()))%int(s->options.items.size()));
            if(s->options.items[target].enabled){if(auto focused=s->tabs[target].focus();!focused)return std::unexpected(focused.error());return true;}
        }
        return true;
    }
public:
    [[nodiscard]] static ii::core::Result<BasicNativeMobileNavigation> create(ii::gui::WidgetBuilder& builder,ii::gui::Widget parent,
            std::string_view id,MobileNavigationConfiguration options={},NativeMobileNavigationCallbacks callbacks={}){
        [[maybe_unused]] const auto outer=Tokens::outer,gap=Tokens::gap,inset=Tokens::inset,group_height=Tokens::group_height,tab_width=Tokens::tab_width,touch_size=Tokens::touch_size,ios_radius=Tokens::ios_radius,ios_selected_radius=Tokens::ios_selected_radius,android_radius=Tokens::android_radius,android_selected_radius=Tokens::android_selected_radius,icon_size=Tokens::icon_size,label_size=Tokens::label_size,label_line_height=Tokens::label_line_height,icon_label_gap=Tokens::icon_label_gap,border_width=Tokens::border_width,badge_dot=Tokens::badge_dot,badge_height=Tokens::badge_height,badge_padding=Tokens::badge_padding,badge_offset=Tokens::badge_offset;
        if(!MobileNavigationModel::validate(options))return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid mobile navigation configuration");
        try{
            auto s=std::make_shared<State>();for(auto& cache:s->caches)cache=std::make_shared<typename Policy::IconCache>();s->options=options;s->authored_index=options.current_index;s->callbacks=std::move(callbacks);s->width=options.width;
            auto preferences=builder.motion_preferences();if(!preferences)return std::unexpected(preferences.error());s->motion=*preferences;
            const auto g=MobileNavigationModel::layout(options,options.width);
            auto root=builder.absolute_stack(parent,id,{options.width,g.height});if(!root)return std::unexpected(root.error());s->root=*root;
            if(auto r=root->set_accessibility_semantics(ii::gui::AccessibilitySemantics{.role=ii::gui::AccessibilityRole::tab_group,.name=options.accessibility_name});!r)return std::unexpected(r.error());
            auto material=NativeMaterial::create(builder,*root,std::string(id)+".material",material_options(group_height,group_height,ios_radius));
            if(!material)return std::unexpected(material.error());s->material=*material;
            auto search_material=NativeMaterial::create(builder,*root,std::string(id)+".search-material",material_options(group_height,group_height,ios_radius));
            if(!search_material)return std::unexpected(search_material.error());s->search_material=*search_material;
            auto viewport=builder.scroll_view(*root,std::string(id)+".viewport",{.width=g.viewport_width,.height=touch_size,.show_indicator=false,.horizontal_enabled=true});
            if(!viewport)return std::unexpected(viewport.error());s->viewport=*viewport;
            auto body=builder.absolute_stack(*viewport,std::string(id)+".body",{g.content_width,touch_size});if(!body)return std::unexpected(body.error());s->body=*body;
            std::weak_ptr<State> weak=s;
            s->frame=[weak](ii::runtime::Clock::time_point now)->ii::core::Result<bool>{auto state=weak.lock();if(!state||state->retired||!state->root.snapshot())return false;
                auto visibility=state->root.interaction();if(!visibility)return std::unexpected(visibility.error());
                if(!visibility->visible||!visibility->enabled){const auto g=MobileNavigationModel::layout(state->options,state->width);
                    if(state->options.current_index>=0)(void)state->indicator.reset(state->options.current_index,g.tab_width);return false;}
                auto preferences=state->motion->state();if(!preferences)return std::unexpected(preferences.error());
                if(!preferences->animated()||!state->options.motion_enabled||!state->options.enabled){const auto g=MobileNavigationModel::layout(state->options,state->width);
                    if(state->options.current_index>=0)(void)state->indicator.reset(state->options.current_index,g.tab_width);}
                const bool more=state->indicator.advance(now);if(auto r=state->body.request_repaint();!r)return std::unexpected(r.error());return more;};
            if(auto r=body->set_frame_handler(s->frame,false);!r)return std::unexpected(r.error());
            if(auto r=body->set_paint_handler([weak](const auto& p)->ii::core::Result<std::shared_ptr<const ii::gui::WidgetDrawing>>{auto s=weak.lock();
                if(!s)return ii::core::fail(ii::core::ErrorCode::closed,"Mobile navigation retired");return paint_indicator(s,p);});!r)return std::unexpected(r.error());
            for(std::size_t i=0;i<s->tabs.size();++i){
                auto tab=builder.action_surface(*body,std::string(id)+".tab"+std::to_string(i),{},[weak,i]{if(auto s=weak.lock()){
                    auto result=activate(s,i);if(!result)throw std::runtime_error(result.error().message);}});
                if(!tab)return std::unexpected(tab.error());s->tabs[i]=*tab;
                if(auto r=tab->set_paint_handler([weak,i](const auto& p){auto s=weak.lock();if(!s)return ii::core::Result<std::shared_ptr<const ii::gui::WidgetDrawing>>(ii::core::fail(ii::core::ErrorCode::closed,"Mobile navigation retired"));return paint_item(s,i,p);});!r)return std::unexpected(r.error());
                if(auto r=tab->set_input_handler([weak,i](const auto& e)->ii::core::Result<bool>{auto s=weak.lock();return s?keyboard(s,i,e):ii::core::Result<bool>(false);});!r)return std::unexpected(r.error());
            }
            for(std::size_t i=0;i<s->badges.size();++i){auto badge=builder.absolute_stack(*body,std::string(id)+".badge"+std::to_string(i),{0,0});
                if(!badge)return std::unexpected(badge.error());s->badges[i]=*badge;
                if(auto r=badge->set_paint_handler([weak,i](const auto& p)->ii::core::Result<std::shared_ptr<const ii::gui::WidgetDrawing>>{auto s=weak.lock();
                    if(!s)return ii::core::fail(ii::core::ErrorCode::closed,"Mobile navigation retired");return paint_badge(s,i,p);});!r)return std::unexpected(r.error());}
            auto search=builder.action_surface(*root,std::string(id)+".search",{},[weak]{if(auto s=weak.lock();s&&s->options.enabled&&s->options.search&&s->options.search->enabled){auto callback=s->callbacks.search_requested;if(callback)callback();}});
            if(!search)return std::unexpected(search.error());s->search=*search;
            if(auto r=search->set_paint_handler([weak](const auto& p){auto s=weak.lock();if(!s)return ii::core::Result<std::shared_ptr<const ii::gui::WidgetDrawing>>(ii::core::fail(ii::core::ErrorCode::closed,"Mobile navigation retired"));return paint_item(s,5,p);});!r)return std::unexpected(r.error());
            if(auto r=root->set_paint_handler([s](const auto& p){return paint_root(s,p);});!r)return std::unexpected(r.error());
            if(auto r=root->set_interaction_handler([weak](const ii::gui::WidgetInteraction& interaction){if(auto state=weak.lock();state&&state->indicator.active()&&(!interaction.visible||!interaction.enabled)){
                const auto g=MobileNavigationModel::layout(state->options,state->width);if(state->options.current_index>=0)(void)state->indicator.reset(state->options.current_index,g.tab_width);
                (void)state->body.set_frame_handler(state->frame,false);(void)state->body.request_repaint();}});!r)return std::unexpected(r.error());
            if(auto r=root->set_layout_handler([weak](ii::gui::Rect r){if(auto s=weak.lock();s&&s->width!=r.width)(void)layout(s,r.width);});!r)return std::unexpected(r.error());
            if(auto retired=builder.on_retire([weak]{if(auto state=weak.lock()){state->retired=true;
                if(state->request_revision!=(std::numeric_limits<std::uint64_t>::max)())++state->request_revision;
                state->callbacks={};state->icons={};for(auto& cache:state->caches)cache.reset();if(state->updates==0)release(state);}});!retired)return std::unexpected(retired.error());
            auto instance=BasicNativeMobileNavigation(s);if(auto r=instance.set_options(std::move(options));!r)return std::unexpected(r.error());
            return instance;
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Mobile navigation construction failed",std::current_exception());}
    }
    [[nodiscard]] ii::gui::Widget root()const{return state_->root;}
    [[nodiscard]] ii::core::Result<void> set_options(MobileNavigationConfiguration options)const{
        if(auto r=check(state_);!r)return r;
        if(!MobileNavigationModel::validate(options))return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid mobile navigation configuration");
        try{
            if(state_->request_revision==(std::numeric_limits<std::uint64_t>::max)())return ii::core::fail(ii::core::ErrorCode::out_of_range,"Mobile navigation request revision exhausted");
            const auto request=++state_->request_revision;UpdateGuard guard(state_);auto caches=state_->caches;
            const auto authored_index=options.current_index;
            if(!state_->applying_automatic&&state_->initialized&&options.auto_select&&state_->options.auto_select&&
               authored_index==state_->authored_index&&state_->options.current_index>=-1&&
               (state_->options.current_index<0||std::size_t(state_->options.current_index)<options.items.size()))
                options.current_index=state_->options.current_index;
            if(options.auto_select&&(options.current_index<0||!options.items[std::size_t(options.current_index)].enabled)){
                auto first=std::ranges::find_if(options.items,[](const auto& item){return item.enabled;});
                options.current_index=first==options.items.end()?-1:int(first-options.items.begin());
            }
            if(state_->initialized&&state_->options==options){if(!state_->applying_automatic)state_->authored_index=authored_index;return {};}
            const auto previous=state_->options;const auto initialized=state_->initialized;
            std::optional<std::string> focused_key;
            if(initialized)for(std::size_t i=0;i<previous.items.size();++i){
                auto snapshot=state_->tabs[i].snapshot();if(!snapshot)return std::unexpected(snapshot.error());
                if(snapshot->focused){focused_key=previous.items[i].key;break;}
            }
            std::array<std::shared_ptr<const ii::gui::RasterImage>,6> icons;
            std::array<double,5> badge_widths{};
            for(std::size_t i=0;i<options.items.size();++i){
                if(initialized&&i<previous.items.size()&&previous.items[i].icon_name==options.items[i].icon_name&&state_->icons[i])icons[i]=state_->icons[i];
                else{auto image=caches[i]->resolve(options.items[i].icon_name,{},Tokens::icon_size);if(!image)return std::unexpected(image.error());icons[i]=*image;}
                const auto& item=options.items[i];
                if(item.badge_dot)badge_widths[i]=Tokens::badge_dot;
                else if(!item.badge.empty()){
                    if(initialized&&i<previous.items.size()&&previous.items[i].badge==item.badge&&!previous.items[i].badge_dot)badge_widths[i]=state_->badge_widths[i];
                    else{auto measured=ii::gui::measure_text_layout({.text=item.badge,.font_size=float(Tokens::label_size),
                        .font_weight=600,.line_height=float(Tokens::label_line_height),.elide_right=true});
                        if(!measured)return std::unexpected(measured.error());badge_widths[i]=(std::max)(Tokens::badge_height,
                            measured->width+2*Tokens::badge_padding);}}
            }
            if(options.search){
                if(initialized&&previous.search&&previous.search->icon_name==options.search->icon_name&&state_->icons[5])icons[5]=state_->icons[5];
                else{auto image=caches[5]->resolve(options.search->icon_name,{},Tokens::icon_size);if(!image)return std::unexpected(image.error());icons[5]=*image;}
            }
            if(state_->retired)return ii::core::fail(ii::core::ErrorCode::closed,"Mobile navigation retired");
            if(state_->request_revision!=request)return {};
            const bool badge_geometry=state_->badge_widths!=badge_widths;
            state_->options=std::move(options);state_->icons=std::move(icons);state_->badge_widths=badge_widths;
            const auto& c=state_->options;const auto g=MobileNavigationModel::layout(c,state_->width);
            const bool geometry=!initialized||previous.width!=c.width||previous.bottom_safe_inset!=c.bottom_safe_inset||
                previous.items.size()!=c.items.size()||previous.search.has_value()!=c.search.has_value()||
                previous.platform!=c.platform||previous.material_enabled!=c.material_enabled||badge_geometry;
            if(!initialized||previous.width!=c.width||previous.bottom_safe_inset!=c.bottom_safe_inset)
                if(auto r=state_->root.set_extent({c.width,g.height});!r)return r;
            if(!initialized||previous.accessibility_name!=c.accessibility_name)
                if(auto r=state_->root.set_accessibility_semantics(ii::gui::AccessibilitySemantics{.role=ii::gui::AccessibilityRole::tab_group,.name=c.accessibility_name});!r)return r;
            for(std::size_t i=0;i<state_->tabs.size();++i){
                const bool visible=i<c.items.size(),was_visible=initialized&&i<previous.items.size();
                if(!initialized||visible!=was_visible)if(auto r=state_->tabs[i].set_visible(visible);!r)return r;
                const bool badge_visible=visible&&(c.items[i].badge_dot||!c.items[i].badge.empty());
                const bool was_badge_visible=was_visible&&(previous.items[i].badge_dot||!previous.items[i].badge.empty());
                if(!initialized||badge_visible!=was_badge_visible)if(auto r=state_->badges[i].set_visible(badge_visible);!r)return r;
                if(badge_visible&&(!was_badge_visible||previous.items[i]!=c.items[i]))if(auto r=state_->badges[i].request_repaint();!r)return r;
                if(visible&&(!was_visible||previous.items[i]!=c.items[i]||
                    (previous.current_index==int(i))!=(c.current_index==int(i))||previous.enabled!=c.enabled||geometry||previous.platform!=c.platform||previous.focus_color!=c.focus_color)){
                    if(auto r=state_->tabs[i].set_action_options(action(c.items[i],c.current_index==int(i),c.enabled,g.tab_width));!r)return r;
                    if(auto r=state_->tabs[i].request_repaint();!r)return r;
                }
            }
            if(!initialized||previous.search.has_value()!=c.search.has_value())if(auto r=state_->search.set_visible(c.search.has_value());!r)return r;
            if(c.search&&(!initialized||previous.search!=c.search||previous.enabled!=c.enabled||previous.platform!=c.platform||previous.focus_color!=c.focus_color)){
                auto a=action(*c.search,false,c.enabled,Tokens::touch_size);a.role=ii::gui::ActionRole::button;
                if(auto r=state_->search.set_action_options(a);!r)return r;if(auto r=state_->search.request_repaint();!r)return r;}
            if(geometry)if(auto r=layout(state_,state_->width);!r)return r;
            if(!initialized||previous.platform!=c.platform||previous.material_enabled!=c.material_enabled)if(auto r=state_->root.request_repaint();!r)return r;
            if(!initialized||geometry||previous.current_index!=c.current_index||previous.motion_enabled!=c.motion_enabled||previous.enabled!=c.enabled){
                state_->indicator_visible=c.current_index>=0;
                if(c.current_index>=0){auto preferences=state_->motion->state();if(!preferences)return std::unexpected(preferences.error());
                    auto duration=state_->motion->duration(std::chrono::milliseconds(360));if(!duration)return std::unexpected(duration.error());
                    const bool animate=initialized&&!geometry&&previous.current_index>=0&&c.enabled&&c.motion_enabled&&preferences->animated();
                    if(!state_->indicator.retarget(c.current_index,g.tab_width,animate,std::chrono::duration_cast<std::chrono::milliseconds>(*duration),ii::runtime::Clock::now()))
                        return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid mobile navigation motion");
                }
                if(state_->indicator_visible&&state_->indicator.active()){
                    if(auto started=state_->body.request_frames();!started){(void)state_->indicator.reset(c.current_index,g.tab_width);(void)state_->body.set_frame_handler(state_->frame,false);}
                }else if(auto stopped=state_->body.set_frame_handler(state_->frame,false);!stopped)return stopped;
                if(auto r=state_->body.request_repaint();!r)return r;
            }
            if(c.current_index>=0&&(geometry||previous.current_index!=c.current_index)){auto offset=state_->viewport.snapshot();if(!offset)return std::unexpected(offset.error());
                const auto left=double(c.current_index)*g.tab_width,right=left+g.tab_width;auto x=offset->horizontal_scroll_offset;
                if(left<x)x=left;else if(right>x+g.viewport_width)x=right-g.viewport_width;
                if(auto r=state_->viewport.set_horizontal_scroll_offset(x);!r)return r;}
            state_->initialized=true;
            if(!state_->applying_automatic)state_->authored_index=authored_index;
            if(focused_key&&c.enabled){auto found=std::ranges::find_if(c.items,[&](const auto& item){return item.key==*focused_key&&item.enabled;});
                if(found!=c.items.end()){const auto index=std::size_t(found-c.items.begin());auto focused=state_->tabs[index].snapshot();
                    if(focused&&!focused->focused)if(auto r=state_->tabs[index].focus();!r)return r;}}
            return {};
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Mobile navigation update failed",std::current_exception());}
    }
};
} // namespace ii::gui
