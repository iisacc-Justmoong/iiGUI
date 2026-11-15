#pragma once

struct ANativeActivity {};
struct android_app {
    ANativeActivity* activity;
    int destroyRequested;
};

extern "C" void ANativeActivity_finish(ANativeActivity* activity);
extern "C" void android_main(android_app* app);
