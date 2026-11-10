const fs = require('fs');
const path = require('path');
const source = fs.readFileSync(path.join(__dirname, '..', 'src', 'Platform', 'HostWeb.cpp'), 'utf8');
const body = name => {
  const match = source.match(new RegExp('/\\* IIGUI_' + name + '_BEGIN \\*/([\\s\\S]*?)/\\* IIGUI_' + name + '_END \\*/'));
  if (!match) throw new Error('Missing ' + name + ' browser observer');
  return new Function('window', 'Module', 'Event', match[1]);
};
const install = body('DPI_WATCH');
const retire = body('DPI_UNWATCH');
const assert = (v, message) => { if (!v) throw new Error(message); };
for (const legacy of [false, true]) {
  const queries = [], events = [], module = {};
  const window = {
    devicePixelRatio: 1,
    dispatchEvent(event) { events.push(event.type); },
    matchMedia(text) {
      const query = {text, listener: null};
      if (legacy) {
        query.addListener = fn => { query.listener = fn; };
        query.removeListener = fn => { if (query.listener === fn) query.listener = null; };
      } else {
        query.addEventListener = (name, fn) => { assert(name === 'change', 'wrong event'); query.listener = fn; };
        query.removeEventListener = (name, fn) => { if (name === 'change' && query.listener === fn) query.listener = null; };
      }
      queries.push(query);return query;
    }
  };
  const Event = function(type) { this.type = type; };
  install(window, module, Event);
  assert(queries.length === 1 && queries[0].text === '(resolution: 1dppx)', 'initial DPR not observed');
  const stale = queries[0].listener;
  window.devicePixelRatio = 1.25;stale();
  assert(queries[0].listener === null && queries[1].text === '(resolution: 1.25dppx)', 'DPR listener not replaced');
  assert(events.length === 1 && events[0] === 'resize', 'DPR transition not delivered');
  stale();assert(events.length === 1, 'retired listener delivered another event');
  window.devicePixelRatio = 2;queries[1].listener();
  assert(queries[2].text === '(resolution: 2dppx)' && events.length === 2, 'second DPR transition lost');
  const after = queries[2].listener;retire(window, module, Event);after();
  assert(queries[2].listener === null && events.length === 2 && !module.iiGUIDpiWatch, 'listener survived shutdown');
}

{
  let notices = 0, listener;
  const window = {devicePixelRatio: 1, dispatchEvent(){++notices;},
    matchMedia(){return {addEventListener(_, fn){listener=fn;},removeEventListener(){}};}};
  const module = {}, Event = function(type){this.type=type;};
  install(window,module,Event);const first=listener;install(window,module,Event);first();
  assert(notices===0, 'replaced observer was not retired');
  retire(window,module,Event);
  const noMedia={devicePixelRatio:1};install(noMedia,module,Event);retire(noMedia,module,Event);
}
