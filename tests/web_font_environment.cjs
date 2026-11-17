// Execute the actual font-registration body with main-thread and worker APIs.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const source = fs.readFileSync(path.join(__dirname, '../src/Text/FontNativeWeb.cpp'), 'utf8');
const begin = '/* IIGUI_WEB_FONT_BEGIN */', end = '/* IIGUI_WEB_FONT_END */';
const first = source.indexOf(begin), last = source.indexOf(end, first + begin.length);
assert(first >= 0 && last > first, 'font registration implementation is missing');
const AsyncFunction = Object.getPrototypeOf(async function() {}).constructor;
const register = new AsyncFunction('family', 'weight', 'bytes', 'length', 'UTF8ToString', 'HEAPU8',
    'FontFace', 'self', 'document', 'console', source.slice(first + begin.length, last));
class FontFace {
    constructor(family, bytes, options) { this.family = family; this.bytes = bytes; this.options = options; }
    async load() { this.loaded = true; return this; }
}
async function main() {
    const heap = Uint8Array.from([9, 7, 8, 6]);
    const call = (worker, document) => register(1, 500, 1, 2, () => 'Example', heap,
        FontFace, worker, document, {error() {}});
    for (const workerMode of [false, true]) {
        const faces = [];
        const fontSet = {add: face => faces.push(face)};
        assert.equal(await call(workerMode ? {fonts: fontSet} : {}, workerMode ? undefined : {fonts: fontSet}), 1);
        assert.equal(faces.length, 1);
        assert.equal(faces[0].family, 'Example');
        assert.equal(faces[0].options.weight, '500');
        assert.deepEqual([...faces[0].bytes], [7, 8]);
        assert.equal(faces[0].loaded, true);
    }
    assert.equal(await call({}, undefined), 0, 'missing font set is reported without throwing');
}
main().catch(error => { console.error(error); process.exitCode = 1; });
