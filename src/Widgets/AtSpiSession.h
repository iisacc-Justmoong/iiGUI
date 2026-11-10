#pragma once
#include "AtSpiCoordinator.h"
#include "AtSpiDbus.h"
#include <functional>
#include <memory>
#include <optional>
#include <string>

typedef struct _GCancellable GCancellable;

namespace ii::gui::detail {
struct AtSpiConnection final {std::shared_ptr<GDBusConnection> value;std::string name;};
struct AtSpiReference final {std::string bus,path;};

// The post function must queue work for the coordinator's owner thread and be
// safe to call from a worker. pump() is called by that owner at bounded cadence.
class AtSpiSession final : public std::enable_shared_from_this<AtSpiSession> {
public:
    using Post=AtSpiCoordinator::Post;
    using Connect=std::function<std::optional<AtSpiConnection>(GCancellable*)>;
    using Embed=std::function<std::optional<AtSpiReference>(const AtSpiConnection&,GCancellable*)>;
    [[nodiscard]] static std::shared_ptr<AtSpiSession> create(std::shared_ptr<AtSpiCoordinator>,Post,Connect={},Embed={});
    ~AtSpiSession();
    AtSpiSession(const AtSpiSession&)=delete;
    AtSpiSession& operator=(const AtSpiSession&)=delete;
    void pump()noexcept;
    void stop()noexcept;
    [[nodiscard]] bool connected()const noexcept;
private:
    struct Impl;
    explicit AtSpiSession(std::unique_ptr<Impl>);
    void start();
    std::unique_ptr<Impl> impl_;
};
} // namespace ii::gui::detail
