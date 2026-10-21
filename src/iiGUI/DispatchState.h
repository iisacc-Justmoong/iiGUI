#pragma once
#include <iiGUI/Host.h>
#include <atomic>

namespace ii::gui::detail {
struct DispatchState final {
    explicit DispatchState(std::size_t capacity) : loop(capacity) {}
    runtime::EventLoop loop;
    std::atomic<bool> accepting{true};
    std::unique_ptr<runtime::HostLoop> host;
    runtime::HostLoopStats last_host_stats;
    template<class Wake> void connect(Wake wake) {
        host=std::make_unique<runtime::HostLoop>(loop,std::move(wake));
    }
    core::Result<void> disconnect() {
        if (!host) return {};
        auto closed=host->close();
        last_host_stats=host->stats();
        host.reset();
        return closed;
    }
    runtime::HostLoopStats host_stats() const { return host ? host->stats() : last_host_stats; }
};
} // namespace ii::gui::detail
