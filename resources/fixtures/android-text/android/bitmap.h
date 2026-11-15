#pragma once
#include <iiGUI/AndroidJni.h>
#include <cstdint>
struct AndroidBitmapInfo { std::uint32_t width, height, stride, format, flags; };
inline constexpr int ANDROID_BITMAP_RESULT_SUCCESS = 0;
inline constexpr int ANDROID_BITMAP_FORMAT_RGBA_8888 = 1;
int AndroidBitmap_getInfo(JNIEnv*, jobject, AndroidBitmapInfo*);
int AndroidBitmap_lockPixels(JNIEnv*, jobject, void**);
int AndroidBitmap_unlockPixels(JNIEnv*, jobject);
