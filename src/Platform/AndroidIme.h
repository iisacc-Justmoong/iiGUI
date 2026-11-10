#pragma once

#include <iiGUI/NativeWidgets.h>
#include <iiSDKCore/Runtime/Execution.h>
#include <functional>
#include <memory>
#include <optional>

namespace ii::gui::detail {

struct AndroidImeHost final {
    std::function<std::optional<WidgetSnapshot>()> focused;
    std::function<core::Result<TextContent>(std::string_view,TextRange)> content;
    std::function<core::Result<void>(WidgetInput)> input;
    std::optional<WidgetSnapshot> focus(){return focused?focused():std::nullopt;}
    core::Result<TextContent> read(std::string_view id,TextRange range){
        return content?content(id,range):core::Result<TextContent>{core::fail(core::ErrorCode::closed,"Android text surface closed")};
    }
    core::Result<void> send(WidgetInput event){
        return input?input(std::move(event)):core::Result<void>{core::fail(core::ErrorCode::closed,"Android text surface closed")};
    }
};
struct AndroidImeBridgeState;

// Owner-thread lifecycle. JNI callbacks retain only the shared bridge state;
// close rejects pending work before the Activity and widget surface retire.
class AndroidIme final {
    std::shared_ptr<AndroidImeBridgeState> state_;
    explicit AndroidIme(std::shared_ptr<AndroidImeBridgeState> state):state_(std::move(state)){}
public:
    static std::unique_ptr<AndroidIme> create(AndroidImeHost,std::function<bool(runtime::Task)>);
    ~AndroidIme();
    AndroidIme(const AndroidIme&)=delete;
    AndroidIme& operator=(const AndroidIme&)=delete;
    void changed() noexcept;
    void close() noexcept;
};
} // namespace ii::gui::detail
