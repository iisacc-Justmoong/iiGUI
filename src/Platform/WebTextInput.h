#pragma once
#include <iiGUI/NativeWidgets.h>
#include <iiSDKCore/Core/Text/Utf8.h>
#include <cstdint>
#include <optional>
#include <string>

namespace ii::gui::detail {
// Browser callbacks contain only bounded data and are delivered on the host
// owner thread. Sequence numbers acknowledge events without blocking the DOM.
enum class WebTextAction : int { replace, composition, commit, cancel, selection, tab, enter, escape, blur, refresh };
struct WebTextMessage final {
    WebTextAction action=WebTextAction::refresh;
    std::string id, text, before;
    std::uint32_t generation=0, sequence=0;
    std::size_t start=0, length=0, selection_start=0, selection_length=0;
    bool has_before=false, first_composition=false, selection_reversed=false, shift=false;
};
inline constexpr std::size_t web_text_limit=65536;
[[nodiscard]] inline bool web_text_representable(bool password,bool multiline,std::string_view text) noexcept {
    return !(password&&multiline) && text.find('\r')==text.npos && (multiline||text.find('\n')==text.npos);
}

[[nodiscard]] inline bool web_text_same_focus(const WidgetSnapshot& current,std::string_view id,
                                               const TextInputRevision& published) noexcept {
    return current.id==id && current.attached && current.visible && current.enabled && current.focused &&
           !current.input_blocked && current.text_input && current.text_revision &&
           current.text_revision->identity==published.identity && current.text_revision->focus==published.focus;
}
[[nodiscard]] inline bool web_text_admissible(const WebTextMessage& message,const WidgetSnapshot& current,
        std::string_view id,const TextInputRevision& published,std::string_view text) noexcept {
    if(message.id!=id || !web_text_same_focus(current,id,published))return false;
    const bool mutation=message.action==WebTextAction::replace||message.action==WebTextAction::composition||
                        message.action==WebTextAction::commit||message.action==WebTextAction::cancel;
    if(current.read_only && mutation)return false;
    if((mutation||message.action==WebTextAction::selection)&&!message.has_before)return false;
    return !message.has_before || message.before==text;
}

[[nodiscard]] inline std::optional<WidgetInput> web_text_input(const WebTextMessage& message) {
    const auto valid=[](const std::string& value) {
        return value.size()<=web_text_limit && value.find('\0')==value.npos && core::valid_utf8(value);
    };
    if(message.id.empty() || !valid(message.id) || !valid(message.text) || !valid(message.before) ||
       message.start>web_text_limit || message.length>web_text_limit-message.start ||
       message.selection_start>web_text_limit || message.selection_length>web_text_limit-message.selection_start ||
       (message.selection_reversed && !message.selection_length)) return {};
    WidgetInput result;
    result.target=message.id;
    result.shift=message.shift;
    switch(message.action) {
    case WebTextAction::replace:
        result.kind=WidgetInputKind::text;result.text=message.text;result.text_service=true;
        result.replacement_range=TextRange{message.start,message.length};
        result.text_selection_after=TextRange{message.selection_start,message.selection_length};
        result.selection_reversed=message.selection_reversed;break;
    case WebTextAction::composition:
        result.kind=WidgetInputKind::composition;result.text=message.text;result.text_service=true;
        if(message.first_composition)result.replacement_range=TextRange{message.start,message.length};
        if(message.text.empty())result.text_cursor_offset=1; // Empty preedit is distinct from cancellation.
        else result.composition_selection=TextRange{message.selection_start,message.selection_length};
        result.selection_reversed=message.selection_reversed;break;
    case WebTextAction::commit:result.kind=WidgetInputKind::commit_composition;result.text_service=true;break;
    case WebTextAction::cancel:result.kind=WidgetInputKind::composition;result.text_service=true;break;
    case WebTextAction::selection:
        result.kind=WidgetInputKind::text_selection;result.text_service=true;
        result.replacement_range=TextRange{message.selection_start,message.selection_length};
        result.selection_reversed=message.selection_reversed;break;
    // Keyboard navigation must traverse the whole focused hierarchy, including
    // content hosts and popovers. The sender is validated before routing.
    case WebTextAction::tab:result.kind=WidgetInputKind::key_down;result.key=WidgetKey::tab;result.target.clear();break;
    case WebTextAction::enter:result.kind=WidgetInputKind::key_down;result.key=WidgetKey::enter;result.target.clear();break;
    case WebTextAction::escape:result.kind=WidgetInputKind::key_down;result.key=WidgetKey::escape;result.target.clear();break;
    case WebTextAction::blur:result.kind=WidgetInputKind::focus_lost;result.target.clear();break;
    case WebTextAction::refresh:return {};
    default:return {};
    }
    return result;
}
}
