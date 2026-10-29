#pragma once
#include <emscripten.h>

// Promise completion can rewind all the way through the application entry
// point. Emscripten uses ExitStatus/'unwind' there; dispatch through its user
// callback boundary just like emscripten_sleep, rather than leaking a normal
// thread exit as an unhandled Promise rejection. Other exceptions still fail.
// Keep these import names in CMake's ASYNCIFY_IMPORTS when adding an operation.
#define IIGUI_WEB_ASYNC_JS(ret, name, params, ...) \
    EM_JS(ret, name, params, { \
        return Asyncify.handleSleep(wakeUp => { \
            (async () => __VA_ARGS__)().then( \
                value => callUserCallback(() => wakeUp(value)), \
                error => callUserCallback(() => { throw error; })); \
        }); \
    })
