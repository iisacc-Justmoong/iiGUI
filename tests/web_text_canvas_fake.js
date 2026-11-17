// Headless deterministic Canvas boundary for the WebAssembly text test.
globalThis.iiGUIWebTextTestLines = [];
globalThis.iiGUIWebTextTestFonts = [];
globalThis.iiGUIWebTextTestUnderlines = [];
globalThis.OffscreenCanvas = class {
    constructor(width, height) { this.width = width; this.height = height; }
    getContext() {
        const owner = this;
        let letterSpacing = '0px';
        let fontSize = 10;
        const context = {
            set font(value) { globalThis.iiGUIWebTextTestFonts.push(value); fontSize = Number(value.match(/([0-9.]+)px/)?.[1] ?? 10); },
            set letterSpacing(value) { letterSpacing = value; },
            get letterSpacing() { return letterSpacing; },
            measureText: text => ({ width: Math.max(0, Array.from(text).length +
                Math.max(0, Array.from(text).length - 1) * parseFloat(letterSpacing)),
                ...(globalThis.iiGUINoFontMetrics ? {} : { fontBoundingBoxAscent: fontSize * .8, fontBoundingBoxDescent: fontSize * .2 }) }),
            scale() {}, beginPath() {}, rect() {}, clip() {},
            fillRect: (x, y) => globalThis.iiGUIWebTextTestUnderlines.push(y),
            fillText: (text, x, y) => globalThis.iiGUIWebTextTestLines.push([text, y, letterSpacing, context.textBaseline]),
            getImageData: () => ({ data: new Uint8ClampedArray(owner.width * owner.height * 4) })
        };
        if (globalThis.iiGUINoLetterSpacing) delete context.letterSpacing;
        return context;
    }
};
