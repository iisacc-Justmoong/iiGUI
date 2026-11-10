#include "../src/Platform/WaylandPoll.h"
#include <unistd.h>
#include <array>
#include <cstdio>
#include <stdexcept>

namespace {
using ii::gui::detail::WaylandWaitFd;
using ii::gui::detail::native_wayland_poll;
void check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
struct Pipe final {
    int read = -1, write = -1;
    Pipe() { int fds[2]; if (pipe(fds)) throw std::runtime_error("Cannot create test pipe"); read=fds[0]; write=fds[1]; }
    ~Pipe() { if (read >= 0) close(read); if (write >= 0) close(write); }
};
void wake_eof_is_not_a_compositor_disconnect() {
    Pipe wake;
    close(wake.write); wake.write=-1;
    std::array descriptors{WaylandWaitFd{.fd=wake.read,.read=true}};
    check(native_wayland_poll(descriptors,0,wake.read) && descriptors[0].readable && !descriptors[0].error,
          "A closed clipboard wake pipe was treated as a compositor failure");
}
void compositor_disconnect_and_transfer_writable() {
    Pipe display, transfer;
    close(display.write); display.write=-1;
    std::array descriptors{WaylandWaitFd{.fd=display.read,.read=true}, WaylandWaitFd{.fd=transfer.write,.write=true}};
    check(native_wayland_poll(descriptors,0,-1) && descriptors[0].error && descriptors[1].writable,
          "Compositor disconnect or clipboard transfer readiness was lost");
}
void simultaneous_wakes_are_reported() {
    Pipe first, second, wake;
    const char value='x';
    check(write(first.write,&value,1)==1 && write(second.write,&value,1)==1 && write(wake.write,&value,1)==1,
          "Cannot signal test pipes");
    std::array descriptors{WaylandWaitFd{.fd=first.read,.read=true}, WaylandWaitFd{.fd=second.read,.read=true},
                           WaylandWaitFd{.fd=wake.read,.read=true}};
    check(native_wayland_poll(descriptors,0,wake.read) && descriptors[0].readable &&
          descriptors[1].readable && descriptors[2].readable, "Simultaneous event sources were not reported");
}
} // namespace
int main() {
    try { wake_eof_is_not_a_compositor_disconnect(); compositor_disconnect_and_transfer_writable(); simultaneous_wakes_are_reported(); }
    catch (const std::exception& error) { std::fprintf(stderr,"%s\n",error.what()); return 1; }
}
