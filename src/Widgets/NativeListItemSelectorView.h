#pragma once
#include <iiGUI/ListItemSelectorModel.h>
#include <iiGUI/NativeWidgets.h>
#include <cmath>
#include <cstdint>
#include <exception>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>
namespace ii::gui {
// The content host keeps
// the button and popup in the same generation so popup anchoring remains valid.
template<class Policy>
class BasicNativeListItemSelector final {
    using ListItemSelector=typename Policy::Model;using ListItemSelectorInput=typename Policy::Input;
    using NativeComboBox=typename Policy::NativeComboBox;using NativeMenu=typename Policy::NativeMenu;
    using NativeMenuItem=typename Policy::NativeMenuItem;using MenuItemInput=typename Policy::MenuItemInput;
    struct Assembly final {
        std::optional<NativeComboBox> control;
        std::optional<NativeMenu> menu;
        std::vector<NativeMenuItem> items;
    };
    struct State final {
        ListItemSelectorInput input;
        ListItemSelector model;
        ii::gui::Widget root;
        std::shared_ptr<Assembly> assembly;
        std::function<void(int)> changed;
        std::function<void(ii::gui::Rect)> layout_handler;
        std::optional<ii::core::Error> failure;
        const std::thread::id owner=std::this_thread::get_id();
        std::uint64_t revision=0;
        unsigned callbacks_active=0;
        bool live=false,retired=false,dirty=true;
    };
    explicit BasicNativeListItemSelector(std::shared_ptr<State> state):state_(std::move(state)){}
    [[nodiscard]] static bool valid(const ListItemSelectorInput& input)noexcept{
        return Policy::valid_accent(input.accent)&&ListItemSelector::validate(input.configuration)&&std::isfinite(input.width)&&input.width>=19&&input.width<=8176&&
            std::isfinite(input.height)&&input.height>=1&&input.height<=8192&&Policy::valid_text(input.accessibility_name);
    }
    [[nodiscard]] static ii::core::Result<void> check(const std::shared_ptr<State>& state){
        if(state->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"Selector requires its owner thread");
        if(state->retired)return ii::core::fail(ii::core::ErrorCode::closed,"Selector has retired");
        if(auto active=state->root.snapshot();!active)return std::unexpected(active.error());return {};
    }
    static void record(const std::shared_ptr<State>& state,ii::core::Error error)noexcept{
        if(state->retired)return;try{state->failure=std::move(error);}catch(...){}
    }
    [[nodiscard]] static ii::core::Result<void> apply(const std::shared_ptr<State>& state){
        auto assembly=state->assembly;if(!assembly||!assembly->control||!assembly->menu)return {};
        const auto revision=state->revision;const auto input=state->input;const auto model=state->model;const auto accent=input.accent;
        if(auto result=assembly->control->set_options(Policy::control_input(input,model));!result)return result;
        if(state->retired||state->revision!=revision)return {};
        if(auto result=assembly->menu->set_options(Policy::menu_input(input,model));!result)return result;
        if(state->retired||state->revision!=revision)return {};
        for(auto& item:assembly->items){
            auto options=item.options();if(!options)return std::unexpected(options.error());
            auto previous=*options;Policy::accent_item(*options,accent);
            if(*options!=previous)if(auto result=item.set_options(std::move(*options));!result)return result;
            if(state->retired||state->revision!=revision)return {};
        }
        return {};
    }
    struct CallbackGuard final {State& state;explicit CallbackGuard(State& value):state(value){++state.callbacks_active;}~CallbackGuard(){--state.callbacks_active;}};
    static void selected(const std::shared_ptr<State>& state,int index)noexcept{
        try{
            if(!state->live||!state->input.visible)return;
            const auto initial_revision=state->revision;auto next=state->model;const auto value=next.choose(index);if(!value)return;
            if(state->revision==(std::numeric_limits<std::uint64_t>::max)())throw std::out_of_range("Selector revision exhausted");
            if(state->assembly&&state->assembly->menu){auto snapshot=state->assembly->menu->root().snapshot();
                if(snapshot&&snapshot->opened)if(auto closed=state->assembly->menu->root().close_popup();!closed){record(state,closed.error());return;}}
            if(!state->live||state->revision!=initial_revision)return;
            const auto previous=state->model;const auto revision=++state->revision;state->model=std::move(next);
            if(auto result=apply(state);!result){if(state->revision==revision){state->model=previous;(void)apply(state);}record(state,result.error());return;}
            if(!state->live||state->revision!=revision)return;
            CallbackGuard guard(*state);if(state->changed){auto callback=state->changed;callback(*value);}
        }catch(...){try{record(state,{ii::core::ErrorCode::failed,"Selector change failed",std::current_exception()});}catch(...) {}}
    }
    static void open(const std::shared_ptr<State>& state)noexcept{
        try{
            if(!state->live||!state->input.visible||!state->model.configuration().enabled||state->model.configuration().options.empty()||!state->assembly)return;
            auto assembly=state->assembly;if(!assembly->control||!assembly->menu)return;
            const auto revision=state->revision;auto snapshot=assembly->control->root().snapshot();if(!snapshot){record(state,snapshot.error());return;}
            if(!state->live||state->revision!=revision)return;
            if(auto result=assembly->menu->root().open_for(assembly->control->root(),0,snapshot->bounds.height);!result)record(state,result.error());
        }catch(...){try{record(state,{ii::core::ErrorCode::failed,"Selector menu opening failed",std::current_exception()});}catch(...) {}}
    }
    [[nodiscard]] static ii::core::Result<void> render(const std::shared_ptr<State>& state){
        if(state->retired)return ii::core::fail(ii::core::ErrorCode::closed,"Selector has retired");
        if(!state->dirty&&state->assembly)return {};
        if(state->callbacks_active)return ii::core::fail(ii::core::ErrorCode::busy,"Selector replacement is deferred until its callback returns");
        try{
            const auto revision=state->revision;auto assembly=std::make_shared<Assembly>();std::weak_ptr<State> weak=state;const auto configuration=state->model.configuration();
            const auto control=Policy::control_input(state->input,state->model);const auto menu=Policy::menu_input(state->input,state->model);const auto accent=state->input.accent;const auto width=state->input.width,height=state->input.height;
            auto built=state->root.set_content([weak,assembly,configuration,control,menu,accent,width,height](ii::gui::WidgetBuilder& builder)->ii::core::Result<void>{
                auto root=builder.absolute_stack({},"surface",{width,height});if(!root)return std::unexpected(root.error());
                auto button=NativeComboBox::create(builder,*root,"control",control,{.clicked=[weak]{if(auto state=weak.lock())open(state);}});
                if(!button)return std::unexpected(button.error());assembly->control=*button;
                if(auto frame=button->root().set_frame({0,0,float(width),float(height)});!frame)return frame;
                if(auto handler=button->root().set_input_handler([weak](const ii::gui::WidgetInput& event)->ii::core::Result<bool>{
                    auto state=weak.lock();if(!state||!state->live)return false;
                    if(event.kind!=ii::gui::WidgetInputKind::key_down||(event.key!=ii::gui::WidgetKey::up&&event.key!=ii::gui::WidgetKey::down))return false;
                    auto next=state->model;auto index=next.navigate(event.key==ii::gui::WidgetKey::up?-1:1);if(index)selected(state,*index);return true;
                });!handler)return handler;
                auto popup=NativeMenu::create(builder,*root,"menu",menu,{.triggered=[weak](int index){if(auto state=weak.lock())selected(state,index);}});
                if(!popup)return std::unexpected(popup.error());assembly->menu=*popup;assembly->items.reserve(configuration.options.size());
                for(std::size_t i=0;i<configuration.options.size();++i){
                    MenuItemInput item;item.configuration.label=configuration.options[i].text;item.configuration.enabled=configuration.options[i].enabled;
                    item.configuration.icon_name.clear();item.configuration.show_icon_slot=false;item.configuration.has_children=false;
                    item.configuration.show_chevron=false;item.configuration.key_visible=false;item.configuration.compact=true;
                    Policy::accent_item(item,accent);
                    auto view=NativeMenuItem::create(builder,popup->content_root(),"option-"+std::to_string(i),std::move(item));
                    if(!view)return std::unexpected(view.error());if(auto registered=popup->register_item(view->weak_group_port());!registered)return registered;
                    assembly->items.push_back(*view);
                }
                return popup->finalize();
            });
            if(!built)return built;
            if(state->retired)return ii::core::fail(ii::core::ErrorCode::closed,"Selector retired during construction");
            if(state->retired||state->revision!=revision)return {};
            state->assembly=std::move(assembly);state->dirty=false;return {};
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Selector construction failed",std::current_exception());}
    }
    [[nodiscard]] static ii::core::Result<void> update(const std::shared_ptr<State>& state,ListItemSelectorInput input){
        if(auto valid_owner=check(state);!valid_owner)return valid_owner;
        if(!valid(input))return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid selector configuration");
        try{
            if(input==state->input)return {};
            auto echo=state->input;echo.configuration.selected_index=input.configuration.selected_index;
            if(echo==input&&input.configuration.selected_index==state->model.configuration().selected_index){state->input=std::move(input);return {};}
            if(state->revision==(std::numeric_limits<std::uint64_t>::max)())return ii::core::fail(ii::core::ErrorCode::out_of_range,"Selector revision exhausted");
            const auto previous=state->input;const auto old_model=state->model;const auto old_dirty=state->dirty;
            auto next=state->model;auto configuration=input.configuration;
            if(configuration.options==previous.configuration.options&&configuration.selected_index==previous.configuration.selected_index)
                configuration.selected_index=state->model.configuration().selected_index;
            if(!next.set_configuration(std::move(configuration)))return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid selector model");
            const bool replacement=input.configuration.options!=previous.configuration.options||input.width!=previous.width||input.height!=previous.height;
            const auto revision=++state->revision;state->input=std::move(input);state->model=std::move(next);state->dirty|=replacement;
            const auto rollback=[&]{if(!state->retired&&state->revision==revision){state->input=previous;state->model=old_model;state->dirty=old_dirty;
                (void)state->root.set_extent({previous.width,previous.height});(void)state->root.set_visible(previous.visible);(void)apply(state);}};
            if(state->assembly&&state->assembly->menu&&(!state->input.visible||!state->model.configuration().enabled)){
                auto menu=state->assembly->menu->root().snapshot();if(!menu){rollback();return std::unexpected(menu.error());}
                if(menu->opened)if(auto result=state->assembly->menu->root().close_popup();!result){rollback();return result;}
                if(state->retired||state->revision!=revision)return {};
            }
            if(auto result=state->root.set_extent({state->input.width,state->input.height});!result){rollback();return result;}
            if(state->retired||state->revision!=revision)return {};
            if(auto result=state->root.set_visible(state->input.visible);!result){rollback();return result;}
            if(state->retired||state->revision!=revision)return {};
            if(state->dirty&&!state->input.visible){state->failure.reset();return {};}
            if(state->dirty&&state->callbacks_active){if(auto result=state->root.set_layout_handler(state->layout_handler);!result){rollback();return result;}return {};}
            const bool rebuilding=state->dirty;
            if(auto result=render(state);!result){rollback();return result;}
            if(state->retired||state->revision!=revision)return {};
            if(!rebuilding)if(auto result=apply(state);!result){rollback();return result;}state->failure.reset();return {};
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Selector update failed",std::current_exception());}
    }
public:
    [[nodiscard]] static ii::core::Result<BasicNativeListItemSelector> create(ii::gui::WidgetBuilder& builder,ii::gui::Widget parent,
            std::string_view id,ListItemSelectorInput input={},std::function<void(int)> changed={}){
        if(!valid(input))return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid selector configuration");
        try{
            auto state=std::make_shared<State>();state->input=std::move(input);(void)state->model.set_configuration(state->input.configuration);state->changed=std::move(changed);
            auto root=builder.content_host(parent,id,{state->input.width,state->input.height});if(!root)return std::unexpected(root.error());state->root=*root;
            if(auto visible=root->set_visible(state->input.visible);!visible)return std::unexpected(visible.error());std::weak_ptr<State> weak=state;
            state->layout_handler=[weak](ii::gui::Rect){if(auto state=weak.lock();state&&state->live&&state->dirty){
                auto result=render(state);if(!result){record(state,result.error());if(result.error().code==ii::core::ErrorCode::busy){
                    (void)state->root.set_layout_handler(state->layout_handler);throw std::runtime_error(result.error().message);}}
            }};
            if(auto handler=root->set_layout_handler(state->layout_handler);!handler)return std::unexpected(handler.error());
            if(auto retained=builder.retain(state);!retained)return std::unexpected(retained.error());
            if(auto published=builder.on_publish([weak]{if(auto state=weak.lock()){
                if(state->retired)return;state->live=true;if(auto armed=state->root.set_layout_handler(state->layout_handler);!armed)record(state,armed.error());
            }});!published)return std::unexpected(published.error());
            if(auto retired=builder.on_retire([weak]{if(auto state=weak.lock()){
                state->live=false;state->retired=true;if(state->revision!=(std::numeric_limits<std::uint64_t>::max)())++state->revision;
                state->assembly.reset();state->changed={};state->layout_handler={};state->failure.reset();
                state->input.configuration.options.clear();state->input.configuration.placeholder.clear();state->input.accent.reset();state->model.release();
            }});!retired)return std::unexpected(retired.error());
            return BasicNativeListItemSelector(std::move(state));
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Selector creation failed",std::current_exception());}
    }
    [[nodiscard]] ii::gui::Widget root()const{return state_->root;}
    [[nodiscard]] ii::core::Result<ListItemSelectorInput> options()const{
        if(auto valid=check(state_);!valid)return std::unexpected(valid.error());return state_->input;
    }
    [[nodiscard]] ii::core::Result<int> selected_index()const{
        if(auto valid=check(state_);!valid)return std::unexpected(valid.error());return state_->model.configuration().selected_index;
    }
    [[nodiscard]] ii::core::Result<void> set_options(ListItemSelectorInput input)const{return update(state_,std::move(input));}
    [[nodiscard]] ii::core::Result<ii::gui::Widget> control()const{
        if(auto valid=check(state_);!valid)return std::unexpected(valid.error());if(auto ready=render(state_);!ready)return std::unexpected(ready.error());if(state_->retired||!state_->assembly)return ii::core::fail(ii::core::ErrorCode::closed,"Selector has retired");return state_->assembly->control->root();
    }
    [[nodiscard]] ii::core::Result<ii::gui::Widget> menu()const{
        if(auto valid=check(state_);!valid)return std::unexpected(valid.error());if(auto ready=render(state_);!ready)return std::unexpected(ready.error());if(state_->retired||!state_->assembly)return ii::core::fail(ii::core::ErrorCode::closed,"Selector has retired");return state_->assembly->menu->root();
    }
    [[nodiscard]] ii::core::Result<ii::gui::Widget> option(std::size_t index)const{
        if(auto valid=check(state_);!valid)return std::unexpected(valid.error());if(auto ready=render(state_);!ready)return std::unexpected(ready.error());
        if(state_->retired||!state_->assembly)return ii::core::fail(ii::core::ErrorCode::closed,"Selector has retired");
        if(index>=state_->assembly->items.size())return ii::core::fail(ii::core::ErrorCode::out_of_range,"Selector option is out of range");return state_->assembly->items[index].root();
    }
    [[nodiscard]] ii::core::Result<std::optional<ii::core::Error>> failure()const{
        if(auto valid=check(state_);!valid)return std::unexpected(valid.error());return state_->failure;
    }
private:
    std::shared_ptr<State> state_;
};

} // namespace ii::gui
