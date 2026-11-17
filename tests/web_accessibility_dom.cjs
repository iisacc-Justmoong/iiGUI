const fs = require('fs');
const path = require('path');
const source = fs.readFileSync(path.join(__dirname, '..', 'src', 'Widgets', 'NativeWidgetsWeb.cpp'), 'utf8');
function body(name) {
  const match = source.match(new RegExp('/\\* IIGUI_' + name + '_BEGIN \\*/([\\s\\S]*?)/\\* IIGUI_' + name + '_END \\*/'));
  if (!match) throw new Error('Missing ' + name + ' bridge');
  return new Function('$0', '$1', '$2', 'document', 'Module', 'UTF8ToString',
                      'lengthBytesUTF8', '_malloc', 'stringToUTF8', '_free', '_iigui_web_accessibility_event', match[1]);
}
const publish = body('WEB_ACCESSIBILITY_PUBLISH');
const retire = body('WEB_ACCESSIBILITY_REMOVE');
const assert = (condition, message) => { if (!condition) throw new Error(message); };
class Element {
  constructor() { this.attributes = {}; this.listeners = {}; this.style = {}; this.children = []; this.parentNode = null; this.textContent = ''; }
  setAttribute(name, value) { this.attributes[name] = String(value); }
  hasAttribute(name) { return Object.hasOwn(this.attributes, name); }
  removeAttribute(name) { delete this.attributes[name]; }
  addEventListener(name, callback) { this.listeners[name] = callback; }
  get firstChild() { return this.children[0] || null; }
  get nextSibling() { if (!this.parentNode) return null; const p = this.parentNode; return p.children[p.children.indexOf(this) + 1] || null; }
  appendChild(child) { if (child.parentNode) child.remove(); this.children.push(child); child.parentNode = this; }
  insertBefore(child, before) { if (child.parentNode) child.remove(); const i = before ? this.children.indexOf(before) : -1;
    this.children.splice(i < 0 ? this.children.length : i, 0, child);child.parentNode = this; }
  contains(child) { return !!child && (child === this || this.children.some(value => value.contains(child))); }
  focus() { document.activeElement = this;this.dispatch('focus'); }
  remove() { if (!this.parentNode) return; const p = this.parentNode; p.children.splice(p.children.indexOf(this), 1); this.parentNode = null; }
  dispatch(type, key = '') { const event = {key, repeat:false, preventDefault() { this.prevented = true; }}; this.listeners[type]?.(event); return event; }
}
const bodyElement = new Element(), canvas = new Element();bodyElement.appendChild(canvas);
canvas.setAttribute('data-iigui-accessibility', '');
const document = {querySelector(selector) { assert(selector === '#canvas', 'wrong canvas'); return canvas; }, createElement() { return new Element(); }};
const runtime = {}, deliveries = [];let allocated = '';
const decode = (value) => value, length = (value) => value.length, alloc = () => 1;
const copy = (value) => { allocated = value; }, free = () => {};
const deliver = (parent, pointer, generation, action) => { deliveries.push([parent, allocated, generation, action]); };
function run(method, nodes, generation = 1) {
  method(JSON.stringify(nodes), generation, 123, document, runtime, decode, length, alloc, copy, free, deliver);
}
const text = {id:'label',role:'text',name:'<img onerror=x>',enabled:true,checked:false,actionable:false};
const button = {id:'button',role:'button',name:'Add',enabled:true,checked:false,actionable:true,bounds:[17,31,80,44]};
const checkbox = {id:'box',role:'checkbox',name:'Choice',enabled:true,checked:true,actionable:true};
const radio = {id:'radio',role:'radio',name:'A',enabled:true,checked:true,actionable:true};
const slider = {id:'slider',role:'slider',name:'Size',enabled:true,checked:false,actionable:true,minimum:0,maximum:10,value:5};
run(publish, [text,button,checkbox,radio,slider]);
const state = runtime.iiGUIAccessibility, peer = state.peers.get('button'), box = state.peers.get('box');
assert(state.root.parentNode && state.peers.get('label').element.textContent === '<img onerror=x>', 'text was not safely projected');
assert(peer.element.attributes.role === 'button' && peer.element.attributes.tabindex === '0', 'button semantics missing');
assert(peer.element.attributes['data-iigui-id'] === 'button' && peer.element.attributes['data-iigui-bounds'] === '17,31,80,44', 'action geometry missing');
assert(state.focusById('button') && deliveries.at(-1)[3]===0, 'host focus did not reach iiGUI');
assert(!state.focusById('not-a-peer'), 'unknown host focus was accepted');
assert(state.blur() && deliveries.at(-1)[3]===8, 'host blur did not reach iiGUI');
deliveries.length = 0;
assert(box.element.attributes['aria-checked'] === 'true', 'checkbox state missing');
const sliderPeer = state.peers.get('slider').element;
assert(sliderPeer.attributes['aria-valuemin'] === '0' && sliderPeer.attributes['aria-valuemax'] === '10' &&
       sliderPeer.attributes['aria-valuenow'] === '5', 'slider range semantics missing');
const home = sliderPeer.dispatch('keydown', 'Home');
assert(home.prevented && deliveries.length === 1 && deliveries[0][3] === 6, 'slider Home did not reach iiGUI');
deliveries.length = 0;
const arrow = state.peers.get('radio').element.dispatch('keydown', 'ArrowDown');
assert(arrow.prevented && deliveries.length === 1 && deliveries[0][3] === 3, 'radio arrow did not reach its group');
deliveries.length = 0;
peer.element.dispatch('focus');peer.element.dispatch('click');
assert(deliveries.length === 2 && deliveries[0][3] === 0 && deliveries[1][3] === 1, 'focus or activation was not sent');
const keydown = peer.element.dispatch('keydown', ' ');peer.element.dispatch('keyup', ' ');
assert(keydown.prevented && deliveries.length === 3 && deliveries[2][3] === 1, 'space activation was not handled');
run(publish, [text,{...button,enabled:false,actionable:false},{...checkbox,checked:false}]);
assert(peer.element.attributes['aria-disabled'] === 'true' && peer.element.attributes.tabindex === '-1', 'disabled peer is focusable');
assert(!state.focusById('button'), 'disabled peer accepted host focus');
assert(box.element.attributes['aria-checked'] === 'false', 'checkbox state did not update');
peer.element.dispatch('click');assert(deliveries.length === 3, 'disabled peer activated');
run(publish, [checkbox,text,{...button,enabled:false,actionable:false}]);
assert(state.root.children[0] === box.element && state.root.children[2] === peer.element, 'peer order did not follow the view');
run(publish, [text,checkbox]);peer.element.dispatch('click');
assert(deliveries.length === 3 && !peer.element.parentNode, 'retired peer sent an action');
run(publish, [button], 2);box.element.dispatch('click');
assert(deliveries.length === 3 && !state.root.parentNode, 'replaced surface remained active');
run(retire, [], 2);runtime.iiGUIAccessibility?.peers.get('button')?.element.dispatch('click');
assert(!runtime.iiGUIAccessibility && deliveries.length === 3, 'removed surface remained active');
run(publish, [button,radio], 4);
const nextState = runtime.iiGUIAccessibility, nextButton = nextState.peers.get('button').element, nextRadio = nextState.peers.get('radio').element;
nextButton.focus();
run(publish, [{...button,focused:false},{...radio,focused:true}], 4);
assert(document.activeElement === nextButton, 'stale view update replaced a pending browser focus');
run(publish, [{...button,focused:true},{...radio,focused:false}], 4);
run(publish, [{...button,focused:false},{...radio,focused:true}], 4);
assert(document.activeElement === nextRadio, 'view-model focus did not follow radio navigation');
run(retire, [], 4);
canvas.removeAttribute('data-iigui-accessibility');run(publish, [button], 3);
assert(!runtime.iiGUIAccessibility, 'embedding page without opt-in gained accessibility peers');
