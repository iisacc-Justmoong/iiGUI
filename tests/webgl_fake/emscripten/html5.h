#pragma once
using EMSCRIPTEN_WEBGL_CONTEXT_HANDLE = int;
using EMSCRIPTEN_RESULT = int;
inline constexpr EMSCRIPTEN_RESULT EMSCRIPTEN_RESULT_SUCCESS = 0;
struct EmscriptenWebGLContextAttributes {
    int majorVersion=1,minorVersion=0;
    bool alpha=false,antialias=false,depth=false,stencil=false,premultipliedAlpha=false,preserveDrawingBuffer=false;
};
extern "C" {
void emscripten_webgl_init_context_attributes(EmscriptenWebGLContextAttributes*);
EMSCRIPTEN_WEBGL_CONTEXT_HANDLE emscripten_webgl_create_context(const char*,const EmscriptenWebGLContextAttributes*);
EMSCRIPTEN_RESULT emscripten_webgl_make_context_current(EMSCRIPTEN_WEBGL_CONTEXT_HANDLE);
int emscripten_webgl_enable_extension(EMSCRIPTEN_WEBGL_CONTEXT_HANDLE,const char*);
EMSCRIPTEN_RESULT emscripten_webgl_destroy_context(EMSCRIPTEN_WEBGL_CONTEXT_HANDLE);
EMSCRIPTEN_RESULT emscripten_set_canvas_element_size(const char*,int,int);
}
