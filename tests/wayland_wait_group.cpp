#include "../src/Platform/WaylandWaitGroup.h"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using namespace ii::gui::detail;
void check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
template<class Exception, class F> bool throws(F&& callback) {
    try { callback(); } catch (const Exception&) { return true; }
    return false;
}
struct Source final : WaylandWaitSource {
    std::string name;
    int fd, deadline = -1, transfer = -1;
    bool blocked = false, fail_prepare = false, fail_flush = false, fail_read = false;
    int prepared = 0, read = 0, cancelled = 0, dispatched = 0, flushed = 0;
    std::vector<std::pair<int, bool>> transfers;
    std::vector<std::string>* events;
    Source(std::string value, int descriptor, std::vector<std::string>& log)
        : name(std::move(value)), fd(descriptor), events(&log) {}
    int descriptor() const override { return fd; }
    void dispatch() override { ++dispatched; events->push_back(name + ":dispatch"); }
    void prepare() override {
        events->push_back(name + ":prepare");
        if (fail_prepare) throw std::runtime_error("prepare failed");
        ++prepared;
    }
    bool flush() override {
        ++flushed; events->push_back(name + ":flush");
        if (fail_flush) throw std::runtime_error("flush failed");
        return blocked;
    }
    int limit_timeout(int current) const override {
        return deadline < 0 ? current : current < 0 ? deadline : std::min(current, deadline);
    }
    void append_writable(std::vector<WaylandWaitFd>& descriptors) const override {
        if (transfer >= 0) descriptors.push_back({.fd=transfer, .write=true});
    }
    void writable(int descriptor, bool error) override { transfers.emplace_back(descriptor, error); }
    void read_events() override {
        ++read; --prepared; events->push_back(name + ":read");
        if (fail_read) throw std::runtime_error("read failed");
    }
    void cancel() noexcept override { ++cancelled; --prepared; events->push_back(name + ":cancel"); }
};

void multiplexes_readiness_and_deadlines() {
    std::vector<std::string> events;
    Source a("a", 10, events), b("b", 11, events);
    a.deadline = 40; a.transfer = 20; b.blocked = true; b.deadline = 80;
    std::vector<WaylandWaitSource*> sources{&a, &b};
    wait_wayland(sources, 5, 100, [&](auto fds, int timeout) {
        check(timeout == 40 && fds.size() == 4, "Wake, display, transfer, or earliest deadline was lost");
        auto find = [&](int fd) -> WaylandWaitFd& {
            auto it = std::ranges::find_if(fds, [=](const auto& value) { return value.fd == fd; });
            check(it != fds.end(), "Expected poll descriptor was missing"); return *it;
        };
        check(find(5).read && find(10).read && find(11).read && find(11).write && find(20).write,
              "Wayland descriptor interests were incorrect");
        find(10).readable = true; find(11).writable = true; find(20).writable = true;
        return true;
    });
    check(a.read == 1 && a.cancelled == 0 && b.read == 0 && b.cancelled == 1 &&
          !a.prepared && !b.prepared && a.dispatched == 2 && b.dispatched == 2 && b.flushed == 2,
          "Multiple displays did not finish every prepared read or writable flush");
    check(a.transfers == std::vector<std::pair<int,bool>>{{20,false}} && b.transfers.empty(),
          "Writable transfer was not routed to its owning connection");
}

void transfers_only_receive_readiness_or_error() {
    std::vector<std::string> events;
    Source a("a", 10, events), b("b", 11, events);
    a.transfer = 20; b.transfer = 21;
    std::vector<WaylandWaitSource*> sources{&a, &b};
    wait_wayland(sources, 5, -1, [](auto fds, int timeout) {
        check(timeout == -1, "A transfer without a deadline introduced a periodic wake");
        for (auto& fd : fds) if (fd.fd == 10) fd.readable = true;
        return true;
    });
    check(a.transfers.empty() && b.transfers.empty(), "Unrelated events retried transfers");
    wait_wayland(sources, 5, -1, [](auto fds, int) {
        for (auto& fd : fds) if (fd.fd == 21) fd.error = true;
        return true;
    });
    check(a.transfers.empty() && b.transfers == std::vector<std::pair<int,bool>>{{21,true}},
          "Transfer failure was not routed to its owner");
}

void transfer_deadlines_do_not_introduce_periodic_wakes() {
    using namespace std::chrono_literals;
    const auto now = std::chrono::steady_clock::time_point{};
    check(limit_wayland_deadline(-1, now, now + 2s) == 2000, "A transfer woke before its expiry");
    check(limit_wayland_deadline(300, now, now + 2s) == 300, "An earlier caller deadline was lost");
    check(limit_wayland_deadline(-1, now, now + 1500ms + 1ns) == 1501, "The remaining timeout was rounded down");
    check(limit_wayland_deadline(-1, now, now - 1ms) == 0, "An expired transfer was not retired");
}

void failures_and_interruptions_cancel_every_prepared_read() {
    std::vector<std::string> events;
    Source a("a", 10, events), b("b", 11, events);
    std::vector<WaylandWaitSource*> sources{&a, &b};
    b.fail_prepare = true;
    check(throws<std::runtime_error>([&] { wait_wayland(sources, 5, -1, [](auto, int) { return true; }); }),
          "Prepare failure was hidden");
    check(a.cancelled == 1 && !a.prepared && !b.prepared, "Prepare failure leaked another prepared read");
    b.fail_prepare = false; b.fail_flush = true;
    check(throws<std::runtime_error>([&] { wait_wayland(sources, 5, -1, [](auto, int) { return true; }); }),
          "Flush failure was hidden");
    check(a.cancelled == 2 && b.cancelled == 1 && !a.prepared && !b.prepared, "Flush failure leaked prepared reads");
    b.fail_flush = false;
    wait_wayland(sources, 5, -1, [](auto, int) { return false; });
    check(a.cancelled == 3 && b.cancelled == 2 && !a.prepared && !b.prepared, "Interrupted polling leaked prepared reads");
    check(throws<std::runtime_error>([&] { wait_wayland(sources, 5, -1, [](auto, int) -> bool { throw std::runtime_error("poll failed"); }); }),
          "Poll failure was hidden");
    check(a.cancelled == 4 && b.cancelled == 3 && !a.prepared && !b.prepared, "Poll failure leaked prepared reads");
    a.fail_read = true;
    check(throws<std::runtime_error>([&] {
        wait_wayland(sources, 5, -1, [](auto fds, int) { for (auto& fd : fds) if (fd.fd == 10) fd.readable = true; return true; });
    }), "Read failure was hidden");
    check(a.read == 1 && b.cancelled == 4 && !a.prepared && !b.prepared, "Read failure leaked another prepared read");
}

void invalid_sources_and_disconnects_are_rejected() {
    std::vector<std::string> events;
    Source a("a", 10, events), b("b", 10, events);
    std::vector<WaylandWaitSource*> sources{&a, &b};
    check(throws<std::invalid_argument>([&] { wait_wayland(sources, 5, -1, [](auto, int) { return true; }); }),
          "Duplicate display fd was accepted");
    check(!a.prepared && !b.prepared, "Invalid descriptors were prepared");
    b.fd = 11;
    check(throws<std::runtime_error>([&] {
        wait_wayland(sources, 5, -1, [](auto fds, int) { for (auto& fd : fds) if (fd.fd == 11) fd.error = true; return true; });
    }), "Compositor disconnect was accepted");
    check(!a.prepared && !b.prepared && a.cancelled == 1 && b.cancelled == 1,
          "A disconnected compositor left a prepared read");
}
} // namespace

int main() {
    try { multiplexes_readiness_and_deadlines(); transfers_only_receive_readiness_or_error(); transfer_deadlines_do_not_introduce_periodic_wakes(); failures_and_interruptions_cancel_every_prepared_read(); invalid_sources_and_disconnects_are_rejected(); }
    catch (const std::exception& error) { std::fprintf(stderr, "%s\n", error.what()); return 1; }
}
