#pragma once

#include <iiGUI/Graphics.h>
#include <iiSDKCore/Core/Result/Result.h>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>

namespace ii::gui::detail {
// All offsets and deletion lengths are UTF-8 bytes. Strings are owned copies
// so no libwayland callback memory escapes into the host's deferred queue.
struct WaylandImeBatch final {
    std::optional<std::string> committed,preedit;
    std::int32_t cursor_begin=0,cursor_end=0;
    std::uint32_t before=0,after=0;
};
struct WaylandSurrounding final {
    std::string text;
    std::int32_t cursor=0,anchor=0;
    friend bool operator==(const WaylandSurrounding&,const WaylandSurrounding&)=default;
};
enum class WaylandImeEventKind { enter,leave,done };
struct WaylandImeEvent final {
    WaylandImeEventKind kind=WaylandImeEventKind::enter;
    std::uint64_t epoch=0;
    std::uint32_t serial=0;
    WaylandImeBatch batch;
};
struct WaylandImePublication final {
    bool enable=false,external=true,password=false,multiline=false;
    Rect caret;
    std::optional<WaylandSurrounding> surrounding;
};

// Each protocol event replaces one pending field; only done publishes the
// complete owned batch. libwayland's nullable strings never escape callbacks.
class WaylandImeCapture final {
    WaylandImeBatch pending_;
    static core::Result<std::string> copy(const char* text){
        if(!text)return std::string{};
        std::size_t length=0;while(length<=65536&&text[length])++length;
        if(length>65536)return core::fail(core::ErrorCode::out_of_range,"Wayland input string exceeds its bound");
        return std::string(text,length);
    }
public:
    [[nodiscard]] core::Result<void> preedit(const char* text,std::int32_t begin,std::int32_t end){
        auto value=copy(text);if(!value)return std::unexpected(value.error());
        pending_.preedit=std::move(*value);pending_.cursor_begin=begin;pending_.cursor_end=end;return {};
    }
    [[nodiscard]] core::Result<void> commit(const char* text){
        if(!text){pending_.committed.reset();return {};}
        auto value=copy(text);if(!value)return std::unexpected(value.error());pending_.committed=std::move(*value);return {};
    }
    void erase(std::uint32_t before,std::uint32_t after)noexcept {pending_.before=before;pending_.after=after;}
    void reset()noexcept {pending_={};}
    [[nodiscard]] WaylandImeBatch done(){return std::exchange(pending_,{});}
};
} // namespace ii::gui::detail
