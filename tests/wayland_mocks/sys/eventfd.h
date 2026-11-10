#pragma once
#include <unistd.h>
#include <mutex>
#include <cerrno>
#include <vector>
extern "C" int fake_wayland_eventfd(unsigned int, int);
extern "C" ssize_t fake_wayland_read(int, void*, size_t);
extern "C" ssize_t fake_wayland_write(int, const void*, size_t);
#define EFD_NONBLOCK 1
#define EFD_CLOEXEC 2
#define eventfd fake_wayland_eventfd
#define read fake_wayland_read
#define write fake_wayland_write
