#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <windows.h>
#ifndef min
#define min(a, b) (((a) < (b)) ? (a) : (b))
#endif
#ifndef max
#define max(a, b) (((a) > (b)) ? (a) : (b))
#endif
#include <iiGUI/ImageModel.h>
#include <iiGUI/AvatarModel.h>
#include <iiGUI/NativeImageView.h>
#include <iiGUI/NativeAvatarView.h>
#if !defined(min) || !defined(max)
#error "iiGUI image headers must preserve the consuming application's macros"
#endif
