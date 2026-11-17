const fs = require('fs');
const path = require('path');
const source = fs.readFileSync(path.join(__dirname, '..', 'src', 'Platform', 'HostWeb.cpp'), 'utf8');
function body(name) {
  const match = source.match(new RegExp('/\\* IIGUI_' + name + '_BEGIN \\*/([\\s\\S]*?)/\\* IIGUI_' + name + '_END \\*/'));
  if (!match) throw new Error('Missing ' + name + ' bridge');
  return new Function('$0', '$1', 'window', 'document', 'Module', match[1]);
}
const install = body('CANVAS_MINIMUM_INSTALL');
const update = body('CANVAS_MINIMUM_UPDATE');
const remove = body('CANVAS_MINIMUM_REMOVE');
const assert = (value, message) => { if (!value) throw new Error(message); };

function canvas(optedIn) {
  const values = new Map([['min-width', ['40px', 'important']], ['min-height', ['12px', '']]]);
  return {
    hasAttribute(name) { return optedIn && name === 'data-iigui-respect-minimum'; },
    style: {
      getPropertyValue(name) { return values.get(name)?.[0] || ''; },
      getPropertyPriority(name) { return values.get(name)?.[1] || ''; },
      setProperty(name, value, priority = '') { values.set(name, [value, priority]); },
      removeProperty(name) { values.delete(name); }
    }
  };
}
let target = canvas(false);
const document = {querySelector(selector) { assert(selector === '#canvas', 'wrong canvas'); return target; }};
const window = {
  getComputedStyle(element) { return {minWidth: element.style.getPropertyValue('min-width') || '0px',
                                      minHeight: element.style.getPropertyValue('min-height') || '0px'}; },
  CSS: {supports(property, value) { return !value.includes('min-content'); }}
};
const runtime = {};
install(600, 360, window, document, runtime);
update(700, 400, window, document, runtime);
remove(0, 0, window, document, runtime);
assert(target.style.getPropertyValue('min-width') === '40px', 'non-opted canvas changed');

target = canvas(true);
install(600, 360, window, document, runtime);
assert(target.style.getPropertyValue('min-width') === 'max(40px, 600px)', 'minimum width not applied');
assert(target.style.getPropertyPriority('min-width') === 'important', 'inline priority lost');
assert(target.style.getPropertyValue('min-height') === 'max(12px, 360px)', 'minimum height not applied');
update(300, 250, window, document, runtime);
assert(target.style.getPropertyValue('min-width') === 'max(40px, 300px)', 'updated width not applied');
assert(target.style.getPropertyValue('min-height') === 'max(12px, 250px)', 'updated height not applied');
install(500, 200, window, document, runtime);
assert(target.style.getPropertyValue('min-width') === 'max(40px, 500px)', 'replacement captured an old host minimum');
remove(0, 0, window, document, runtime);
assert(target.style.getPropertyValue('min-width') === '40px' &&
       target.style.getPropertyPriority('min-width') === 'important' &&
       target.style.getPropertyValue('min-height') === '12px' && !runtime.iiGUICanvasMinimum,
       'original canvas minima were not restored');

target = canvas(true);
install(600, 360, window, document, runtime);
target.style.setProperty('min-width', '80px');
update(700, 400, window, document, runtime);
remove(0, 0, window, document, runtime);
assert(target.style.getPropertyValue('min-width') === '80px', 'host overwrote a later page style');
assert(target.style.getPropertyValue('min-height') === '12px', 'unchanged dimension was not restored');

target = canvas(true);
target.style.setProperty('min-width', 'min-content');
install(600, 360, window, document, runtime);
assert(target.style.getPropertyValue('min-width') === 'min-content', 'unsupported CSS minimum was destroyed');
remove(0, 0, window, document, runtime);
