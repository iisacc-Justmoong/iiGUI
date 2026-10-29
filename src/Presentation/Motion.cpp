#include <iiGUI/Motion.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <thread>
#include <vector>

namespace ii::gui {
struct MotionPreferences::Impl final {
    std::thread::id owner=std::this_thread::get_id();
    MotionState state;
    bool notifying=false;
    std::vector<std::weak_ptr<const MotionChangePort>> receivers;
    core::Result<void> ready(bool edit=false)const {
        if(owner!=std::this_thread::get_id())return core::fail(core::ErrorCode::wrong_executor,"Motion preferences require their owning thread");
        if(edit&&notifying)return core::fail(core::ErrorCode::busy,"Motion preferences cannot mutate from a change receiver");
        return {};
    }
    core::Result<void> notify() {
        ++state.revision;const auto committed=state;
        std::erase_if(receivers,[](const auto& receiver){return receiver.expired();});
        std::array<std::shared_ptr<const MotionChangePort>,256> pending{};
        std::array<const MotionChangePort*,256> closed{};
        std::size_t count=0,closed_count=0;
        for(auto& receiver:receivers)if(auto port=receiver.lock())pending[count++]=std::move(port);
        notifying=true;struct Reset final {bool& flag;~Reset(){flag=false;}}reset{notifying};
        std::optional<runtime::QueryError> failure;
        for(std::size_t i=0;i<count;++i){
            auto result=pending[i]->send(committed);
            if(!result){if(result.error().code==runtime::QueryErrorCode::receiver_expired)closed[closed_count++]=pending[i].get();else if(!failure)failure=result.error();}
        }
        std::erase_if(receivers,[&](const auto& receiver){auto port=receiver.lock();return !port||std::find(closed.begin(),closed.begin()+closed_count,port.get())!=closed.begin()+closed_count;});
        if(failure)return core::fail(core::ErrorCode::failed,"Motion preferences committed; a change receiver failed",failure->cause);
        return {};
    }
};
MotionPreferences::MotionPreferences():impl_(std::make_unique<Impl>()){}
MotionPreferences::~MotionPreferences()=default;
core::Result<MotionState> MotionPreferences::state()const{if(auto result=impl_->ready();!result)return std::unexpected(result.error());return impl_->state;}
core::Result<runtime::Clock::duration> MotionPreferences::duration(runtime::Clock::duration base)const {
    if(auto result=impl_->ready();!result)return std::unexpected(result.error());
    if(!impl_->state.animated()||base<=runtime::Clock::duration::zero())return runtime::Clock::duration::zero();
    const auto factor=std::isfinite(impl_->state.speed)?std::clamp(impl_->state.speed,.1,4.):1.;
    using Milliseconds=std::chrono::duration<long double,std::milli>;
    const auto value=std::round(Milliseconds(base).count()/factor);
    if(value>=Milliseconds(runtime::Clock::duration::max()).count())return runtime::Clock::duration::max();
    return std::chrono::duration_cast<runtime::Clock::duration>(Milliseconds(value));
}
core::Result<void> MotionPreferences::set_enabled(bool value){if(auto result=impl_->ready(true);!result)return result;if(value==impl_->state.enabled)return {};impl_->state.enabled=value;return impl_->notify();}
core::Result<void> MotionPreferences::set_reduced_motion(bool value){if(auto result=impl_->ready(true);!result)return result;if(value==impl_->state.reduced_motion)return {};impl_->state.reduced_motion=value;return impl_->notify();}
core::Result<void> MotionPreferences::set_speed(double value){if(auto result=impl_->ready(true);!result)return result;if(value==impl_->state.speed||(std::isnan(value)&&std::isnan(impl_->state.speed)))return {};impl_->state.speed=value;return impl_->notify();}
core::Result<void> MotionPreferences::observe(std::shared_ptr<const MotionChangePort> receiver) {
    if(auto result=impl_->ready();!result)return result;
    if(!receiver)return core::fail(core::ErrorCode::invalid_argument,"Motion receiver must be nonnull");
    std::erase_if(impl_->receivers,[](const auto& receiver){return receiver.expired();});
    for(auto& current:impl_->receivers)if(current.lock()==receiver)return {};
    if(impl_->receivers.size()>=256)return core::fail(core::ErrorCode::full,"Motion preferences have 256 change receivers");
    impl_->receivers.push_back(receiver);return {};
}
}
