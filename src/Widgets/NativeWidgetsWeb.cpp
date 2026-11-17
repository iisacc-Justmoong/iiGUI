#include <iiGUI/WebSurface.h>
#include <iiGUI/WidgetSurfaceNative.h>
#include <iiGUI/NativeCanvas.h>
#include <iiGUI/FontDatabase.h>
#include "WebAccessibility.h"
#include "WebTextInputDom.h"
#include <emscripten.h>
#include <limits>
#include <utility>
namespace ii::gui::detail {
struct NativeWidgetSurface::Impl final {
    WebSurfaceParent& parent;WidgetSurfaceCallbacks callbacks;std::unique_ptr<NativeCanvas> canvas;bool dirty=true,lost=false,live=true,accessibility_dirty=true,text_dirty=false;
    std::uint32_t accessibility_generation=0;
    std::uint32_t text_generation=0,text_acknowledged=0;
    std::string text_id;
    std::optional<TextInputRevision> text_focus;
    Impl(WebSurfaceParent& p,WidgetSurfaceCallbacks c):parent(p),callbacks(std::move(c)){reset();}
    void reset(){canvas.reset();canvas=std::make_unique<NativeCanvas>(&parent,nullptr,Acceleration::prefer_hardware,callbacks.renderer,callbacks.presentations);dirty=true;lost=false;}
    void accessibility(){
        const auto snapshots=callbacks.accessibility();if(!live)return;
        const auto nodes=web_accessibility_nodes(snapshots);const auto json=web_accessibility_json(nodes);if(!live)return;
        if(!accessibility_generation){if(parent.accessibility_serial==(std::numeric_limits<std::uint32_t>::max)())throw std::overflow_error("Web accessibility generation exhausted");accessibility_generation=++parent.accessibility_serial;}
        parent.accessibility_active.store(accessibility_generation,std::memory_order_release);
        MAIN_THREAD_EM_ASM({
            /* IIGUI_WEB_ACCESSIBILITY_PUBLISH_BEGIN */
            (function(){
                const generation=$1>>>0,owner=$2,canvas=document.querySelector('#canvas');
                let state=Module['iiGUIAccessibility'];
                if(state&&state.generation!==generation){state.dispose();delete Module['iiGUIAccessibility'];state=null;}
                if(!canvas||!canvas.hasAttribute('data-iigui-accessibility'))return;
                if(!state){
                    const root=document.createElement('div');
                    root.style.position='fixed';root.style.left='0';root.style.top='0';root.style.width='1px';root.style.height='1px';
                    root.style.overflow='hidden';root.style.clipPath='inset(50%)';root.style.whiteSpace='nowrap';
                    canvas.parentNode.insertBefore(root,canvas.nextSibling);
                    state={generation,owner,root,active:true,peers:new Map(),pendingFocus:null};
                    state.dispose=()=>{state.active=false;state.root.remove();state.peers.clear();};
                    Module['iiGUIAccessibility']=state;
                }
                const send=(peer,action)=>{
                    if(!state.active||state.peers.get(peer.id)!==peer||!peer.enabled||!peer.actionable)return false;
                    const size=lengthBytesUTF8(peer.id)+1,ptr=_malloc(size);if(!ptr)return false;
                    try{stringToUTF8(peer.id,ptr,size);_iigui_web_accessibility_event(state.owner,ptr,state.generation,action);}finally{_free(ptr);}
                    return true;
                };
                state.focusById=id=>{const peer=state.peers.get(id);return peer?send(peer,0):false;};
                state.blur=()=>{for(const peer of state.peers.values())if(send(peer,8))return true;return false;};
                const nodes=JSON.parse(UTF8ToString($0)),seen=new Set();let position=state.root.firstChild,focused=null;
                const focusWasInPeers=state.root.contains(document.activeElement);
                for(const node of nodes){
                    seen.add(node.id);let peer=state.peers.get(node.id);
                    if(!peer){
                        const element=document.createElement('span');peer={id:node.id,element,enabled:false,actionable:false,role:''};
                        element.addEventListener('focus',()=>{state.pendingFocus=peer.id;send(peer,0);});
                        element.addEventListener('click',event=>{event.preventDefault();if(peer.role!=='slider')send(peer,1);});
                        element.addEventListener('keydown',event=>{
                            if(peer.role==='slider'){
                                const actions={ArrowLeft:2,ArrowRight:3,ArrowUp:4,ArrowDown:5,Home:6,End:7};
                                if(Object.prototype.hasOwnProperty.call(actions,event.key)){event.preventDefault();send(peer,actions[event.key]);}
                            }
                            else if(event.key==='Enter'){event.preventDefault();if(!event.repeat)send(peer,1);}
                            else if(event.key===' '&&peer.role!=='link')event.preventDefault();
                            else if(peer.role==='radio'&&(event.key==='ArrowLeft'||event.key==='ArrowUp')){event.preventDefault();send(peer,2);}
                            else if(peer.role==='radio'&&(event.key==='ArrowRight'||event.key==='ArrowDown')){event.preventDefault();send(peer,3);}
                        });
                        element.addEventListener('keyup',event=>{if(event.key===' '&&peer.role!=='link'&&peer.role!=='slider'){event.preventDefault();send(peer,1);}});
                        state.peers.set(node.id,peer);
                    }
                    peer.enabled=node.enabled;peer.actionable=node.actionable;peer.role=node.role;
                    const element=peer.element;element.textContent=node.name;
                    element.setAttribute('data-iigui-id',node.id);
                    if(Array.isArray(node.bounds)&&node.bounds.length===4&&node.bounds.every(Number.isFinite))element.setAttribute('data-iigui-bounds',node.bounds.join(','));
                    else element.removeAttribute('data-iigui-bounds');
                    if(node.role==='text')element.removeAttribute('role');else element.setAttribute('role',node.role);
                    if(node.enabled)element.removeAttribute('aria-disabled');else element.setAttribute('aria-disabled','true');
                    if(node.role==='checkbox'||node.role==='radio'||node.role==='switch')element.setAttribute('aria-checked',node.checked?'true':'false');
                    else element.removeAttribute('aria-checked');
                    if(node.role==='slider'||node.role==='progressbar'){
                        element.setAttribute('aria-valuemin',String(node.minimum));element.setAttribute('aria-valuemax',String(node.maximum));element.setAttribute('aria-valuenow',String(node.value));
                    }else{element.removeAttribute('aria-valuemin');element.removeAttribute('aria-valuemax');element.removeAttribute('aria-valuenow');}
                    if(node.role==='text')element.removeAttribute('tabindex');else element.setAttribute('tabindex',node.enabled&&node.actionable?'0':'-1');
                    if(element!==position){const focused=document.activeElement===element;state.root.insertBefore(element,position);
                        if(focused&&document.activeElement!==element)element.focus({preventScroll:true});}
                    position=element.nextSibling;
                    if(node.focused&&node.enabled&&node.actionable){focused=element;if(state.pendingFocus===node.id)state.pendingFocus=null;}
                }
                for(const [id,peer] of state.peers)if(!seen.has(id)){peer.element.remove();state.peers.delete(id);}
                if(state.pendingFocus){const pending=state.peers.get(state.pendingFocus);if(!pending||!pending.enabled||!pending.actionable)state.pendingFocus=null;}
                if(focusWasInPeers&&!state.pendingFocus&&focused&&document.activeElement!==focused)focused.focus({preventScroll:true});
            })();
            /* IIGUI_WEB_ACCESSIBILITY_PUBLISH_END */
        },json.c_str(),accessibility_generation,&parent);
    }
    void retire_accessibility(){
        const auto retired=std::exchange(accessibility_generation,0);if(!retired)return;
        auto expected=retired;(void)parent.accessibility_active.compare_exchange_strong(expected,0,std::memory_order_acq_rel);
        MAIN_THREAD_EM_ASM({
            /* IIGUI_WEB_ACCESSIBILITY_REMOVE_BEGIN */
            const state=Module['iiGUIAccessibility'];if(state&&state.generation===($1>>>0)){state.dispose();delete Module['iiGUIAccessibility'];}
            /* IIGUI_WEB_ACCESSIBILITY_REMOVE_END */
        },0,retired);
    }
    void retire_text(){
        const auto retired=std::exchange(text_generation,0);text_id.clear();text_focus.reset();text_acknowledged=0;
        if(!retired)return;
        auto expected=retired;(void)parent.text_active.compare_exchange_strong(expected,0,std::memory_order_acq_rel);
        web_text_remove(retired);
    }
    std::optional<TextContent> editor_text(const WidgetSnapshot& editor){
        if(editor.text_length>web_text_limit)return {};
        auto content=callbacks.text_content(editor.id,{0,editor.text_length});
        if(!content||content->range!=TextRange{0,editor.text_length}||content->text.size()>web_text_limit)return {};
        return std::move(*content);
    }
    void publish_text(){
        auto editor=callbacks.focused_editor();if(!live)return;
        if(!editor||!editor->attached||!editor->enabled||!editor->visible||!editor->focused||editor->input_blocked||!editor->text_revision){retire_text();return;}
        auto content=editor_text(*editor);if(!live)return;
        if(!content){retire_text();return;}
        if(!text_generation||!text_focus||!web_text_same_focus(*editor,text_id,*text_focus)){
            retire_text();
            if(parent.text_serial==(std::numeric_limits<std::uint32_t>::max)())throw std::overflow_error("Web text generation exhausted");
            text_generation=++parent.text_serial;text_id=editor->id;text_focus=*editor->text_revision;
            parent.text_active.store(text_generation,std::memory_order_release);
        }
        web_text_publish(&parent,text_generation,text_acknowledged,*editor,content->text);
    }
    void text_event(const WebTextMessage& message){
        if(!text_generation||message.generation!=text_generation||message.sequence<=text_acknowledged)return;
        const bool ordered=message.sequence==text_acknowledged+1;text_acknowledged=message.sequence;text_dirty=true;
        if(!ordered||message.id!=text_id||!text_focus)return;
        auto editor=callbacks.focused_editor();if(!live||!editor||!web_text_same_focus(*editor,text_id,*text_focus))return;
        const bool mutation=message.action==WebTextAction::replace||message.action==WebTextAction::composition||
                            message.action==WebTextAction::commit||message.action==WebTextAction::cancel;
        if(editor->read_only&&mutation)return;
        if((mutation||message.action==WebTextAction::selection)&&!message.has_before)return;
        if(message.has_before){auto current=editor_text(*editor);if(!live||!current||current->text!=message.before)return;}
        auto input=web_text_input(message);if(!input)return;
        if(input->text_service){if(callbacks.text_service)(void)callbacks.text_service(std::move(*input));}
        else callbacks.input(std::move(*input));
    }
    void flush(){if(!live||!dirty||lost||!parent.visible||!canvas)return;auto frame=callbacks.frame(parent.size);if(!live)return;canvas->draw(frame,parent.scale,parent.size.height);
        if(text_dirty){text_dirty=false;publish_text();if(!live)return;}
        if(parent.accessibility_enabled&&accessibility_dirty){accessibility_dirty=false;accessibility();}dirty=accessibility_dirty&&parent.accessibility_enabled;}
};
NativeWidgetSurface::NativeWidgetSurface(void* parent,WidgetSurfaceCallbacks callbacks):impl_(std::make_unique<Impl>(*static_cast<WebSurfaceParent*>(parent),std::move(callbacks))){}
NativeWidgetSurface::~NativeWidgetSurface(){impl_->live=false;impl_->retire_text();if(impl_->parent.accessibility_enabled)impl_->retire_accessibility();}
void NativeWidgetSurface::invalidate(){impl_->dirty=true;if(impl_->parent.wake)web_wake(*impl_->parent.wake);}
void NativeWidgetSurface::accessibility_changed(){impl_->text_dirty=true;if(impl_->parent.accessibility_enabled)impl_->accessibility_dirty=true;invalidate();}
void NativeWidgetSurface::accessibility_value_changed(std::string_view){impl_->text_dirty=true;if(impl_->parent.accessibility_enabled)impl_->accessibility_dirty=true;invalidate();}
void NativeWidgetSurface::set_frame_paced(bool){}
void NativeWidgetSurface::flush(){auto state=impl_;state->flush();}
Size NativeWidgetSurface::size()const{return impl_->parent.size;}
double NativeWidgetSurface::backing_scale()const{return impl_->parent.scale;}
std::string NativeWidgetSurface::resolve_font_family(std::string_view value)const{return FontDatabase::application()->resolve_family(value);}
GraphicsInfo NativeWidgetSurface::info(){return impl_->canvas?impl_->canvas->info():GraphicsInfo{};}
PixelBuffer NativeWidgetSurface::capture(){auto state=impl_;state->flush();if(!state->live||!state->canvas||state->lost)throw std::runtime_error("WebGL2 context is lost or retired");return state->canvas->capture();}
bool NativeWidgetSurface::native_event(const void* raw){auto state=impl_;try{const auto& event=*static_cast<const WebSurfaceEvent*>(raw);if(event.kind==WebSurfaceEventKind::lost){state->lost=true;state->accessibility_dirty=true;state->text_dirty=true;state->retire_text();if(state->parent.accessibility_enabled)state->retire_accessibility();}else if(event.kind==WebSurfaceEventKind::restore){state->reset();state->text_dirty=true;}else if(event.kind==WebSurfaceEventKind::text_input&&event.text)state->text_event(*event.text);else if(event.input){state->callbacks.input(*event.input);if(event.input->kind!=WidgetInputKind::pointer_move&&event.input->kind!=WidgetInputKind::pointer_leave)state->text_dirty=true;}if(state->live){
    if(event.kind==WebSurfaceEventKind::resize||event.kind==WebSurfaceEventKind::restore){state->accessibility_dirty=true;
        state->text_dirty=true;if(!state->parent.visible)state->retire_text();
        if(state->parent.accessibility_enabled&&!state->parent.visible)state->retire_accessibility();}
    state->dirty=true;if(state->parent.wake)web_wake(*state->parent.wake);}return true;}catch(...){if(state->live&&state->callbacks.failure)state->callbacks.failure(std::current_exception());return true;}}
}
