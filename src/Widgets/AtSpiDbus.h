#pragma once
#include "AtSpiApplication.h"
#include <functional>
#include <memory>
#include <string>

typedef struct _GDBusConnection GDBusConnection;

namespace ii::gui::detail {
// Owns only registrations on a borrowed connection. All operations and D-Bus
// callbacks run on the same owner context as the supplied application model.
// The action callback must enqueue, not execute, user code synchronously.
class AtSpiDbusBridge final {
public:
    using Action=std::function<bool(std::string,WidgetInputKind)>;
    using ValueInput=std::function<bool(std::string,double)>;
    struct TextCallbacks final {
        std::function<std::optional<AtSpiTextData>(std::string_view)> read;
        std::function<bool(std::string,WidgetInput)> input;
    };
    AtSpiDbusBridge(AtSpiApplication&,Action,TextCallbacks={},ValueInput={});
    ~AtSpiDbusBridge();
    AtSpiDbusBridge(const AtSpiDbusBridge&)=delete;
    AtSpiDbusBridge& operator=(const AtSpiDbusBridge&)=delete;
    [[nodiscard]] bool attach(GDBusConnection*,std::string local_name);
    void registry_parent(std::string bus,std::string path);
    // Reconcile the current projection and publish structural/state changes.
    void changed()noexcept;
    void detach()noexcept;
private:
    struct Impl;
    std::shared_ptr<Impl> impl_;
};
} // namespace ii::gui::detail
