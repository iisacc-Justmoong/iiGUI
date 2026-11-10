#pragma once

#include <iiGUI/NativeWidgets.h>
#include <cstdint>
#include <chrono>
#include <condition_variable>
#include <limits>
#include <mutex>
#include <optional>
#include <unordered_map>

namespace ii::gui::detail {
// COM calls may arrive on arbitrary threads. Only a bounded, generation-
// qualified request crosses to the owner thread; no caller-owned pointer does.
class Win32AccessibilityQueue final {
public:
    struct Completion final {
        enum class State {queued,running,finished,cancelled};
        std::mutex mutex;std::condition_variable changed;State state=State::queued;std::int32_t result=0;
        bool begin(){std::lock_guard lock(mutex);if(state!=State::queued)return false;state=State::running;changed.notify_all();return true;}
        void finish(std::int32_t value){std::lock_guard lock(mutex);if(state!=State::running)return;result=value;state=State::finished;changed.notify_all();}
        void cancel(){std::lock_guard lock(mutex);if(state!=State::queued)return;state=State::cancelled;changed.notify_all();}
        std::optional<std::int32_t> wait(){
            std::unique_lock lock(mutex);
            if(!changed.wait_for(lock,std::chrono::seconds(5),[&]{return state!=State::queued;}))state=State::cancelled;
            if(state==State::running)changed.wait(lock,[&]{return state!=State::running;});
            if(state!=State::finished)return {};return result;
        }
    };
    struct Request final {
        std::string target;WidgetInputKind kind;std::uint32_t object_id=0;
        std::string text;std::optional<TextInputRevision> revision;std::shared_ptr<Completion> completion;
    };
private:
    std::mutex mutex_;
    std::unordered_map<std::uint32_t,Request> pending_;
    std::uint32_t next_=1;
    bool open_=true;
public:
    [[nodiscard]] std::optional<std::uint32_t> admit(Request request){
        if(request.target.empty()||!request.object_id||(request.kind!=WidgetInputKind::focus&&request.kind!=WidgetInputKind::activate&&
           (request.kind!=WidgetInputKind::text||!request.revision||!request.completion)))return {};
        std::lock_guard lock(mutex_);
        if(!open_||!next_||pending_.size()>=256)return {};
        const auto token=next_;
        if(next_==std::numeric_limits<std::uint32_t>::max())next_=0;else ++next_;
        pending_.emplace(token,std::move(request));return token;
    }
    [[nodiscard]] std::optional<Request> take(std::uint32_t token){
        std::lock_guard lock(mutex_);auto found=pending_.find(token);if(found==pending_.end())return {};
        auto result=std::move(found->second);pending_.erase(found);return result;
    }
    void close(){
        std::unordered_map<std::uint32_t,Request> pending;
        {std::lock_guard lock(mutex_);open_=false;pending.swap(pending_);}
        for(auto& [_,request]:pending)if(request.completion)request.completion->cancel();
    }
};
} // namespace ii::gui::detail
