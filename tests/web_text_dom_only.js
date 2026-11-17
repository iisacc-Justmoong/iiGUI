// Reuse the deterministic fake Canvas with OffscreenCanvas deliberately absent.
const iiGUIDomCanvas = globalThis.OffscreenCanvas;
delete globalThis.OffscreenCanvas;
globalThis.document = {createElement(name) {
    if (name !== 'canvas') throw new Error('Unexpected DOM element');
    return new iiGUIDomCanvas(1, 1);
}};
