#pragma once

#include "WaylandImeEvents.h"
#include <iiGUI/NativeWidgets.h>
#include <iiGUI/TextBoundary.h>
#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>

namespace ii::gui::detail {

struct WaylandImeOwner final {
    std::string id;
    std::shared_ptr<const void> identity;
    std::uint64_t focus=0;
    friend bool operator==(const WaylandImeOwner&,const WaylandImeOwner&)=default;
};

// This adapter only operates on versioned text providers: Wayland batches can
// arrive after focus has moved away and back, so a widget ID alone is unsafe.
class WaylandImeProtocol final {
    std::optional<WaylandImeOwner> owner_;
    bool invalidated_=true,busy_=false;
    static constexpr std::size_t context_limit=4000;

    template<class Host> [[nodiscard]] core::Result<WidgetSnapshot> target(Host& host){
        if(invalidated_)return core::fail(core::ErrorCode::closed,"Wayland input focus expired");
        auto value=host.focus();
        if(!value||!value->focused||!value->text_input||!value->visible||!value->enabled||value->input_blocked||value->read_only){
            if(owner_)invalidated_=true;
            return core::fail(core::ErrorCode::closed,"Wayland input has no editable focus");
        }
        if(!value->text_revision||!value->text_revision->identity)
            return core::fail(core::ErrorCode::unsupported,"Wayland text input requires a versioned editor");
        const WaylandImeOwner current{value->id,value->text_revision->identity,value->text_revision->focus};
        if(!owner_)owner_=current;
        else if(*owner_!=current){invalidated_=true;return core::fail(core::ErrorCode::closed,"Wayland input editor changed");}
        const auto valid=[&](TextRange range){return range.location<=value->text_length&&range.length<=value->text_length-range.location;};
        if(!valid(value->text_selection)||(value->marked_range&&!valid(*value->marked_range)))
            return core::fail(core::ErrorCode::invalid_argument,"Invalid Wayland editor range");
        return std::move(*value);
    }
    template<class Host> [[nodiscard]] core::Result<TextContent> read(Host& host,const WidgetSnapshot& before,TextRange range){
        auto content=host.read(before.id,range);if(!content)return std::unexpected(content.error());
        if(content->range!=range)return core::fail(core::ErrorCode::invalid_argument,"Wayland editor returned a partial range");
        auto index=TextIndex::create(content->text);
        if(!index||index->utf16_size()!=range.length)
            return core::fail(core::ErrorCode::invalid_argument,"Wayland editor returned invalid text");
        auto after=target(host);if(!after)return std::unexpected(after.error());
        if(after->text_revision!=before.text_revision||after->text_length!=before.text_length||
           after->text_selection!=before.text_selection||after->marked_range!=before.marked_range||
           after->text_selection_reversed!=before.text_selection_reversed)
            return core::fail(core::ErrorCode::busy,"Wayland editor changed during text query");
        return content;
    }
    // Truncate only the outer edge when the requested unit budget lands in a
    // surrogate pair; the caret/selection edge must remain exact.
    template<class Host> [[nodiscard]] core::Result<TextContent> left_context(Host& host,const WidgetSnapshot& value,std::size_t edge,std::size_t units){
        auto start=edge-std::min(edge,units);auto content=read(host,value,{start,edge-start});
        if(!content&&content.error().code==core::ErrorCode::out_of_range&&start<edge)
            content=read(host,value,{start+1,edge-start-1});
        return content;
    }
    template<class Host> [[nodiscard]] core::Result<TextContent> right_context(Host& host,const WidgetSnapshot& value,std::size_t edge,std::size_t units){
        auto end=edge+std::min(value.text_length-edge,units);auto content=read(host,value,{edge,end-edge});
        if(!content&&content.error().code==core::ErrorCode::out_of_range&&end>edge)
            content=read(host,value,{edge,end-edge-1});
        return content;
    }
    template<class Host> [[nodiscard]] core::Result<void> send(Host& host,WidgetInput input,const WidgetSnapshot& expected){
        auto value=target(host);if(!value)return std::unexpected(value.error());
        if(value->text_revision!=expected.text_revision||value->text_length!=expected.text_length||
           value->text_selection!=expected.text_selection||value->marked_range!=expected.marked_range||
           value->text_selection_reversed!=expected.text_selection_reversed)
            return core::fail(core::ErrorCode::busy,"Wayland editor changed before text delivery");
        input.target=value->id;input.text_service=true;input.expected_text_revision=expected.text_revision;
        return host.send(std::move(input));
    }
    static bool continuation(char value){return (static_cast<unsigned char>(value)&0xc0)==0x80;}
public:
    void abandon() noexcept {owner_.reset();invalidated_=true;}
    template<class Host> [[nodiscard]] core::Result<void> cancel(Host& host){
        if(busy_)return core::fail(core::ErrorCode::busy,"Wayland input is delivering an edit");
        auto value=target(host);if(!value)return std::unexpected(value.error());
        if(!value->marked_range)return {};
        busy_=true;struct Reset final{bool& value;~Reset(){value=false;}} reset{busy_};
        WidgetInput event;event.kind=WidgetInputKind::composition;
        return send(host,std::move(event),*value);
    }
    template<class Host> [[nodiscard]] core::Result<void> start(Host& host){
        if(busy_)return core::fail(core::ErrorCode::busy,"Wayland input is delivering an edit");
        owner_.reset();invalidated_=false;auto value=target(host);
        if(!value){invalidated_=true;return std::unexpected(value.error());}return {};
    }
    template<class Host> [[nodiscard]] core::Result<std::optional<WaylandSurrounding>> surrounding(Host& host){
        auto value=target(host);if(!value)return std::unexpected(value.error());
        if(value->password)return std::optional<WaylandSurrounding>{};
        const auto range=value->marked_range.value_or(value->text_selection);
        if(!value->marked_range&&range.length>context_limit)return std::optional<WaylandSurrounding>{};
        const auto right=range.location+range.length;
        auto left=left_context(host,*value,range.location,context_limit);if(!left)return std::unexpected(left.error());
        auto tail=right_context(host,*value,right,context_limit);if(!tail)return std::unexpected(tail.error());
        std::string selected;
        if(!value->marked_range&&range.length){auto content=read(host,*value,range);if(!content)return std::unexpected(content.error());selected=std::move(content->text);}
        if(selected.size()>context_limit||left->text.find('\0')!=std::string::npos||
           selected.find('\0')!=std::string::npos||tail->text.find('\0')!=std::string::npos)
            return std::optional<WaylandSurrounding>{};
        auto room=context_limit-selected.size();
        auto before=std::min(left->text.size(),room/2),after=std::min(tail->text.size(),room-before);
        before=std::min(left->text.size(),room-after);
        auto first=left->text.size()-before,last=after;
        while(first<left->text.size()&&continuation(left->text[first]))++first;
        while(last<tail->text.size()&&last>0&&continuation(tail->text[last]))--last;
        WaylandSurrounding result;
        result.text=left->text.substr(first)+selected+tail->text.substr(0,last);
        const auto start=left->text.size()-first;
        const bool reversed=!value->marked_range&&value->text_selection_reversed;
        result.cursor=static_cast<std::int32_t>(start+(reversed?0:selected.size()));
        result.anchor=static_cast<std::int32_t>(start+(reversed?selected.size():0));
        return std::optional<WaylandSurrounding>{std::move(result)};
    }
    template<class Host> [[nodiscard]] core::Result<void> apply(Host& host,const WaylandImeBatch& batch){
        if(busy_)return core::fail(core::ErrorCode::busy,"Wayland input is delivering an edit");
        const auto check_text=[](const std::optional<std::string>& text)->core::Result<void>{
            if(!text)return {};
            if(text->size()>65536||text->find('\0')!=std::string::npos||!TextIndex::create(*text))
                return core::fail(core::ErrorCode::invalid_argument,"Invalid Wayland input text");
            return {};
        };
        if(auto result=check_text(batch.committed);!result)return result;
        if(auto result=check_text(batch.preedit);!result)return result;
        std::optional<TextRange> preedit_selection;bool hidden=false,reversed=false;
        if(batch.preedit){
            auto index=TextIndex::create(*batch.preedit);if(!index)return std::unexpected(index.error());
            if(batch.cursor_begin==-1&&batch.cursor_end==-1){hidden=true;preedit_selection=TextRange{index->utf16_size(),0};}
            else{
                if(batch.cursor_begin<0||batch.cursor_end<0)return core::fail(core::ErrorCode::out_of_range,"Invalid Wayland preedit cursor");
                auto a=index->utf16_for_byte(std::size_t(batch.cursor_begin));auto b=index->utf16_for_byte(std::size_t(batch.cursor_end));
                if(!a||!b)return core::fail(core::ErrorCode::out_of_range,"Wayland preedit cursor splits a scalar");
                preedit_selection=TextRange{std::min(*a,*b),std::max(*a,*b)-std::min(*a,*b)};
                reversed=*a>*b;
            }
        }
        if(batch.before>context_limit||batch.after>context_limit)
            return core::fail(core::ErrorCode::out_of_range,"Wayland deletion exceeds its context bound");
        busy_=true;struct Reset final{bool& value;~Reset(){value=false;}} reset{busy_};
        auto value=target(host);if(!value)return std::unexpected(value.error());
        const auto range=value->marked_range.value_or(value->text_selection);
        const auto left=range.location,right=range.location+range.length;
        std::size_t first=left,last=right;
        if(batch.before){
            auto text=left_context(host,*value,left,std::size_t(batch.before)+1);if(!text)return std::unexpected(text.error());
            if(batch.before>text->text.size())return core::fail(core::ErrorCode::out_of_range,"Wayland deletion precedes text");
            auto index=TextIndex::create(text->text);if(!index)return std::unexpected(index.error());
            auto point=index->utf16_for_byte(text->text.size()-batch.before);if(!point)return std::unexpected(point.error());
            first=text->range.location+*point;
        }
        if(batch.after){
            auto text=right_context(host,*value,right,std::size_t(batch.after)+1);if(!text)return std::unexpected(text.error());
            if(batch.after>text->text.size())return core::fail(core::ErrorCode::out_of_range,"Wayland deletion exceeds text");
            auto index=TextIndex::create(text->text);if(!index)return std::unexpected(index.error());
            auto point=index->utf16_for_byte(batch.after);if(!point)return std::unexpected(point.error());
            last=right+*point;
        }
        const bool deleting=first!=left||last!=right;
        if(deleting){
            WidgetInput event;
            if(value->marked_range){event.kind=WidgetInputKind::text_surrounding_delete;event.replacement_range=TextRange{first,last-first};}
            else{
                event.kind=WidgetInputKind::text;
                if(value->text_selection.length){
                    if(first<left&&last>right){
                        if(value->text_selection.length>65536)return core::fail(core::ErrorCode::out_of_range,"Wayland selection exceeds edit bound");
                        auto text=read(host,*value,value->text_selection);if(!text)return std::unexpected(text.error());
                        event.text=std::move(text->text);event.replacement_range=TextRange{first,last-first};
                    }else if(first<left)event.replacement_range=TextRange{first,left-first};
                    else event.replacement_range=TextRange{right,last-right};
                    event.text_selection_after=TextRange{first,value->text_selection.length};
                    event.selection_reversed=value->text_selection_reversed;
                }else{event.replacement_range=TextRange{first,last-first};event.text_cursor_offset=1;}
            }
            if(auto result=send(host,std::move(event),*value);!result)return result;
        }
        auto current=deleting?target(host):value;if(!current)return std::unexpected(current.error());
        // A candidate is a projection over the original selected range. A
        // commit replaces it directly, so one composition remains one undo
        // step; an empty done removes that range instead of restoring it.
        if(batch.committed){WidgetInput event;event.kind=WidgetInputKind::text;event.text=*batch.committed;event.text_cursor_offset=1;
            if(auto result=send(host,std::move(event),*current);!result)return result;}
        else if(current->marked_range&&(!batch.preedit||batch.preedit->empty())){
            WidgetInput event;event.kind=WidgetInputKind::text;event.text_cursor_offset=1;
            if(auto result=send(host,std::move(event),*current);!result)return result;
        }
        if(batch.preedit&&!batch.preedit->empty()){
            if(batch.committed){current=target(host);if(!current)return std::unexpected(current.error());}
            WidgetInput event;event.kind=WidgetInputKind::composition;event.text=*batch.preedit;event.composition_selection=preedit_selection;
            event.selection_reversed=reversed;event.composition_cursor_hidden=hidden;
            if(auto result=send(host,std::move(event),*current);!result)return result;}
        return {};
    }
};
} // namespace ii::gui::detail
