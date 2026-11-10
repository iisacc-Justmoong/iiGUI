#pragma once
#include "AtSpiSession.h"
#include "AtSpiTextSource.h"
#include "WidgetState.h"
#include <chrono>
#include <memory>
#include <stdexcept>

namespace ii::gui::detail {
// Shared by the root native host and its child windows. Accessibility failure
// is isolated from the application's window and rendering lifetime.
class AtSpiHost final {
    std::shared_ptr<AtSpiCoordinator> coordinator_;
    std::shared_ptr<AtSpiSession> session_;
    runtime::TaskToken pulse_;
    AtSpiHost()=default;
public:
    AtSpiHost(const AtSpiHost&)=delete;
    AtSpiHost& operator=(const AtSpiHost&)=delete;
    ~AtSpiHost(){(void)pulse_.cancel();if(session_)session_->stop();}
    [[nodiscard]] static std::shared_ptr<AtSpiHost> create(UiExecutor executor,std::string name)noexcept{
        try{
            auto result=std::shared_ptr<AtSpiHost>(new AtSpiHost);
            auto post=[executor](std::function<void()> task){return bool(executor.post(std::move(task)));};
            result->coordinator_=AtSpiCoordinator::create(std::move(name),post);
            result->session_=AtSpiSession::create(result->coordinator_,post);if(!result->session_)return {};
            auto weak=std::weak_ptr<AtSpiSession>(result->session_);
            auto timer=executor.every(std::chrono::milliseconds(16),[weak]{if(auto session=weak.lock())session->pump();});
            if(!timer)return {};result->pulse_=timer.token;return result;
        }catch(...){return {};}
    }
    [[nodiscard]] std::optional<std::uint64_t> open(std::string title,Size size,std::optional<WidgetPoint> origin=std::nullopt)noexcept{
        try{return coordinator_->open_window(std::move(title),size,origin);}catch(...){return std::nullopt;}
    }
    void geometry(std::uint64_t id,std::string title,Size size,std::optional<WidgetPoint> origin=std::nullopt)noexcept{
        try{(void)coordinator_->geometry(id,std::move(title),size,origin);}catch(...){}
    }
    void close(std::uint64_t id)noexcept{try{(void)coordinator_->close_window(id);}catch(...){}}
    void clear(std::uint64_t id)noexcept{try{(void)coordinator_->unpublish(id);}catch(...){}}
    void publish(std::uint64_t id,const std::shared_ptr<WidgetState>& state,std::function<void(runtime::Task)> invoke)noexcept{
        try{
            auto snapshots=state->accessibility_snapshots();if(!snapshots)return;
            auto weak=std::weak_ptr<WidgetState>(state);
            auto value_invoke=invoke;
            (void)coordinator_->publish(id,*snapshots,[weak,invoke=std::move(invoke)](WidgetInput input){
                if(auto owner=weak.lock())invoke([owner,input=std::move(input)]{auto result=owner->input(input);if(!result)throw std::runtime_error(result.error().message);});
            },[weak](const AtSpiTextTarget& target)->std::optional<AtSpiTextIndex>{if(auto owner=weak.lock())return read_atspi_text(*owner,target);return std::nullopt;},
            AtSpiCoordinator::ValueCallbacks{
                .can_set=[weak](std::string_view id,double value){try{if(auto owner=weak.lock()){auto result=owner->can_set_accessibility_value(id,value);return result&&*result;}return false;}catch(...){return false;}},
                .set=[weak,invoke=std::move(value_invoke)](std::string_view id,double value){if(auto owner=weak.lock())invoke([owner,id=std::string(id),value]{auto result=owner->set_accessibility_value(id,value);if(!result)throw std::runtime_error(result.error().message);});}});
        }catch(...){}
    }
    void update(std::uint64_t id,const std::shared_ptr<WidgetState>& state)noexcept{
        try{auto snapshots=state->accessibility_snapshots();if(snapshots)(void)coordinator_->update(id,*snapshots);}catch(...){}
    }
};
} // namespace ii::gui::detail
