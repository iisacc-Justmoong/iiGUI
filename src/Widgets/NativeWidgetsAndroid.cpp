#include <iiGUI/FontDatabase.h>
#include <iiGUI/AndroidSurface.h>
#include <iiGUI/WidgetSurfaceNative.h>
#include <iiGUI/NativeCanvas.h>
#include <iiGUI/Desktop.h>
#include "../Platform/AndroidKeyboardDispatch.h"
#include "../Platform/AndroidIme.h"
#include "../Platform/AndroidKeyboardMap.h"
#include "../Platform/AndroidKeyboardNative.h"
#include "../Platform/AndroidPointerNative.h"
#include <android/looper.h>

namespace ii::gui::detail {
struct NativeWidgetSurface::Impl final {
    AndroidSurfaceParent& parent;
    WidgetSurfaceCallbacks callbacks;
    std::unique_ptr<NativeCanvas> canvas;
    std::unique_ptr<AndroidIme> ime;
    AndroidPointerState pointer;
    AndroidKeyboardDispatcher keyboard;
    bool dirty=true,live=true;
    Impl(AndroidSurfaceParent& p,WidgetSurfaceCallbacks c):parent(p),callbacks(std::move(c)){reset();}
    void reset(){canvas.reset();if(parent.window)canvas=std::make_unique<NativeCanvas>(parent.window,nullptr,Acceleration::prefer_hardware,callbacks.renderer,callbacks.presentations);dirty=true;}
    Size size()const{return parent.window?Size{ANativeWindow_getWidth(parent.window)/parent.scale,ANativeWindow_getHeight(parent.window)/parent.scale}:Size{};}
    void invalidate(){if(!live)return;dirty=true;ALooper_wake(parent.app->looper);}
    void flush(){if(!live)return;if(ime)ime->changed();if(!live||!dirty||!canvas||!parent.active)return;const auto extent=size();if(extent.width<=0||extent.height<=0)return;auto frame=callbacks.frame(extent);if(!live)return;canvas->draw(frame,parent.scale,extent.height);dirty=false;}
    void deliver(WidgetInput input){if(!live)return;callbacks.input(std::move(input));if(live)invalidate();}
    bool send(WidgetInput input){deliver(std::move(input));return live;}
    std::optional<WidgetSnapshot> focus(){
        if(!live||!callbacks.accessibility)return {};
        std::optional<WidgetSnapshot> result;
        for(auto& value:callbacks.accessibility())if(value.focused&&value.visible&&value.enabled&&!value.input_blocked){
            if(value.text_input)return value;result=std::move(value);
        }
        return result;
    }
    core::Result<TextContent> read(std::string_view id,TextRange range){
        if(!live||!callbacks.text_content)return core::fail(core::ErrorCode::closed,"Android text surface is unavailable");
        return callbacks.text_content(id,range);
    }
    core::Result<ClipboardContent> read_clipboard(){return ii::gui::read_clipboard();}
    core::Result<void> write_clipboard(const ClipboardContent& value){return ii::gui::write_clipboard(value);}
    void lose_focus(){
        keyboard.reset();
        if(auto canceled=pointer.cancel())deliver(std::move(*canceled));
        else {WidgetInput lost;lost.kind=WidgetInputKind::focus_lost;deliver(std::move(lost));}
    }
    bool event(const AndroidSurfaceEvent& e){
        if(e.kind==AndroidSurfaceEventKind::window){if(!parent.window)lose_focus();if(live)reset();if(live&&ime)ime->changed();return true;}
        if(e.kind==AndroidSurfaceEventKind::activation){if(!parent.active)lose_focus();if(live)invalidate();if(live&&ime)ime->changed();return true;}
        if(e.kind==AndroidSurfaceEventKind::focus_lost){lose_focus();if(live&&ime)ime->changed();return true;}
        if(e.kind==AndroidSurfaceEventKind::resize){invalidate();return true;}
        if(!e.input)return false;
        if(AInputEvent_getType(e.input)==AINPUT_EVENT_TYPE_KEY){
            if(!parent.active)return false;
            AndroidKeyboardMap map;auto frame=android_keyboard_frame(e.input,map);if(!frame)return false;
            return keyboard.input(*frame,*this,[&](std::uint32_t accent,std::uint32_t character){return map.combine(accent,character);});
        }
        if(AInputEvent_getType(e.input)!=AINPUT_EVENT_TYPE_MOTION)return false;
        const auto action=AMotionEvent_getAction(e.input)&AMOTION_EVENT_ACTION_MASK;
        if(action==AMOTION_EVENT_ACTION_DOWN||action==AMOTION_EVENT_ACTION_POINTER_DOWN)keyboard.reset();
        auto frames=android_pointer_frames(e.input,parent.scale);if(!frames)return false;
        for(const auto& frame:*frames){
            if((frame.action==AndroidPointerAction::down||frame.action==AndroidPointerAction::secondary_down)&&callbacks.touch_begin)callbacks.touch_begin(frame.time);
            if(!dispatch_android_pointer(pointer,pointer.translate(frame),[&](WidgetInput input){deliver(std::move(input));return live;}))return true;
            if((frame.action==AndroidPointerAction::cancel||frame.canceled)&&callbacks.touch_cancel)callbacks.touch_cancel();
            else if(frame.action==AndroidPointerAction::up&&callbacks.touch_end)callbacks.touch_end(frame.time);
        }
        return true;
    }
};
NativeWidgetSurface::NativeWidgetSurface(void* parent,WidgetSurfaceCallbacks callbacks):impl_(std::make_unique<Impl>(*static_cast<AndroidSurfaceParent*>(parent),std::move(callbacks))){
    std::weak_ptr<Impl> weak=impl_;
    AndroidImeHost host{
        .focused=[weak]()->std::optional<WidgetSnapshot>{auto state=weak.lock();return state&&state->live&&state->parent.active?state->focus():std::nullopt;},
        .content=[weak](std::string_view id,TextRange range)->core::Result<TextContent>{auto state=weak.lock();return state&&state->live?state->read(id,range):core::Result<TextContent>{core::fail(core::ErrorCode::closed,"Android text surface closed")};},
        .input=[weak](WidgetInput event)->core::Result<void>{auto state=weak.lock();if(!state||!state->live||!state->parent.active||!state->callbacks.text_service)return core::fail(core::ErrorCode::closed,"Android text surface closed");return state->callbacks.text_service(std::move(event));}
    };
    const auto sender=impl_->parent.sender;
    impl_->ime=AndroidIme::create(std::move(host),[sender](runtime::Task task){return bool(sender.post(std::move(task)));});
}
NativeWidgetSurface::~NativeWidgetSurface(){impl_->live=false;if(impl_->ime)impl_->ime->close();}
void NativeWidgetSurface::invalidate(){impl_->invalidate();}
void NativeWidgetSurface::accessibility_changed(){auto state=impl_;if(state->live&&state->ime)state->ime->changed();}
void NativeWidgetSurface::accessibility_value_changed(std::string_view){auto state=impl_;if(state->live&&state->ime)state->ime->changed();}
void NativeWidgetSurface::set_frame_paced(bool){}
void NativeWidgetSurface::flush(){auto state=impl_;state->flush();}
Size NativeWidgetSurface::size()const{return impl_->size();}
double NativeWidgetSurface::backing_scale()const{return impl_->parent.scale;}
std::string NativeWidgetSurface::resolve_font_family(std::string_view value)const{return FontDatabase::application()->resolve_family(value);}
GraphicsInfo NativeWidgetSurface::info(){return impl_->canvas?impl_->canvas->info():GraphicsInfo{};}
PixelBuffer NativeWidgetSurface::capture(){auto state=impl_;state->flush();if(!state->live||!state->canvas)throw std::runtime_error("Android surface is suspended or retired");return state->canvas->capture();}
bool NativeWidgetSurface::native_event(const void* event){auto state=impl_;try{return state->event(*static_cast<const AndroidSurfaceEvent*>(event));}catch(...){if(state->live&&state->callbacks.failure)state->callbacks.failure(std::current_exception());return true;}}
}
