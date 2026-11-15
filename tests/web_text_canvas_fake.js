// Headless deterministic Canvas boundary for the WebAssembly text test.
globalThis.iiGUIWebTextTestLines = [];
globalThis.OffscreenCanvas = class {
    constructor(width, height) { this.width = width; this.height = height; }
    getContext() {
        const owner = this;
        return {
            measureText: text => ({ width: Array.from(text).length }),
            scale() {}, beginPath() {}, rect() {}, clip() {}, fillRect() {},
            fillText: (text, x, y) => globalThis.iiGUIWebTextTestLines.push([text, y]),
            getImageData: () => ({ data: new Uint8ClampedArray(owner.width * owner.height * 4) })
        };
    }
};
