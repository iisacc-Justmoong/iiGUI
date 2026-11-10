#pragma once
#include <iiGUI/NativeWidgets.h>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_set>

namespace ii::gui::detail {
class WidgetState;
class AtSpiHost final {
    std::uint64_t next_=1;
    std::unordered_set<std::uint64_t> live_;
public:
    inline static int created=0,opened=0,closed=0,published=0,cleared=0,updated=0;
    static void reset(){created=opened=closed=published=cleared=updated=0;}
    static std::shared_ptr<AtSpiHost> create(UiExecutor,std::string){++created;return std::make_shared<AtSpiHost>();}
    std::optional<std::uint64_t> open(std::string,Size,std::optional<WidgetPoint> = {}){++opened;return next_++;}
    void geometry(std::uint64_t,std::string,Size,std::optional<WidgetPoint> = {}){}
    void close(std::uint64_t id){live_.erase(id);++closed;}
    void clear(std::uint64_t id){if(live_.erase(id))++cleared;}
    void publish(std::uint64_t id,const std::shared_ptr<WidgetState>&,std::function<void(runtime::Task)>){live_.insert(id);++published;}
    void update(std::uint64_t,const std::shared_ptr<WidgetState>&){++updated;}
};
} // namespace ii::gui::detail
