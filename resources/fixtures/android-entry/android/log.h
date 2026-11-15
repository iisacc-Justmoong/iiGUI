#pragma once

#define ANDROID_LOG_ERROR 6

#ifdef __cplusplus
extern "C" {
#endif
int __android_log_write(int priority, const char* tag, const char* text);
#ifdef __cplusplus
}
#endif
