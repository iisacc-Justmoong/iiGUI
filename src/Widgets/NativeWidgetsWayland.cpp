#include <iiGUI/WaylandConnection.h>
#include <iiGUI/WidgetSurface.h>
#include <iiGUI/NativeCanvas.h>
#include <iiGUI/FontDatabase.h>
#include <iiGUI/TextBoundary.h>
namespace ii::gui::detail {
struct WidgetSurface::Impl final {
    WaylandConnection& parent;WidgetSurfaceCallbacks callbacks;NativeCanvas canvas;bool dirty=true,live=true;
    Impl(WaylandConnection& p,WidgetSurfaceCallbacks value):parent(p),callbacks(std::move(value)),canvas(&parent,nullptr,Acceleration::prefer_hardware,callbacks.renderer,callbacks.presentations){}
    void flush(){if(!live||!dirty||!parent.configured())return;const auto size=parent.size();auto frame=callbacks.frame(size);if(!live)return;canvas.draw(frame,parent.scale(),size.height);dirty=false;}
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
WidgetSurface::WidgetSurface(void* parent,WidgetSurfaceCallbacks value):impl_(std::make_unique<Impl>(*static_cast<WaylandConnection*>(parent),std::move(value))){}
WidgetSurface::~WidgetSurface(){impl_->live=false;}
void WidgetSurface::invalidate(){impl_->dirty=true;}
void WidgetSurface::accessibility_changed(){}
void WidgetSurface::accessibility_value_changed(std::string_view){}
void WidgetSurface::set_frame_paced(bool){}
void WidgetSurface::flush(){auto state=impl_;state->flush();}
Size WidgetSurface::size()const{return impl_->parent.size();}
double WidgetSurface::backing_scale()const{return impl_->parent.scale();}
std::string WidgetSurface::resolve_font_family(std::string_view value)const{return FontDatabase::application()->resolve_family(value);}
GraphicsInfo WidgetSurface::info(){return impl_->canvas.info();}
PixelBuffer WidgetSurface::capture(){auto state=impl_;state->flush();if(!state->live)throw std::runtime_error("Widget surface retired during capture");return state->canvas.capture();}
bool WidgetSurface::native_event(const void* raw){auto state=impl_;try{const auto& event=*static_cast<const WaylandSurfaceEvent*>(raw);if(event.kind==WaylandEventKind::input&&event.input)state->input(*event.input);if(state->live)state->dirty=true;return true;}catch(...){if(state->live&&state->callbacks.failure)state->callbacks.failure(std::current_exception());return true;}}
}
