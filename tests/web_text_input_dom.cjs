const fs = require('fs');
const path = require('path');
const source = fs.readFileSync(path.join(__dirname, '..', 'src', 'Widgets', 'WebTextInputDom.h'), 'utf8');
function bridge(name, args) {
  const match = source.match(new RegExp('/\\* IIGUI_' + name + '_BEGIN \\*/([\\s\\S]*?)/\\* IIGUI_' + name + '_END \\*/'));
  if (!match) throw new Error('Missing ' + name + ' bridge');
  return new Function(...args, 'document', 'Module', 'UTF8ToString', 'lengthBytesUTF8', '_malloc', 'stringToUTF8', '_free', '_iigui_web_text_event', match[1]);
}
const sync = bridge('WEB_TEXT_SYNC', Array.from({length:12}, (_, i) => '$' + i));
const remove = bridge('WEB_TEXT_REMOVE', ['$0', '$1']);
const assert = (value, message) => { if (!value) throw new Error(message); };
const allListeners = new Map();
class Element {
  constructor(tag) { this.tagName=tag;this.style={};this.attrs={};this.listeners={};this.children=[];this.parentNode=null;
    this.value='';this.selectionStart=0;this.selectionEnd=0;this.selectionDirection='none';this.type=''; }
  setAttribute(name, value) { this.attrs[name]=String(value); }
  removeAttribute(name) { delete this.attrs[name]; }
  addEventListener(name, fn) { (this.listeners[name]??=[]).push(fn); }
  focus() { const old=document.activeElement;if(old===this)return;document.activeElement=this;
    if(old)old.dispatch('blur',{relatedTarget:this});this.dispatch('focus',{relatedTarget:old}); }
  remove() { if(this.parentNode){this.parentNode.children=this.parentNode.children.filter(value=>value!==this);this.parentNode=null;} }
  appendChild(child) { child.parentNode=this;this.children.push(child); }
  setSelectionRange(start,end,direction='none') { this.selectionStart=start;this.selectionEnd=end;this.selectionDirection=direction; }
  getBoundingClientRect() { return {left:40,top:30}; }
  dispatch(name, props={}) { const event={...props,preventDefault(){this.defaultPrevented=true;}};
    for(const fn of this.listeners[name]??[])fn(event);return event; }
}
const root = new Element('body'),canvas = new Element('canvas');root.appendChild(canvas);
const document = {activeElement:canvas, body:root, querySelector:s=>s==='#canvas'?canvas:null,
  createElement:tag=>new Element(tag), addEventListener(name,fn){(allListeners.get(name)??allListeners.set(name,[]).get(name)).push(fn);},
  removeEventListener(name,fn){allListeners.set(name,(allListeners.get(name)??[]).filter(value=>value!==fn));},
  dispatch(name){for(const fn of allListeners.get(name)??[])fn({});}};
const Module={},memory=new Map(),events=[];let next=10;
const decode=value=>typeof value==='string'?value:memory.get(value)??'';
const length=value=>Buffer.byteLength(value,'utf8'),alloc=()=>next++,copy=(s,p)=>memory.set(p,s),free=p=>memory.delete(p);
const deliver=(owner,generation,sequence,action,id,text,before,start,removed,selection,selectionLength,flags)=>
  events.push({owner,generation,sequence,action,id:decode(id),text:decode(text),before:decode(before),start,removed,selection,selectionLength,flags});
function publish(id,value,generation=1,ack=0,flags=0,start=0,len=0){
  sync(id,value,generation,99,start,len,flags,15,20,16,ack,'Account',document,Module,decode,length,alloc,copy,free,deliver);
}
publish('account','');const state=Module.iiGUITextInput,el=state.element;
assert(document.activeElement===el && el.type==='text' && el.style.opacity==='0', 'editor bridge did not acquire focus without painting');
el.value='a';el.setSelectionRange(1,1);el.dispatch('input',{inputType:'insertText',data:'a',isComposing:false});
assert(events.length===1 && events[0].action===0 && events[0].text==='a' && events[0].before==='' && events[0].selection===1,
       'ordinary insertion lost its replacement or selection');
publish('account','',1,0);assert(el.value==='a','an unacknowledged browser edit was overwritten');
publish('account','a',1,events.at(-1).sequence,0,1);
el.value='a😀';el.setSelectionRange(3,3);el.dispatch('input',{inputType:'insertText',data:'😀',isComposing:false});
assert(events.at(-1).text==='😀' && events.at(-1).start===1 && events.at(-1).selection===3,'UTF-16 emoji input was split');
publish('account','a😀',1,events.at(-1).sequence,0,3);
el.dispatch('compositionstart');el.dispatch('compositionupdate',{data:'ㅎ'});
assert(events.at(-1).action===1 && events.at(-1).before==='a😀' && (events.at(-1).flags&2),'composition did not start');
el.value='a😀ㅎ';el.setSelectionRange(4,4);el.dispatch('input',{inputType:'insertCompositionText',data:'ㅎ',isComposing:true});
el.dispatch('compositionupdate',{data:'하'});
assert(events.at(-1).action===1 && events.at(-1).text==='하' && events.at(-1).before==='a😀ㅎ','composition update lost its base');
el.value='a😀하';el.setSelectionRange(4,4);el.dispatch('input',{inputType:'insertCompositionText',data:'하',isComposing:true});
el.dispatch('compositionend',{data:'하'});const committed=events.length;
assert(events.at(-1).action===2,'composition did not commit');
el.dispatch('input',{inputType:'insertCompositionText',data:'하',isComposing:false});
assert(events.length===committed,'composition was inserted twice');
publish('account','a😀하',1,events.at(-1).sequence,0,4);
el.setSelectionRange(1,3,'backward');document.dispatch('selectionchange');
assert(events.at(-1).action===4 && events.at(-1).selection===1 && events.at(-1).selectionLength===2 && (events.at(-1).flags&4),
       'backward selection was not delivered');
const tab=el.dispatch('keydown',{key:'Tab',shiftKey:true,isComposing:false});
assert(tab.defaultPrevented && events.at(-1).action===5 && (events.at(-1).flags&8),'Tab was not forwarded');
publish('secret','secret-value',2,0,1,12);
const secret=Module.iiGUITextInput.element;
assert(secret.type==='password' && document.activeElement===secret && el.value==='', 'password transition retained prior text');
const old=events.length;el.value='retired';el.dispatch('input',{inputType:'insertText'});assert(events.length===old,'retired input sent an event');
remove(0,2,document,Module,decode,length,alloc,copy,free,deliver);
assert(!Module.iiGUITextInput && secret.value==='', 'retired input retained its value');
console.log('WEB_TEXT_DOM_OK');
