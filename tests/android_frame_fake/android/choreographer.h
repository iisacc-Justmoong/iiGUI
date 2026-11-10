#pragma once

#include <stdint.h>

struct AChoreographer {};
using AChoreographer_frameCallback = void (*)(long, void*);

void AChoreographer_postFrameCallback(AChoreographer*, AChoreographer_frameCallback, void*);
#if __ANDROID_API__ >= 29
using AChoreographer_frameCallback64 = void (*)(int64_t, void*);
void AChoreographer_postFrameCallback64(AChoreographer*, AChoreographer_frameCallback64, void*);
#endif
