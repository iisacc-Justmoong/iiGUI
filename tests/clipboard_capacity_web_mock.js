// The Web test uses only this in-memory clipboard, never the user's clipboard.
if (typeof globalThis.navigator === "undefined") globalThis.navigator = {};
let iiGuiClipboard = "";
Object.defineProperty(globalThis.navigator, "clipboard", {configurable: true, value: {
    readText: () => Promise.resolve(iiGuiClipboard),
    writeText: text => { iiGuiClipboard = text; return Promise.resolve(); }
}});
