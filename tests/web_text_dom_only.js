// Reuse the deterministic fake Canvas with OffscreenCanvas deliberately absent.
const iiGUIDomCanvas = globalThis.OffscreenCanvas;
delete globalThis.OffscreenCanvas;
globalThis.document = {
    body: { append() {} },
    createElement(name) {
        if (name === 'canvas') return new iiGUIDomCanvas(1, 1);
        return {
            style: {}, setAttribute() {}, remove() {},
            append(node) { this.child = node; },
            getBoundingClientRect() { return { left: 0, width: this.child.text.length }; }
        };
    },
    createTextNode(text) { return { text }; },
    createRange() {
        return {
            setStart(node, offset) { this.first = offset; },
            setEnd(node, offset) { this.last = offset; },
            getBoundingClientRect() { return { left: this.first }; },
            getClientRects() { return [{ left: this.first, width: this.last - this.first }]; }
        };
    }
};
