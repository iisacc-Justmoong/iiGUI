#include <iiGUI/NativeEventListenerView.h>
#include <iiGUI/WidgetState.h>
#include <algorithm>
#include <memory>
#include <limits>
#include <optional>
#include <stdexcept>
#include <thread>
#include <vector>

namespace {
void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
template<class T>T take(ii::core::Result<T> value){if(!value)throw std::runtime_error(value.error().message);return std::move(*value);}
void good(ii::core::Result<void> value){if(!value)throw std::runtime_error(value.error().message);}
void events(){
    auto tree=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});
    auto outer=take(tree->content_host({},"outer",{640,480}));std::optional<ii::gui::NativeEventListener> listener;
    std::vector<ii::gui::WidgetInput> events;std::vector<ii::gui::Rect> sizes;bool rejected_reentry=false;
    ii::gui::EventListenerCallbacks callbacks;
    callbacks.input=[&](const auto& event)->ii::core::Result<bool>{events.push_back(event);auto nested=listener->dispatch(event);
        rejected_reentry=!nested&&nested.error().code==ii::core::ErrorCode::busy;return true;};
    callbacks.resized=[&](ii::gui::Rect value){sizes.push_back(value);};
    good(outer.set_content([&](auto& builder)->ii::core::Result<void>{auto stack=builder.vstack({},"layout",{.spacing=0,.alignment=ii::gui::StackAlignment::leading});
        if(!stack)return std::unexpected(stack.error());auto created=ii::gui::NativeEventListener::create(builder,*stack,"listener",{320,180},callbacks);
        if(!created)return std::unexpected(created.error());listener=*created;return {};}));
    auto bounds=take(listener->root().snapshot()).bounds;require(!sizes.empty(),"Published listener did not report its initial layout");
    good(tree->input({.kind=ii::gui::WidgetInputKind::pointer_down,.x=bounds.x+5,.y=bounds.y+5}));
    require(events.size()==1&&events.back().kind==ii::gui::WidgetInputKind::pointer_down&&rejected_reentry,
        "Listener lost the input event or reentry protection");
    good(listener->set_options({400,240}));(void)take(listener->root().snapshot());
    require(sizes.size()>=2,"Listener did not report a changed extent");
    good(listener->set_options({0,240}));
    auto invalid=listener->set_options({-1,240});require(!invalid&&listener->root().snapshot(),"Invalid extent retired the listener");
    int replacement=0;good(listener->set_callbacks({.input=[&](const auto&)->ii::core::Result<bool>{++replacement;return false;}}));
    auto dispatched=listener->dispatch({.kind=ii::gui::WidgetInputKind::pointer_move});require(dispatched&&!*dispatched&&replacement==1,
        "Listener callback replacement failed");
    int expected_replacement=1;
#if defined(IIGUI_KEY_CANCEL_API)
    auto canceled=listener->dispatch({.kind=ii::gui::WidgetInputKind::key_cancel,.key=ii::gui::WidgetKey::space});
    require(canceled&&!*canceled&&replacement==2,"Listener rejected a canceled keyboard gesture");++expected_replacement;
#endif
#if defined(IIGUI_TEXT_SELECTION_API)
    auto selection=listener->dispatch({.kind=ii::gui::WidgetInputKind::text_selection,.replacement_range=ii::gui::TextRange{1,0}});
    ++expected_replacement;
    require(selection&&!*selection&&replacement==expected_replacement,"Listener rejected a text-service selection");
#endif
#if defined(IIGUI_POINTER_CANCEL_API)
    auto pointer_cancel=listener->dispatch({.kind=ii::gui::WidgetInputKind::pointer_cancel,.device=ii::gui::PointerDevice::touch});
    ++expected_replacement;
    require(pointer_cancel&&!*pointer_cancel&&replacement==expected_replacement,"Listener rejected a pointer-only cancellation");
#endif
    auto bad=ii::gui::WidgetInput{};bad.x=std::numeric_limits<double>::quiet_NaN();require(!listener->dispatch(bad)&&replacement==expected_replacement,
        "Invalid input reached the callback");
    bad=ii::gui::WidgetInput{};bad.text=std::string("x\0y",3);
    require(!listener->dispatch(bad)&&replacement==expected_replacement,"Input containing a NUL byte reached the callback");
    bad=ii::gui::WidgetInput{};bad.target=std::string("bad\xC0\x80",5);
    require(!listener->dispatch(bad)&&replacement==expected_replacement,"Invalid UTF-8 input target reached the callback");
    good(listener->set_callbacks({.input=[](const auto&)->ii::core::Result<bool>{throw std::runtime_error("Callback failed");}}));
    auto failed=listener->dispatch({});require(!failed&&failed.error().cause,"Listener did not preserve a callback exception");
    good(listener->set_callbacks({.input=[&](const auto&)->ii::core::Result<bool>{good(outer.clear_content());return true;}}));
    require(take(listener->dispatch({})),"In-flight listener did not finish after retirement");
    require(!listener->dispatch({.kind=ii::gui::WidgetInputKind::pointer_down}),"Retired listener accepted input");tree->close();
}
#if defined(IIGUI_KEY_CANCEL_API)
void keyboard_cancellation(){
    using namespace ii::gui;
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{200,100}));
    std::optional<ii::gui::NativeEventListener> listener;Widget button;
    int cancellations=0,activations=0,notices=0;
    good(host.set_content([&](auto& builder)->ii::core::Result<void>{
        auto result=ii::gui::NativeEventListener::create(builder,{},"listener",{200,100},
            {.input=[&](const WidgetInput& event)->ii::core::Result<bool>{
                if(event.kind==WidgetInputKind::key_cancel){++notices;return true;}return false;
            }});
        if(!result)return std::unexpected(result.error());listener=*result;
        auto child=builder.action_surface(listener->root(),"button",{.accessibility_name="Button",.width=60,.height=30},[&]{++activations;});
        if(!child)return std::unexpected(child.error());button=*child;
        return button.set_control_events({.canceled=[&]{++cancellations;}});
    }));
    (void)take(tree->drawing_snapshot({200,100}));good(button.focus());
    good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::space}));
    good(tree->input({.kind=WidgetInputKind::key_cancel,.key=WidgetKey::space}));
    good(tree->input({.kind=WidgetInputKind::key_up,.key=WidgetKey::space}));
    require(notices==1&&cancellations==1&&activations==0&&!take(button.snapshot()).pressed,
            "EventListener intercepted a terminal key cancellation");
    tree->close();
}
#endif
void reentrant_updates(){
    auto tree=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});
    auto outer=take(tree->content_host({},"outer",{640,480}));std::optional<ii::gui::NativeEventListener> listener;
    bool update_options=false,update_callbacks=false;int selected=0;
    ii::gui::EventListenerCallbacks callbacks;
    callbacks.resized=[&](ii::gui::Rect){
        if(update_options){update_options=false;good(listener->set_options({500,300}));}
        if(update_callbacks){update_callbacks=false;good(listener->set_callbacks({.input=[&](const auto&)->ii::core::Result<bool>{selected=2;return true;}}));}
    };
    good(outer.set_content([&](auto& builder)->ii::core::Result<void>{
        auto stack=builder.vstack({},"layout",{.spacing=0,.alignment=ii::gui::StackAlignment::leading});
        if(!stack)return std::unexpected(stack.error());
        auto created=ii::gui::NativeEventListener::create(builder,*stack,"listener",{320,180},callbacks);
        if(!created)return std::unexpected(created.error());listener=*created;return {};
    }));
    (void)take(listener->root().snapshot());
    good(listener->root().set_extent({321,181}));update_options=true;
    good(listener->set_options({400,240}));auto size=take(listener->root().snapshot()).size;
    require(!update_options&&size.width==500&&size.height==300,"A stale options update replaced the reentrant update");
    good(listener->root().set_extent({480,280}));update_callbacks=true;
    good(listener->set_callbacks({.input=[&](const auto&)->ii::core::Result<bool>{selected=1;return true;}}));
    require(take(listener->dispatch({}))&&selected==2,"A stale callback update replaced the reentrant update");
    bool rejected_options=false,rejected_callbacks=false,rejected_dispatch=false;
    std::thread other([&]{
        auto options=listener->set_options({400,240});rejected_options=!options&&options.error().code==ii::core::ErrorCode::wrong_executor;
        auto replacement=listener->set_callbacks({});rejected_callbacks=!replacement&&replacement.error().code==ii::core::ErrorCode::wrong_executor;
        auto input=listener->dispatch({});rejected_dispatch=!input&&input.error().code==ii::core::ErrorCode::wrong_executor;
    });other.join();
    require(rejected_options&&rejected_callbacks&&rejected_dispatch,"A foreign thread accessed listener state");tree->close();
}
void host_reentrant_input(){
    using K=ii::gui::WidgetInputKind;
    auto tree=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});
    auto outer=take(tree->content_host({},"outer",{640,480}));
    std::optional<ii::gui::NativeEventListener> listener;
    std::vector<ii::gui::WidgetInput> events;
    bool reenter=true, in_move=false, nested_callback=false;
    ii::gui::Rect bounds;
    ii::gui::EventListenerCallbacks callbacks;
    callbacks.input=[&](const ii::gui::WidgetInput& event)->ii::core::Result<bool>{
        if(in_move)nested_callback=true;
        events.push_back(event);
        if(event.kind==K::pointer_move&&reenter){
            reenter=false;in_move=true;
            good(tree->input({.kind=K::pointer_move,.x=bounds.x+20,.y=bounds.y+5,
                              .device=ii::gui::PointerDevice::pen,.pressure=.4,.timestamp=2,.buttons=1}));
            good(tree->input({.kind=K::pointer_move,.x=bounds.x+30,.y=bounds.y+5,
                              .device=ii::gui::PointerDevice::pen,.pressure=.8,.timestamp=3,.buttons=1}));
            // Hiding a captured listener synchronously produces focus_lost.
            good(listener->root().set_visible(false));
            in_move=false;
        }
        return true;
    };
    good(outer.set_content([&](auto& builder)->ii::core::Result<void>{
        auto created=ii::gui::NativeEventListener::create(builder,{},"listener",{320,180},callbacks);
        if(!created)return std::unexpected(created.error());listener=*created;return {};
    }));
    bounds=take(listener->root().snapshot()).bounds;
    good(tree->input({.kind=K::pointer_down,.x=bounds.x+5,.y=bounds.y+5,.buttons=1}));
    good(tree->input({.kind=K::pointer_move,.x=bounds.x+10,.y=bounds.y+5,.buttons=1}));
    require(!nested_callback&&events.size()==5&&events[0].kind==K::pointer_down&&
            events[1].kind==K::pointer_move&&events[2].pressure==.4&&events[2].timestamp==2&&
            events[3].pressure==.8&&events[3].timestamp==3&&events[4].kind==K::focus_lost,
            "Nested native input or capture cancellation was lost, reordered, or called recursively");
    good(listener->root().set_visible(true));
    good(tree->input({.kind=K::pointer_down,.x=bounds.x+5,.y=bounds.y+5,.buttons=1}));
    good(tree->input({.kind=K::pointer_up,.x=bounds.x+5,.y=bounds.y+5}));
    require(events.size()==7&&events.back().kind==K::pointer_up,
            "Listener did not recover after reentrant capture cancellation");
    tree->close();
}
void host_reentrant_release(){
    using K=ii::gui::WidgetInputKind;
    auto tree=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});
    auto outer=take(tree->content_host({},"outer",{640,480}));
    std::optional<ii::gui::NativeEventListener> listener;
    ii::gui::Rect bounds;std::vector<K> events;
    good(outer.set_content([&](auto& builder)->ii::core::Result<void>{
        auto created=ii::gui::NativeEventListener::create(builder,{},"listener",{320,180},
            {.input=[&](const ii::gui::WidgetInput& event)->ii::core::Result<bool>{
                events.push_back(event.kind);
                if(event.kind==K::pointer_down)
                    good(tree->input({.kind=K::pointer_up,.x=bounds.x+1000,.y=bounds.y+900}));
                return true;
            }});
        if(!created)return std::unexpected(created.error());listener=*created;return {};
    }));
    bounds=take(listener->root().snapshot()).bounds;
    good(tree->input({.kind=K::pointer_down,.x=bounds.x+5,.y=bounds.y+5,.buttons=1}));
    good(tree->input({.kind=K::pointer_move,.x=bounds.x+1000,.y=bounds.y+900}));
    require(events.size()>=2&&events[0]==K::pointer_down&&events[1]==K::pointer_up&&
            std::ranges::find(events,K::pointer_move)==events.end(),
            "A nested release left the listener capturing later pointer motion");
    tree->close();
}
void host_input_retirement_and_bounds(){
    using K=ii::gui::WidgetInputKind;
    auto tree=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});
    auto outer=take(tree->content_host({},"outer",{640,480}));
    std::optional<ii::gui::NativeEventListener> listener;
    ii::gui::Rect bounds;
    int old_calls=0,new_calls=0,cancels=0;
    bool replace=false,flood=false,retire=false;
    ii::gui::EventListenerCallbacks callbacks;
    callbacks.input=[&](const ii::gui::WidgetInput& event)->ii::core::Result<bool>{
        ++old_calls;
        if(event.kind==K::pointer_move&&replace){
            replace=false;
            good(tree->input({.kind=K::pointer_move,.x=bounds.x+20,.y=bounds.y+5,.buttons=1}));
            good(listener->set_callbacks({.input=[&](const auto&)->ii::core::Result<bool>{++new_calls;return true;}}));
        }else if(event.kind==K::pointer_move&&flood){
            flood=false;
            for(int i=0;i<300;++i)
                good(tree->input({.kind=K::pointer_move,.x=bounds.x+20,.y=bounds.y+5,.buttons=1}));
        }else if(event.kind==K::pointer_move&&retire){
            retire=false;
            good(tree->input({.kind=K::pointer_move,.x=bounds.x+20,.y=bounds.y+5,.buttons=1}));
            good(outer.clear_content());
        }else if(event.kind==K::focus_lost)++cancels;
        return true;
    };
    auto mount=[&]{good(outer.set_content([&](auto& builder)->ii::core::Result<void>{
        auto created=ii::gui::NativeEventListener::create(builder,{},"listener",{320,180},callbacks);
        if(!created)return std::unexpected(created.error());listener=*created;return {};
    }));bounds=take(listener->root().snapshot()).bounds;};
    auto down=[&]{good(tree->input({.kind=K::pointer_down,.x=bounds.x+5,.y=bounds.y+5,.buttons=1}));};
    auto move=[&]{good(tree->input({.kind=K::pointer_move,.x=bounds.x+10,.y=bounds.y+5,.buttons=1}));};
    mount();down();replace=true;move();
    require(old_calls==2&&new_calls==0,"An input queued for an old callback reached a replacement");
    move();require(new_calls==1,"Replacement callback did not receive later input");
    good(outer.clear_content());
    old_calls=0;mount();down();flood=true;move();
    require(old_calls==3&&cancels==1,"Input overflow did not cancel the active gesture once");
    good(outer.clear_content());
    old_calls=0;mount();down();retire=true;move();
    require(old_calls==2&&!listener->root().snapshot(),"Retired listener delivered queued input");
    tree->close();
}
void content_sizing(){
    using namespace ii::gui;
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    auto outer=take(tree->content_host({},"content-sized-host",{240,160}));
    std::optional<ii::gui::NativeEventListener> listener;Widget child;int presses=0;
    good(outer.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto root=take(builder.vstack({},"root",{.spacing=0,.alignment=StackAlignment::leading}));
        listener=take(ii::gui::NativeEventListener::create(builder,root,"content-sized",
            {.width=0,.height=0,.size_to_content=true},
            {.input=[&](const WidgetInput& value)->ii::core::Result<bool>{
                if(value.kind==WidgetInputKind::pointer_down)++presses;
                return true;}}));
        child=take(builder.absolute_stack(listener->root(),"child",{64,40}));return {};
    }));
    auto bounds=take(listener->root().snapshot()).bounds;
    require(bounds.width==64&&bounds.height==40,"Listener did not follow its child's natural size");
    good(tree->input({.kind=WidgetInputKind::pointer_down,.x=bounds.x+5,.y=bounds.y+5,.buttons=1}));
    good(tree->input({.kind=WidgetInputKind::pointer_up,.x=bounds.x+5,.y=bounds.y+5}));
    require(presses==1,"Content-sized listener lost pointer input");
    good(child.set_extent({80,50}));bounds=take(listener->root().snapshot()).bounds;
    require(bounds.width==80&&bounds.height==50,"Listener did not follow the resized child");
    good(listener->set_options({.width=100,.height=0,.size_to_content=true}));
    bounds=take(listener->root().snapshot()).bounds;
    require(bounds.width==100&&bounds.height==50,"Explicit listener axis did not override content sizing");
    require(!listener->set_options({.width=1,.height=1,.size_to_content=false})&&
            take(listener->root().snapshot()).bounds==bounds,"Changing a live sizing mode partially committed");
    good(child.set_visible(false));const auto implicit=take(listener->root().snapshot()).implicit_size;
    require(implicit.width==100&&implicit.height==0,"Hidden content contributed to intrinsic listener size");
    good(outer.clear_content());tree->close();
}

}
int main(){events();
#if defined(IIGUI_KEY_CANCEL_API)
keyboard_cancellation();
#endif
reentrant_updates();host_reentrant_input();host_reentrant_release();host_input_retirement_and_bounds();content_sizing();}
