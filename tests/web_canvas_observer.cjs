const fs = require('fs');
const path = require('path');
const source = fs.readFileSync(path.join(__dirname, '..', 'src', 'Platform', 'HostWeb.cpp'), 'utf8');
function body(name) {
  const match = source.match(new RegExp('/\\* IIGUI_' + name + '_BEGIN \\*/([\\s\\S]*?)/\\* IIGUI_' + name + '_END \\*/'));
  if (!match) throw new Error('Missing ' + name + ' observer');
  return new Function('window', 'document', 'Module', 'Event', match[1]);
}
const install = body('CANVAS_WATCH');
const retire = body('CANVAS_UNWATCH');
const assert = (value, message) => { if (!value) throw new Error(message); };
const events = [], observers = [], canvas = {};
const window = {
  dispatchEvent(event) { events.push(event.type); },
  ResizeObserver: class {
    constructor(callback) { this.callback = callback; this.disconnected = false; observers.push(this); }
    observe(target) { this.target = target; }
    disconnect() { this.disconnected = true; }
  }
};
const document = {querySelector(selector) { assert(selector === '#canvas', 'wrong canvas'); return canvas; }};
const runtime = {}, Event = function(type) { this.type = type; };
install(window, document, runtime, Event);
assert(observers.length === 1 && observers[0].target === canvas, 'canvas was not observed');
observers[0].callback([{target: canvas}]);
assert(events.length === 1 && events[0] === 'resize', 'element resize was not delivered');
const stale = observers[0].callback;
install(window, document, runtime, Event);
assert(observers[0].disconnected && observers[1].target === canvas, 'replacement did not retire the observer');
stale([{target: canvas}]);
assert(events.length === 1, 'replaced observer still delivered events');
retire(window, document, runtime, Event);
observers[1].callback([{target: canvas}]);
assert(observers[1].disconnected && events.length === 1 && !runtime.iiGUICanvasWatch, 'retired observer still delivered events');
install({dispatchEvent() {}}, document, runtime, Event);
retire(window, document, runtime, Event);
