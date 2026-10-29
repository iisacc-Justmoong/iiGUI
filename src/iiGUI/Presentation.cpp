#include "Presentation.h"
#include <iiSDKCore/Core/Property/Observable.h>
#include <algorithm>
#include <atomic>
#include <limits>
#include <thread>

namespace ii::gui {
struct PresentationStream::Impl final {
    const std::thread::id owner = std::this_thread::get_id();
    std::shared_ptr<std::atomic_size_t> observers=std::make_shared<std::atomic_size_t>(0);
    std::atomic_bool stopped{false};
    core::Observable<PresentationEvent> event;
    std::function<runtime::Submission(runtime::Task)> deferred;
    std::vector<runtime::TaskToken> pending;
    runtime::Clock::time_point last{};
    std::uint64_t sequence = 0;
    std::atomic_uint64_t failures{0};
    bool has_time = false, notifying = false;
    void read() const {
        if (owner != std::this_thread::get_id()) throw std::logic_error("Presentation source requires its owner thread");
    }
    core::Result<void> write() const {
        if (owner != std::this_thread::get_id()) return core::fail(core::ErrorCode::wrong_executor,"Presentation source requires its owner thread");
        if (stopped.load()) return core::fail(core::ErrorCode::closed,"Presentation source is closed");
        if (notifying) return core::fail(core::ErrorCode::busy,"Presentation source is notifying");
        return {};
    }
    core::Result<void> emit(PresentationEvent value) {
        const bool previous = notifying;
        notifying = true;
        struct Restore { bool& target; bool value; ~Restore(){target=value;} } restore{notifying,previous};
        try {
            core::ObservableReadScope::pause([&]{event.set(std::move(value));});
            return {};
        } catch (...) {
            ++failures;
            return core::fail(core::ErrorCode::failed,"Presentation observer failed after commit",std::current_exception());
        }
    }
    void stop(std::string error = {}) noexcept {
        if (stopped.exchange(true)) return;
        auto tokens = std::move(pending);
        deferred = {};
        for (const auto& token : tokens) (void)token.cancel();
        (void)emit({PresentationKind::closed,last,sequence,GraphicsApi::none,std::move(error)});
    }
};
PresentationStream::PresentationStream():impl_(std::make_shared<Impl>()){}
PresentationStream::PresentationStream(std::function<runtime::Submission(runtime::Task)> deferred):PresentationStream(){
    if(!deferred) throw std::invalid_argument("Deferred presentation source requires an executor");
    impl_->deferred=std::move(deferred);
}
PresentationStream::~PresentationStream(){
    if(impl_->owner!=std::this_thread::get_id())std::terminate();
    impl_->stop();
}
bool PresentationStream::closed()const{impl_->read();return impl_->stopped.load();}
bool PresentationStream::has_observers()const noexcept{return !impl_->stopped.load()&&impl_->observers->load()!=0;}
std::uint64_t PresentationStream::sequence()const{impl_->read();return impl_->sequence;}
std::uint64_t PresentationStream::observer_failures()const{impl_->read();return impl_->failures.load();}
core::Result<core::ObservableSubscription> PresentationStream::observe(std::function<void(const PresentationEvent&)> callback){
    auto state=impl_;if(auto ready=state->write();!ready)return std::unexpected(ready.error());
    if(!callback)return core::fail(core::ErrorCode::invalid_argument,"Presentation observer is empty");
    struct Interest final {
        std::shared_ptr<std::atomic_size_t> count;
        explicit Interest(std::shared_ptr<std::atomic_size_t> value):count(std::move(value)){++*count;}
        ~Interest(){--*count;}
    };
    try {
        auto interest=std::make_shared<Interest>(state->observers);
        return state->event.observe([interest=std::move(interest),callback=std::move(callback)](const auto& e,std::uint64_t){callback(e);});
    }catch(...){return core::fail(core::ErrorCode::full,"Presentation observer capacity exhausted",std::current_exception());}
}
core::Result<void> PresentationStream::queued(runtime::Clock::time_point time,GraphicsApi api){
    auto state=impl_;if(auto ready=state->write();!ready)return ready;
    if(unsigned(api)>unsigned(GraphicsApi::webgl2)||(state->has_time&&time<state->last))
        return core::fail(core::ErrorCode::invalid_argument,"Invalid presentation time or graphics API");
    if(state->sequence==UINT64_MAX)return core::fail(core::ErrorCode::full,"Presentation sequence exhausted");
    PresentationEvent value{PresentationKind::queued,time,state->sequence+1,api,{}};
    if(!state->deferred){state->last=time;state->has_time=true;++state->sequence;return state->emit(std::move(value));}
    std::erase_if(state->pending,[](const auto& token){return !token.pending();});
    if(state->pending.size()==2048){state->stop("Presentation delivery capacity exhausted");return core::fail(core::ErrorCode::full,"Presentation delivery capacity exhausted");}
    auto admitting=std::make_shared<std::atomic_bool>(true);auto inline_call=std::make_shared<std::atomic_bool>(false);
    auto submit=state->deferred;
    runtime::Submission accepted;
    try {
        accepted=submit([weak=std::weak_ptr<Impl>(state),value,admitting,inline_call]{
            if(*admitting){*inline_call=true;return;}
            if(auto live=weak.lock()){
                if(live->owner!=std::this_thread::get_id()){++live->failures;return;}
                if(!live->stopped.load())(void)live->emit(value);
            }
        });
    }catch(...){*admitting=false;state->stop("Presentation executor threw");return core::fail(core::ErrorCode::failed,"Presentation executor threw",std::current_exception());}
    *admitting=false;
    if(!accepted||*inline_call||!accepted.token.pending()){(void)accepted.token.cancel();state->stop("Presentation delivery was rejected or not deferred");return core::fail(core::ErrorCode::full,"Presentation delivery was rejected or not deferred");}
    state->last=time;state->has_time=true;++state->sequence;
    state->pending.push_back(std::move(accepted.token));
    return {};
}
void PresentationStream::close()noexcept{
    if(impl_->owner!=std::this_thread::get_id())std::terminate();
    impl_->stop();
}
}
