// Exercises the actual unthreaded host wait body with deterministic browser events.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const source = fs.readFileSync(path.join(__dirname, '../src/Platform/HostWeb.cpp'), 'utf8');
function between(begin, end) {
    const first = source.indexOf(begin), last = source.indexOf(end, first + begin.length);
    assert(first >= 0 && last > first, `missing host implementation: ${begin}`);
    return source.slice(first + begin.length, last);
}
const AsyncFunction = Object.getPrototypeOf(async function() {}).constructor;
const wait = new AsyncFunction('signal', 'expected', 'timeout', 'frame', 'Module', 'HEAP32',
    'requestAnimationFrame', 'cancelAnimationFrame', 'MessageChannel', 'setTimeout', 'clearTimeout',
    between('/* IIGUI_SINGLE_WAIT_BEGIN */', '/* IIGUI_SINGLE_WAIT_END */'));
const wake = new Function('Module', between('/* IIGUI_SINGLE_WAKE_BEGIN */', '/* IIGUI_SINGLE_WAKE_END */'));

async function main() {
    const module = {}, heap = new Int32Array(4), frames = new Map(), timers = new Map();
    let next = 1, issuedFrames = 0;
    const raf = fn => { const id = next++; frames.set(id, fn); ++issuedFrames; return id; };
    const cancelRaf = id => frames.delete(id);
    const setTimer = (fn, ms) => { const id = next++; timers.set(id, {fn, ms}); return id; };
    const clearTimer = id => timers.delete(id);
    class Channel {
        constructor() {
            this.port1 = {onmessage: null, close() {}};
            this.port2 = {postMessage: data => setImmediate(() => this.port1.onmessage?.({data})), close() {}};
        }
    }
    const start = (expected, timeout, frame) => wait(0, expected, timeout, frame, module, heap,
        raf, cancelRaf, Channel, setTimer, clearTimer);
    const notify = () => { ++heap[0]; wake(module); };

    let completed = false;
    const idle = start(0, -1, 0).then(value => { completed = true; return value; });
    await new Promise(resolve => setImmediate(resolve));
    assert.equal(completed, false, 'idle waits for an actual event');
    assert.equal(timers.size, 0, 'idle must not start a periodic timer');
    assert.equal(frames.size, 0, 'idle must not schedule display frames');
    notify(); assert.equal(await idle, -1);

    const withFrame = start(heap[0], -1, 1);
    assert.equal(frames.size, 1); notify(); assert.equal(await withFrame, -1);
    const nextFrame = start(heap[0], -1, 1);
    assert.equal(issuedFrames, 1, 'an unrelated wake must preserve the outstanding frame');
    const [frameId, frameCallback] = frames.entries().next().value;
    frames.delete(frameId); frameCallback(101.25);
    assert.equal(await nextFrame, 101.25);

    const deadline = start(heap[0], 25, 0);
    assert.equal(timers.size, 1); const [timerId, timer] = timers.entries().next().value;
    assert.equal(timer.ms, 25); timers.delete(timerId); timer.fn(); assert.equal(await deadline, -1);

    const changed = start(heap[0] - 1, -1, 0);
    assert.equal(await changed, -1, 'a wake before waiting is not lost');
    const superseded = start(heap[0] - 1, -1, 0);
    notify(); assert.equal(await superseded, -1);
    let staleWake = false;
    const subsequent = start(heap[0], -1, 0).then(value => { staleWake = true; return value; });
    await new Promise(resolve => setImmediate(resolve));
    assert.equal(staleWake, false, 'an old queued wake must not wake the next wait');
    notify(); assert.equal(await subsequent, -1);
    const cancellable = start(heap[0], 100, 1);
    assert.equal(frames.size, 1); assert.equal(timers.size, 1);
    notify(); assert.equal(await cancellable, -1); assert.equal(timers.size, 0);
    const disabled = start(heap[0] - 1, -1, 0);
    assert.equal(await disabled, -1); assert.equal(frames.size, 0, 'disabled frame source is cancelled');
}
main().catch(error => { console.error(error); process.exitCode = 1; });
