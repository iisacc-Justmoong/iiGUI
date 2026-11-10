#pragma once

#include <iiSDKCore/Core/Result/Result.h>
#include <iiSDKCore/Runtime/Execution.h>
#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>
#include <utility>
#include <vector>

namespace ii::gui::detail {

// JNI calls arrive on Android's IME handler thread; the text model belongs to the
// native Looper. A queued timeout cancels before execution. Once work starts,
// the caller waits for its result so a failed call cannot mutate later.
class AndroidImeDispatcher final {
    enum class Phase {queued,running,done,canceled};
    struct PendingBase {Phase phase=Phase::queued;core::ErrorCode cancellation=core::ErrorCode::cancelled;std::condition_variable changed;};
    template<class T>struct Pending final:PendingBase {std::optional<core::Result<T>> result;};
    struct Shared final {std::mutex mutex;bool closed=false;std::size_t count=0;std::vector<std::weak_ptr<PendingBase>> requests;};
    std::shared_ptr<Shared> shared_=std::make_shared<Shared>();
    std::function<bool(runtime::Task)> post_;
    std::chrono::milliseconds timeout_;
    std::thread::id owner_=std::this_thread::get_id();
    template<class T,class F> static core::Result<T> execute(F& work) noexcept {
        try{return work();}catch(...){return core::fail(core::ErrorCode::failed,"Android text-service callback failed",std::current_exception());}
    }
public:
    explicit AndroidImeDispatcher(std::function<bool(runtime::Task)> post,std::chrono::milliseconds timeout=std::chrono::milliseconds(1500))
        :post_(std::move(post)),timeout_(timeout){}
    ~AndroidImeDispatcher(){close();}
    AndroidImeDispatcher(const AndroidImeDispatcher&)=delete;
    AndroidImeDispatcher& operator=(const AndroidImeDispatcher&)=delete;

    template<class T,class F> [[nodiscard]] core::Result<T> call(F work){
        const auto shared=shared_;
        if(std::this_thread::get_id()==owner_){
            {std::lock_guard lock(shared->mutex);if(shared->closed)return core::fail(core::ErrorCode::closed,"Android text service is closed");}
            auto result=execute<T>(work);
            {std::lock_guard lock(shared->mutex);if(shared->closed)return core::fail(core::ErrorCode::closed,"Android text service closed during delivery");}
            return result;
        }
        auto pending=std::make_shared<Pending<T>>();
        {
            std::lock_guard lock(shared->mutex);
            if(shared->closed)return core::fail(core::ErrorCode::closed,"Android text service is closed");
            if(shared->count>=32)return core::fail(core::ErrorCode::full,"Android text-service queue is full");
            std::erase_if(shared->requests,[](const auto& value){return value.expired();});
            ++shared->count;shared->requests.emplace_back(pending);
        }
        auto task=[shared,pending,work=std::move(work),owner=owner_]() mutable {
            {
                std::lock_guard lock(shared->mutex);
                if(pending->phase!=Phase::queued)return;
                if(shared->closed){pending->phase=Phase::canceled;pending->cancellation=core::ErrorCode::closed;--shared->count;pending->changed.notify_all();return;}
                pending->phase=Phase::running;
            }
            auto result=std::this_thread::get_id()==owner?execute<T>(work):
                core::Result<T>{core::fail(core::ErrorCode::wrong_executor,"Android text service ran on the wrong thread")};
            {
                std::lock_guard lock(shared->mutex);
                if(shared->closed)result=core::fail(core::ErrorCode::closed,"Android text service closed during delivery");
                pending->result.emplace(std::move(result));pending->phase=Phase::done;--shared->count;pending->changed.notify_all();
            }
        };
        bool accepted=false;try{accepted=post_&&post_(std::move(task));}catch(...){}
        std::unique_lock lock(shared->mutex);
        if(!accepted&&pending->phase==Phase::queued){pending->phase=Phase::canceled;pending->cancellation=core::ErrorCode::closed;--shared->count;}
        if(pending->phase==Phase::queued&&!pending->changed.wait_for(lock,timeout_,[&]{return pending->phase!=Phase::queued;})){
            pending->phase=Phase::canceled;pending->cancellation=core::ErrorCode::cancelled;--shared->count;
        }
        if(pending->phase==Phase::running)pending->changed.wait(lock,[&]{return pending->phase!=Phase::running;});
        if(pending->phase==Phase::canceled)return core::fail(pending->cancellation,"Android text-service request was not delivered");
        if(!pending->result)return core::fail(core::ErrorCode::closed,"Android text-service result is unavailable");
        return std::move(*pending->result);
    }
    void close()noexcept{
        const auto shared=shared_;std::lock_guard lock(shared->mutex);if(shared->closed)return;shared->closed=true;
        for(auto& weak:shared->requests)if(auto pending=weak.lock();pending&&pending->phase==Phase::queued){
            pending->phase=Phase::canceled;pending->cancellation=core::ErrorCode::closed;--shared->count;pending->changed.notify_all();
        }
    }
};
} // namespace ii::gui::detail
