#include "../src/Platform/ReadinessWait.h"
#include <chrono>
#include <stdexcept>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

namespace {
void check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
}
int main() {
    using namespace std::chrono_literals;
    using ii::gui::detail::Readiness;
    using ii::gui::detail::wait_readable_until;
    int pair[2];
    check(socketpair(AF_UNIX, SOCK_STREAM, 0, pair) == 0, "cannot create test socket");
    struct Close final { int* pair; ~Close() { close(pair[0]); close(pair[1]); } } close_pair{pair};
    check(wait_readable_until(pair[0], std::chrono::steady_clock::now() + 30ms) == Readiness::timeout,
          "an idle descriptor did not reach its deadline");
    std::jthread writer([&] { std::this_thread::sleep_for(20ms); const char byte = 'x'; (void)write(pair[1], &byte, 1); });
    check(wait_readable_until(pair[0], std::chrono::steady_clock::now() + 2s) == Readiness::ready,
          "readable data did not wake the waiter");
    char byte = 0; check(read(pair[0], &byte, 1) == 1 && byte == 'x', "readiness lost data");
    check(wait_readable_until(-1, std::chrono::steady_clock::now() + 2s) == Readiness::failed,
          "an invalid descriptor did not fail");
}
