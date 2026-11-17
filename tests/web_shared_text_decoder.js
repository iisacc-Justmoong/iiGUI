// Chrome's TextDecoder rejects views backed by SharedArrayBuffer. Node permits
// them, so impose the browser boundary while testing the actual shared heap.
const iiGUIOriginalDecode = TextDecoder.prototype.decode;
TextDecoder.prototype.decode = function(input, options) {
    if (input && typeof SharedArrayBuffer !== 'undefined' && input.buffer instanceof SharedArrayBuffer)
        throw new TypeError('TextDecoder does not accept shared buffer views');
    return iiGUIOriginalDecode.call(this, input, options);
};
