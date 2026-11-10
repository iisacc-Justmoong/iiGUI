#pragma once

#include <iiGUI/NativeWidgets.h>
#include <iiSDKCore/Core/Text/Text.h>
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>

namespace ii::gui::detail {

// IMM's GCS_* values. The native adapter verifies these against imm.h.
inline constexpr std::uint32_t win32_ime_composition=0x0008;
inline constexpr std::uint32_t win32_ime_cursor=0x0080;
inline constexpr std::uint32_t win32_ime_result=0x0800;

struct Win32ImeMessage final {
    std::optional<std::u16string> committed,composing;
    std::optional<std::size_t> cursor;
    bool cancel=false;
};
struct Win32TextOwner final {
    std::string id;
    std::shared_ptr<const void> identity;
    std::uint64_t focus=0;
    bool versioned=false;
    friend bool operator==(const Win32TextOwner&,const Win32TextOwner&)=default;
};
[[nodiscard]] inline Win32TextOwner win32_text_owner(const WidgetSnapshot& value){
    return {value.id,value.text_revision?value.text_revision->identity:nullptr,
            value.text_revision?value.text_revision->focus:0,bool(value.text_revision)};
}

[[nodiscard]] inline core::Result<std::string> win32_ime_utf8(std::u16string_view text){
    if(text.size()>65536)return core::fail(core::ErrorCode::out_of_range,"Windows IME text exceeds its UTF-16 bound");
    auto converted=core::to_utf8(text);
    if(!converted||converted->find('\0')!=std::string::npos)
        return core::fail(core::ErrorCode::invalid_argument,"Windows IME returned invalid UTF-16 text");
    if(converted->size()>65536)return core::fail(core::ErrorCode::out_of_range,"Windows IME text exceeds its byte bound");
    return std::move(*converted);
}
[[nodiscard]] inline core::Result<void> win32_ime_validate(const Win32ImeMessage& value){
    if(value.cancel&&(value.committed||value.composing||value.cursor))
        return core::fail(core::ErrorCode::invalid_argument,"Windows IME cancellation contains text");
    if(value.cursor&&!value.composing)return core::fail(core::ErrorCode::invalid_argument,"Windows IME cursor has no candidate");
    if(value.committed)if(auto text=win32_ime_utf8(*value.committed);!text)return std::unexpected(text.error());
    if(value.composing){
        if(auto text=win32_ime_utf8(*value.composing);!text)return std::unexpected(text.error());
        const auto cursor=value.cursor.value_or(value.composing->size());
        if(cursor>value.composing->size()||(cursor>0&&cursor<value.composing->size()&&
           (*value.composing)[cursor]>=0xdc00&&(*value.composing)[cursor]<=0xdfff))
            return core::fail(core::ErrorCode::out_of_range,"Windows IME cursor splits or exceeds the candidate");
    }
    return {};
}

// Reader has ImmGetCompositionStringW's byte-count contract, without an IMM
// dependency. Copy both strings while the native input context is still held.
template<class Reader> [[nodiscard]] core::Result<Win32ImeMessage> win32_ime_capture(Reader& reader,std::uint32_t flags){
    Win32ImeMessage value;if(!flags){value.cancel=true;return value;}
    const auto read=[&](std::uint32_t kind)->core::Result<std::u16string>{
        const auto bytes=reader(kind,nullptr,0);
        if(bytes<0)return core::fail(core::ErrorCode::failed,"Windows IME string is unavailable");
        if(bytes%2||bytes>131072)return core::fail(core::ErrorCode::out_of_range,"Invalid Windows IME byte count");
        std::u16string result(std::size_t(bytes)/2,u'\0');
        if(bytes&&reader(kind,result.data(),std::uint32_t(bytes))!=bytes)
            return core::fail(core::ErrorCode::failed,"Windows IME string changed during its read");
        return result;
    };
    if(flags&win32_ime_result){auto text=read(win32_ime_result);if(!text)return std::unexpected(text.error());value.committed=std::move(*text);}
    if(flags&(win32_ime_composition|win32_ime_cursor)){
        auto text=read(win32_ime_composition);if(!text)return std::unexpected(text.error());value.composing=std::move(*text);
        const auto cursor=reader(win32_ime_cursor,nullptr,0);
        if(cursor>=0)value.cursor=std::size_t(cursor);
        else if(flags&win32_ime_cursor)return core::fail(core::ErrorCode::failed,"Windows IME cursor is unavailable");
    }
    if(auto valid=win32_ime_validate(value);!valid)return std::unexpected(valid.error());
    return value;
}

// One native composition belongs to one focused editor. A versioned focus
// identity survives content changes but not retirement or a focus ABA.
class Win32ImeProtocol final {
    std::optional<Win32TextOwner> owner_;
    bool invalidated_=false,busy_=false;

    template<class Host> [[nodiscard]] core::Result<WidgetSnapshot> focused(Host& host){
        auto value=host.focus();
        if(!value||!value->focused||!value->text_input||!value->visible||!value->enabled||value->input_blocked)
            return core::fail(core::ErrorCode::closed,"Windows IME has no focused editor");
        if(value->text_revision&&!value->text_revision->identity)
            return core::fail(core::ErrorCode::invalid_argument,"Windows IME revision has no identity");
        return std::move(*value);
    }
    template<class Host> [[nodiscard]] core::Result<WidgetSnapshot> target(Host& host){
        if(invalidated_)return core::fail(core::ErrorCode::closed,"Windows IME focus generation expired");
        auto value=focused(host);
        if(!value){if(owner_)invalidated_=true;return std::unexpected(value.error());}
        if(!owner_)owner_=win32_text_owner(*value);
        else if(*owner_!=win32_text_owner(*value)){invalidated_=true;return core::fail(core::ErrorCode::closed,"Windows IME editor changed");}
        return value;
    }
    template<class Host> [[nodiscard]] core::Result<void> send(Host& host,WidgetInput event){
        auto value=target(host);if(!value)return std::unexpected(value.error());
        if(value->read_only)return core::fail(core::ErrorCode::unsupported,"Windows IME editor is read-only");
        event.target=value->id;event.text_service=true;event.expected_text_revision=value->text_revision;
        return host.send(std::move(event));
    }
public:
    void abandon() noexcept {owner_.reset();invalidated_=true;}
    template<class Host> [[nodiscard]] core::Result<void> start(Host& host){
        if(busy_)return core::fail(core::ErrorCode::busy,"Windows IME is delivering an edit");
        owner_.reset();invalidated_=false;auto value=target(host);
        if(!value){invalidated_=true;return std::unexpected(value.error());}return {};
    }
    template<class Host> [[nodiscard]] core::Result<void> apply(Host& host,const Win32ImeMessage& message){
        if(busy_)return core::fail(core::ErrorCode::busy,"Windows IME is delivering an edit");
        if(auto valid=win32_ime_validate(message);!valid)return valid;
        if(!message.cancel&&!message.committed&&!message.composing)return {};
        busy_=true;struct Reset final{bool& value;~Reset(){value=false;}} reset{busy_};
        if(message.cancel){
            auto value=target(host);if(!value)return std::unexpected(value.error());
            if(!value->marked_range)return {};
            WidgetInput event;event.kind=WidgetInputKind::composition;return send(host,std::move(event));
        }
        if(message.committed){
            auto text=win32_ime_utf8(*message.committed);if(!text)return std::unexpected(text.error());
            WidgetInput event;event.kind=WidgetInputKind::text;event.text=std::move(*text);event.text_cursor_offset=1;
            if(auto delivered=send(host,std::move(event));!delivered)return delivered;
        }
        if(message.composing){
            auto text=win32_ime_utf8(*message.composing);if(!text)return std::unexpected(text.error());
            WidgetInput event;event.kind=WidgetInputKind::composition;event.text=std::move(*text);
            event.composition_selection=TextRange{message.cursor.value_or(message.composing->size()),0};
            if(event.text.empty())event.text_cursor_offset=1;
            return send(host,std::move(event));
        }
        return {};
    }
    template<class Host> [[nodiscard]] core::Result<void> end(Host& host){
        if(busy_)return core::fail(core::ErrorCode::busy,"Windows IME is delivering an edit");
        core::Result<void> result;
        if(owner_&&!invalidated_){Win32ImeMessage cancel;cancel.cancel=true;result=apply(host,cancel);}
        owner_.reset();invalidated_=false;return result;
    }
};

// WM_CHAR delivers one UTF-16 code unit at a time. A pending surrogate is
// never combined across a known editor/focus generation.
class Win32Utf16Input final {
    std::optional<Win32TextOwner> owner_;
    char16_t pending_=0;
public:
    void clear() noexcept {pending_=0;owner_.reset();}
    [[nodiscard]] core::Result<std::optional<std::string>> accept(char16_t value,const std::optional<WidgetSnapshot>& editor){
        const auto owner=editor?std::optional{win32_text_owner(*editor)}:std::nullopt;
        if(pending_&&owner_!=owner)clear();
        if(value<32){clear();return std::optional<std::string>{};}
        if(value>=0xd800&&value<=0xdbff){pending_=value;owner_=owner;return std::optional<std::string>{};}
        if(value>=0xdc00&&value<=0xdfff){
            if(!pending_)return std::optional<std::string>{};
            const std::array<char16_t,2> pair{pending_,value};clear();
            auto text=win32_ime_utf8(std::u16string_view(pair.data(),pair.size()));if(!text)return std::unexpected(text.error());
            return std::optional<std::string>{std::move(*text)};
        }
        clear();auto text=win32_ime_utf8(std::u16string_view(&value,1));if(!text)return std::unexpected(text.error());
        return std::optional<std::string>{std::move(*text)};
    }
};

} // namespace ii::gui::detail
