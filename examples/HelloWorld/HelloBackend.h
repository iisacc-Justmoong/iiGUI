#pragma once
#include <functional>

// Ordinary C++ object: no registration, LVUI base type or export annotation.
// One instance belongs to each generated mount, including repeated clicks.
struct HelloBackend final {
    inline static int created = 0, destroyed = 0, greetings = 0;
    std::function<void()> on_greet, on_close;
    HelloBackend() { ++created; }
    ~HelloBackend() { ++destroyed; }
    void greet() { ++greetings; if (on_greet) on_greet(); }
    void close() { if (on_close) on_close(); }
};
