#pragma once
#include <iiGUI/NativeWidgets.h>
#include <emscripten.h>
#include <cstdint>
#include <string>

namespace ii::gui::detail {
// The element is only an input-method bridge. iiGUI paints and owns the text.
inline void web_text_publish(void* owner,std::uint32_t generation,std::uint32_t acknowledged,
                             const WidgetSnapshot& editor,const std::string& text) {
    const int flags=(editor.password?1:0)|(editor.multiline?2:0)|(editor.read_only?4:0)|(editor.text_selection_reversed?8:0);
    MAIN_THREAD_EM_ASM({
        /* IIGUI_WEB_TEXT_SYNC_BEGIN */
        (function(){
            const id=UTF8ToString($0),value=UTF8ToString($1),generation=$2>>>0,owner=$3;
            const selection=$4>>>0,selectionLength=$5>>>0,flags=$6|0,ack=$10>>>0;
            const canvas=document.querySelector('#canvas');if(!canvas)return;
            let state=Module['iiGUITextInput'];
            if((flags&3)===3||value.indexOf('\r')!==-1||(!(flags&2)&&value.indexOf('\n')!==-1)){
                if(state){state.dispose();delete Module['iiGUITextInput'];}return;
            }
            if(!state){
                state={active:true,element:null,generation:0,sent:0,shadow:'',composing:false,transfer:false,lastSelection:''};
                state.send=(action,text,before,start,removed,selected,selectedLength,eventFlags)=>{
                    if(!state.active||!state.element||state.sent===0xffffffff)return false;
                    if([state.id,text,before].some(item=>item.indexOf('\0')!==-1||lengthBytesUTF8(item)>65536)){
                        if(action!==9)state.send(9,'','',0,0,0,0,0);return false;
                    }
                    const sizes=[state.id,text,before].map(item=>lengthBytesUTF8(item)+1),ptrs=[];
                    try{
                        for(const size of sizes){const ptr=_malloc(size);if(!ptr)return false;ptrs.push(ptr);}
                        stringToUTF8(state.id,ptrs[0],sizes[0]);stringToUTF8(text,ptrs[1],sizes[1]);stringToUTF8(before,ptrs[2],sizes[2]);
                        const sequence=++state.sent;
                        _iigui_web_text_event(state.owner,state.generation,sequence,action,ptrs[0],ptrs[1],ptrs[2],
                                              start>>>0,removed>>>0,selected>>>0,selectedLength>>>0,eventFlags|0);
                        return true;
                    } finally { for(const ptr of ptrs)_free(ptr); }
                };
                state.selection=()=>{
                    const el=state.element;if(!state.active||!el||document.activeElement!==el||el.value!==state.shadow)return;
                    const start=el.selectionStart||0,end=el.selectionEnd||0,reverse=el.selectionDirection==='backward'&&end>start;
                    const signature=start+':'+end+':'+reverse;if(signature===state.lastSelection)return;
                    state.lastSelection=signature;state.send(4,'',state.shadow,0,0,start,end-start,1|(reverse?4:0));
                };
                document.addEventListener('selectionchange',state.selection);
                state.dispose=()=>{
                    if(!state.active)return;state.active=false;document.removeEventListener('selectionchange',state.selection);
                    const el=state.element;state.element=null;state.shadow='';state.original='';state.candidate='';
                    if(el){el.value='';if(document.activeElement===el){state.transfer=true;canvas.focus({preventScroll:true});state.transfer=false;}el.remove();}
                };
                Module['iiGUITextInput']=state;
            }
            const password=!!(flags&1),multiline=!!(flags&2),readOnly=!!(flags&4);
            const changed=state.generation!==generation,kind=multiline?'textarea':password?'password':'text';
            if(changed||state.kind!==kind){
                const old=state.element,hadFocus=old&&document.activeElement===old;
                state.generation=generation;state.owner=owner;state.id=id;state.sent=0;state.composing=false;
                state.shadow='';state.original='';state.candidate='';state.lastSelection='';
                if(!old||state.kind!==kind){
                    const el=document.createElement(multiline?'textarea':'input');
                    if(!multiline)el.type=password?'password':'text';el.tabIndex=-1;
                    el.style.position='fixed';el.style.opacity='0';el.style.pointerEvents='none';
                    el.style.width='1px';el.style.margin='0';el.style.padding='0';el.style.border='0';el.style.zIndex='-1';
                    el.addEventListener('input',event=>{
                        if(!state.active||state.element!==el)return;
                        if(state.composing||event.isComposing)return;
                        const before=state.shadow,after=el.value;if(before===after)return;
                        let first=0,oldEnd=before.length,newEnd=after.length;
                        while(first<oldEnd&&first<newEnd&&before.charCodeAt(first)===after.charCodeAt(first))++first;
                        const high=n=>n>=0xd800&&n<=0xdbff,low=n=>n>=0xdc00&&n<=0xdfff;
                        if(first>0&&((first<oldEnd&&high(before.charCodeAt(first-1))&&low(before.charCodeAt(first)))||
                                     (first<newEnd&&high(after.charCodeAt(first-1))&&low(after.charCodeAt(first)))))--first;
                        while(oldEnd>first&&newEnd>first&&before.charCodeAt(oldEnd-1)===after.charCodeAt(newEnd-1)){--oldEnd;--newEnd;}
                        if((oldEnd>first&&oldEnd<before.length&&high(before.charCodeAt(oldEnd-1))&&low(before.charCodeAt(oldEnd)))||
                           (newEnd>first&&newEnd<after.length&&high(after.charCodeAt(newEnd-1))&&low(after.charCodeAt(newEnd)))){++oldEnd;++newEnd;}
                        const start=el.selectionStart||0,end=el.selectionEnd||0,reverse=el.selectionDirection==='backward'&&end>start;
                        state.send(0,after.slice(first,newEnd),before,first,oldEnd-first,start,end-start,1|(reverse?4:0));
                        state.shadow=after;state.lastSelection=start+':'+end+':'+reverse;
                    });
                    el.addEventListener('compositionstart',()=>{
                        if(!state.active||state.element!==el||el.readOnly)return;
                        state.composing=true;state.original=state.shadow;state.start=el.selectionStart||0;
                        state.removed=(el.selectionEnd||0)-state.start;state.candidate='';state.first=true;
                    });
                    el.addEventListener('compositionupdate',event=>{
                        if(!state.active||state.element!==el||!state.composing)return;
                        const candidate=event.data||'';
                        if(state.send(1,candidate,state.shadow,state.start,state.removed,candidate.length,0,1|(state.first?2:0))){
                            state.shadow=state.original.slice(0,state.start)+candidate+state.original.slice(state.start+state.removed);
                            state.first=false;state.candidate=candidate;
                        }
                    });
                    el.addEventListener('compositionend',event=>{
                        if(!state.active||state.element!==el||!state.composing)return;
                        const candidate=event.data||'';
                        if(!candidate)state.send(3,'',state.shadow,0,0,0,0,1);
                        else{
                            if(candidate!==state.candidate||state.first){
                                if(!state.send(1,candidate,state.shadow,state.start,state.removed,candidate.length,0,1|(state.first?2:0))){
                                    if(!state.first)state.send(3,'',state.shadow,0,0,0,0,1);
                                    state.composing=false;state.original='';state.candidate='';return;
                                }
                                state.shadow=state.original.slice(0,state.start)+candidate+state.original.slice(state.start+state.removed);
                            }
                            state.send(2,'',state.shadow,0,0,0,0,1);
                        }
                        state.shadow=candidate?state.original.slice(0,state.start)+candidate+state.original.slice(state.start+state.removed):state.original;
                        state.composing=false;state.original='';state.candidate='';
                        const start=el.selectionStart||0,end=el.selectionEnd||0;state.lastSelection=start+':'+end+':'+(el.selectionDirection==='backward'&&end>start);
                    });
                    el.addEventListener('keydown',event=>{
                        if(!state.active||state.element!==el||state.composing||event.isComposing)return;
                        let action=-1;if(event.key==='Tab')action=5;else if(event.key==='Enter'&&!multiline)action=6;else if(event.key==='Escape')action=7;
                        if(action>=0){event.preventDefault();state.send(action,'','',0,0,0,0,event.shiftKey?8:0);}
                    });
                    el.addEventListener('blur',event=>{
                        if(!state.active||state.element!==el||state.transfer||event.relatedTarget===canvas||event.relatedTarget===state.element)return;
                        state.send(8,'','',0,0,0,0,0);
                    });
                    document.body.appendChild(el);state.element=el;state.kind=kind;
                    if(old){old.value='';old.remove();}
                }
                state.restoreFocus=!!hadFocus;
            }
            state.id=id;state.owner=owner;const el=state.element;el.readOnly=readOnly;
            el.setAttribute('aria-label',UTF8ToString($11)||'Text input');
            const rect=canvas.getBoundingClientRect();el.style.left=(rect.left+$7)+'px';el.style.top=(rect.top+$8)+'px';el.style.height=Math.max(1,$9)+'px';
            if(state.sent===ack){
                if(el.value!==value)el.value=value;state.shadow=value;
                const end=selection+selectionLength,reverse=!!(flags&8)&&selectionLength>0;
                state.lastSelection=selection+':'+end+':'+reverse;
                if(el.selectionStart!==selection||el.selectionEnd!==end||((el.selectionDirection==='backward')!==reverse&&selectionLength))
                    el.setSelectionRange(selection,end,reverse?'backward':'forward');
            }
            if(state.restoreFocus||document.activeElement===canvas){
                state.restoreFocus=false;state.transfer=true;el.focus({preventScroll:true});state.transfer=false;
            }
        })();
        /* IIGUI_WEB_TEXT_SYNC_END */
    },editor.id.c_str(),text.c_str(),generation,owner,editor.text_selection.location,editor.text_selection.length,
      flags,editor.caret_bounds.x,editor.caret_bounds.y,editor.caret_bounds.height,acknowledged,editor.accessibility_name.c_str());
}
inline void web_text_remove(std::uint32_t generation) {
    MAIN_THREAD_EM_ASM({
        /* IIGUI_WEB_TEXT_REMOVE_BEGIN */
        const state=Module['iiGUITextInput'];if(state&&state.generation===($1>>>0)){state.dispose();delete Module['iiGUITextInput'];}
        /* IIGUI_WEB_TEXT_REMOVE_END */
    },0,generation);
}
}
