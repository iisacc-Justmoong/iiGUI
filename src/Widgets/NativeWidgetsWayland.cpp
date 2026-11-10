#include <iiGUI/WaylandConnection.h>
#include <iiGUI/WidgetSurfaceNative.h>
#include <iiGUI/NativeCanvas.h>
#include <iiGUI/FontDatabase.h>
#include <iiGUI/TextBoundary.h>
#include "../Platform/WaylandImeSession.h"
namespace ii::gui::detail {
struct NativeWidgetSurface::Impl final {
    WaylandConnection& parent;WidgetSurfaceCallbacks callbacks;NativeCanvas canvas;bool dirty=true,live=true;
    WaylandImeSession ime;std::uint64_t claim=0;std::optional<std::uint64_t> ime_epoch;bool ime_dirty=true;
    Impl(WaylandConnection& p,WidgetSurfaceCallbacks value):parent(p),callbacks(std::move(value)),canvas(&parent,nullptr,Acceleration::prefer_hardware,callbacks.renderer,callbacks.presentations),claim(parent.claim_text_input()){}
    std::optional<WidgetSnapshot> focus(){for(auto& value:callbacks.accessibility())if(value.focused&&value.text_input&&value.visible&&value.enabled&&!value.input_blocked)return value;return {};}
    core::Result<TextContent> read(std::string_view id,TextRange range){return callbacks.text_content(id,range);}
    core::Result<void> send(WidgetInput event){return callbacks.text_service(std::move(event));}
    std::optional<std::uint64_t> focus_epoch()const{return parent.text_input_focus(claim);}
    std::optional<std::uint32_t> publish(std::uint64_t epoch,const WaylandImePublication& value){return parent.text_input_publish(claim,epoch,value);}
    std::optional<std::uint32_t> disable(std::uint64_t epoch){return parent.text_input_disable(claim,epoch);}
    void sync_ime(){
        if(!live)return;
        const bool changed=std::exchange(ime_dirty,false);
        auto current=focus_epoch();
        if(!current){if(ime_epoch)ime.leave(*this,*this,*ime_epoch);ime_epoch.reset();return;}
        if(ime_epoch!=current){ime.enter(*this,*this,*current);ime_epoch=current;}
        else if(changed)ime.refresh(*this,*this);
    }
    void text_event(const WaylandImeEvent& event){
        if(event.kind==WaylandImeEventKind::enter){if(focus_epoch()==event.epoch){ime.enter(*this,*this,event.epoch);ime_epoch=event.epoch;}}
        else if(event.kind==WaylandImeEventKind::leave){ime.leave(*this,*this,event.epoch);if(ime_epoch==event.epoch)ime_epoch.reset();}
        else if(ime_epoch==event.epoch)ime.done(*this,*this,event);
        ime_dirty=true;
    }
    void shutdown()noexcept{if(!live)return;live=false;try{ime.retire(*this,*this);}catch(...){}try{parent.release_text_input(claim);}catch(...){}}
    void flush(){if(!live)return;sync_ime();if(!dirty||!parent.configured())return;const auto size=parent.size();auto frame=callbacks.frame(size);if(!live)return;ime_dirty=true;sync_ime();if(!live)return;canvas.draw(frame,parent.scale(),size.height);dirty=false;}
    void input(WidgetInput value){
        if(value.kind==WidgetInputKind::key_down&&(value.control||value.meta)){
            std::optional<WidgetSnapshot> editor;for(const auto& item:callbacks.accessibility())if(item.focused&&item.visible&&item.text_input&&!item.input_blocked){editor=item;break;}
            if(editor){
                if(value.text=="a"||value.text=="A")value.key=WidgetKey::select_all;
                else if(value.text=="v"||value.text=="V"){auto clip=read_clipboard();if(clip){WidgetInput paste{.kind=WidgetInputKind::text};if(editor->kind==WidgetKind::input_field)paste.text=clip->text;else paste.content=std::make_shared<const ClipboardContent>(std::move(*clip));callbacks.input(std::move(paste));}return;}
                else if(value.text=="c"||value.text=="C"||value.text=="x"||value.text=="X"){
                    const bool cut=value.text=="x"||value.text=="X";
                    if(editor->kind!=WidgetKind::input_field)value.key=cut?WidgetKey::cut:WidgetKey::copy;
                    else{if(!editor->password&&editor->text_selection.length){auto index=TextIndex::create(editor->text);if(index){auto begin=index->byte_for_utf16(editor->text_selection.location),end=index->byte_for_utf16(editor->text_selection.location+editor->text_selection.length);if(begin&&end&&*end>=*begin)(void)write_clipboard({.text=editor->text.substr(*begin,*end-*begin)});}}if(!cut)return;value.key=WidgetKey::cut;}
                }
            }
        }
        callbacks.input(std::move(value));
    }
};
NativeWidgetSurface::NativeWidgetSurface(void* parent,WidgetSurfaceCallbacks value):impl_(std::make_unique<Impl>(*static_cast<WaylandConnection*>(parent),std::move(value))){}
NativeWidgetSurface::~NativeWidgetSurface(){impl_->shutdown();}
void NativeWidgetSurface::invalidate(){impl_->dirty=true;impl_->ime_dirty=true;}
void NativeWidgetSurface::accessibility_changed(){impl_->ime_dirty=true;}
void NativeWidgetSurface::accessibility_value_changed(std::string_view){impl_->ime_dirty=true;}
void NativeWidgetSurface::set_frame_paced(bool){}
void NativeWidgetSurface::flush(){auto state=impl_;state->flush();}
Size NativeWidgetSurface::size()const{return impl_->parent.size();}
double NativeWidgetSurface::backing_scale()const{return impl_->parent.scale();}
std::string NativeWidgetSurface::resolve_font_family(std::string_view value)const{return FontDatabase::application()->resolve_family(value);}
GraphicsInfo NativeWidgetSurface::info(){return impl_->canvas.info();}
PixelBuffer NativeWidgetSurface::capture(){auto state=impl_;state->flush();if(!state->live)throw std::runtime_error("Widget surface retired during capture");return state->canvas.capture();}
bool NativeWidgetSurface::native_event(const void* raw){auto state=impl_;try{const auto& event=*static_cast<const WaylandSurfaceEvent*>(raw);
    if(event.kind==WaylandEventKind::input&&event.input)state->input(*event.input);
    else if(event.kind==WaylandEventKind::ime&&event.ime)state->text_event(*event.ime);
    if(state->live){state->dirty=true;state->ime_dirty=true;state->sync_ime();}return true;
    }catch(...){if(state->live&&state->callbacks.failure)state->callbacks.failure(std::current_exception());return true;}}
}
