#pragma once
#include <iiGUI/Host.h>
#include <atomic>
#include <iiSDKCore/Runtime/TaskScope.h>
#include <mutex>

namespace ii::gui::detail {
struct DispatchState final {
    explicit DispatchState(std::size_t capacity, runtime::EventLoop* external = nullptr)
        : owned_loop(external ? nullptr : std::make_unique<runtime::EventLoop>(capacity)),
          loop(external ? *external : *owned_loop) {}
    std::unique_ptr<runtime::EventLoop> owned_loop;
    runtime::EventLoop& loop;
    std::atomic<bool> accepting{true};
    runtime::TaskScope tasks;
    std::mutex token_mutex;
    std::vector<runtime::TaskToken> tokens;
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
    runtime::Submission track(runtime::Submission submitted){
        if(!submitted)return submitted;
        std::unique_lock lock(token_mutex);
        if(!accepting.load(std::memory_order_acquire)){lock.unlock();(void)submitted.token.cancel();return {{},runtime::SubmitError::stopped};}
        std::erase_if(tokens,[](const auto& token){return !token.pending();});tokens.push_back(submitted.token);return submitted;
    }
    void retire(){
        accepting.store(false,std::memory_order_release);tasks.request_close();
        std::vector<runtime::TaskToken> retired;{std::lock_guard lock(token_mutex);retired.swap(tokens);}
        for(const auto& token:retired)(void)token.cancel();
    }
};
} // namespace ii::gui::detail
