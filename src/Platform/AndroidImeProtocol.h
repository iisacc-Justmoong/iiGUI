#pragma once

#include <iiGUI/NativeWidgets.h>
#include <iiSDKCore/Core/Text/Text.h>
#include <algorithm>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace ii::gui::detail {

enum class AndroidImeQuery { selected=0,before=1,after=2 };

struct AndroidImeState final {
    std::uint64_t generation=0;
    std::string target;
    std::size_t length=0;
    TextRange selection;
    std::optional<TextRange> marked;
    bool selection_reversed=false,read_only=false,multiline=false,password=false;
    std::optional<TextInputRevision> revision;
};

// Owner-thread adapter shared by the JNI bridge and headless widget tests.
// The host supplies focused snapshots, bounded reads and checked input delivery.
class AndroidImeProtocol final {
    struct Identity final {
        std::string target;
        std::shared_ptr<const void> owner;
        std::uint64_t focus=0;
        bool read_only=false,multiline=false,password=false;
        friend bool operator==(const Identity&,const Identity&)=default;
    };
    std::optional<Identity> identity_;
    std::uint64_t generation_=0;
    bool exhausted_=false;

    static bool same_editing_state(const AndroidImeState& left,const AndroidImeState& right){
        return left.generation==right.generation&&left.target==right.target&&left.length==right.length&&
            left.selection==right.selection&&left.marked==right.marked&&left.selection_reversed==right.selection_reversed&&
            left.revision==right.revision;
    }

    template<class Host>
    [[nodiscard]] core::Result<AndroidImeState> current(Host& host,std::uint64_t expected){
        auto focused=host.focus();
        if(focused&&(!focused->focused||!focused->text_input||!focused->enabled||!focused->visible||focused->input_blocked))focused.reset();
        if(focused&&focused->text_revision&&!focused->text_revision->identity)
            return core::fail(core::ErrorCode::invalid_argument,"Android text revision has no owner identity");
        std::optional<Identity> identity;
        if(focused)identity=Identity{focused->id,focused->text_revision?focused->text_revision->identity:nullptr,
            focused->text_revision?focused->text_revision->focus:0,focused->read_only,focused->multiline,focused->password};
        if(identity!=identity_){
            if(generation_==std::uint64_t((std::numeric_limits<std::int64_t>::max)()))exhausted_=true;
            else ++generation_;
            identity_=std::move(identity);
        }
        if(exhausted_)return core::fail(core::ErrorCode::closed,"Android text-service generation exhausted");
        if(!focused)return core::fail(core::ErrorCode::unsupported,"No focused Android text editor");
        if(expected&&expected!=generation_)return core::fail(core::ErrorCode::closed,"Android text-service focus expired");
        const auto range=focused->text_selection;
        if(range.location>focused->text_length||range.length>focused->text_length-range.location||
           (focused->text_selection_reversed&&!range.length))
            return core::fail(core::ErrorCode::invalid_argument,"Android text editor returned an invalid selection");
        if(focused->marked_range&&(focused->marked_range->location>focused->text_length||
           focused->marked_range->length>focused->text_length-focused->marked_range->location))
            return core::fail(core::ErrorCode::invalid_argument,"Android text editor returned an invalid composition");
        return AndroidImeState{generation_,focused->id,focused->text_length,range,focused->marked_range,
            focused->text_selection_reversed,focused->read_only,focused->multiline,focused->password,focused->text_revision};
    }
    template<class Host>
    [[nodiscard]] core::Result<void> send(Host& host,std::uint64_t generation,WidgetInput event,bool edits,
                                          const AndroidImeState* expected=nullptr){
        auto before=current(host,generation);if(!before)return std::unexpected(before.error());
        if(!before->revision)return core::fail(core::ErrorCode::unsupported,"Android text input requires a provider revision");
        if(expected&&!same_editing_state(*before,*expected))
            return core::fail(core::ErrorCode::busy,"Android text selection changed during the request");
        if(edits&&before->read_only)return core::fail(core::ErrorCode::unsupported,"Android text editor is read-only");
        event.target=before->target;event.text_service=true;event.expected_text_revision=before->revision;
        auto delivered=host.send(std::move(event));if(!delivered)return delivered;
        auto after=current(host,generation);if(!after)return std::unexpected(after.error());return {};
    }
    template<class Host>
    [[nodiscard]] core::Result<std::size_t> scalar_edge(Host& host,std::uint64_t generation,std::size_t position,
            std::size_t length,std::size_t requested,bool backward,std::size_t& removed){
        constexpr std::size_t chunk=8192,limit=65536;
        const auto high=[](char16_t c){return c>=0xd800&&c<=0xdbff;};
        const auto low=[](char16_t c){return c>=0xdc00&&c<=0xdfff;};
        while(requested&&(backward?position>0:position<length)){
            const auto units=requested>=chunk/2?chunk:requested*2;
            auto first=backward?position-std::min(position,units):position;
            auto last=backward?position:position+std::min(length-position,units);
            auto part=read(host,generation,{first,last-first});
            if(!part&&part.error().code==core::ErrorCode::out_of_range){
                if(backward&&first>0){--first;part=read(host,generation,{first,last-first});}
                else if(!backward&&last<length){++last;part=read(host,generation,{first,last-first});}
            }
            if(!part)return std::unexpected(part.error());
            auto text=core::to_utf16(part->text);
            if(!text||text->size()!=last-first)
                return core::fail(core::ErrorCode::invalid_argument,"Android editor returned invalid text");
            auto offset=backward?text->size():std::size_t(0);
            while(requested&&(backward?offset>0:offset<text->size())){
                std::size_t width=1;
                if(backward){
                    const auto value=(*text)[--offset];
                    if(low(value)){if(!offset||!high((*text)[offset-1]))return core::fail(core::ErrorCode::invalid_argument,"Invalid UTF-16 pair");--offset;width=2;}
                    else if(high(value))return core::fail(core::ErrorCode::invalid_argument,"Invalid UTF-16 pair");
                }else{
                    const auto value=(*text)[offset++];
                    if(high(value)){if(offset>=text->size()||!low((*text)[offset]))return core::fail(core::ErrorCode::invalid_argument,"Invalid UTF-16 pair");++offset;width=2;}
                    else if(low(value))return core::fail(core::ErrorCode::invalid_argument,"Invalid UTF-16 pair");
                }
                if(width>limit-removed)return core::fail(core::ErrorCode::out_of_range,"Android deletion exceeds its bound");
                removed+=width;position=backward?position-width:position+width;--requested;
            }
        }
        return position;
    }
    template<class Host>
    [[nodiscard]] core::Result<void> erase(Host& host,std::uint64_t generation,std::int32_t before,std::int32_t after,bool codepoints){
        if(before<0||after<0)return core::fail(core::ErrorCode::invalid_argument,"Android deletion lengths cannot be negative");
        auto editor=current(host,generation);if(!editor)return std::unexpected(editor.error());
        if(editor->read_only)return core::fail(core::ErrorCode::unsupported,"Android text editor is read-only");
        const auto selection=editor->selection;
        const auto left=selection.location,right=left+selection.length;std::size_t first,last;
        if(codepoints){
            std::size_t removed=0;
            auto start=scalar_edge(host,generation,left,editor->length,std::size_t(before),true,removed);
            if(!start)return std::unexpected(start.error());
            auto end=scalar_edge(host,generation,right,editor->length,std::size_t(after),false,removed);
            if(!end)return std::unexpected(end.error());first=*start;last=*end;
        }else{
            first=left-std::min(left,std::size_t(before));
            last=right+std::min(editor->length-right,std::size_t(after));
            if((left-first)+(last-right)>65536)return core::fail(core::ErrorCode::out_of_range,"Android deletion exceeds its bound");
        }
        auto a=read(host,generation,{first,0});if(!a)return std::unexpected(a.error());
        auto b=read(host,generation,{last,0});if(!b)return std::unexpected(b.error());
        if(first==left&&last==right){
            auto latest=current(host,generation);if(!latest)return std::unexpected(latest.error());
            if(!same_editing_state(*latest,*editor))return core::fail(core::ErrorCode::busy,"Android text selection changed during the request");
            return {};
        }
        if(editor->marked){
            WidgetInput event;event.kind=WidgetInputKind::text_surrounding_delete;
            event.replacement_range=TextRange{first,last-first};
            return send(host,generation,std::move(event),true,&*editor);
        }
        WidgetInput event;event.kind=WidgetInputKind::text;
        if(!selection.length){event.replacement_range=TextRange{first,last-first};event.text_cursor_offset=1;}
        else{
            if(first<left&&last>right){
                auto retained=read(host,generation,selection);if(!retained)return std::unexpected(retained.error());
                if(retained->text.size()>65536)return core::fail(core::ErrorCode::out_of_range,"Selected text exceeds the Android edit bound");
                event.text=std::move(retained->text);event.replacement_range=TextRange{first,last-first};
            }else if(first<left)event.replacement_range=TextRange{first,left-first};
            else event.replacement_range=TextRange{right,last-right};
            event.text_selection_after=TextRange{first,selection.length};event.selection_reversed=editor->selection_reversed;
        }
        return send(host,generation,std::move(event),true,&*editor);
    }
public:
    template<class Host> [[nodiscard]] core::Result<AndroidImeState> state(Host& host,std::uint64_t expected=0){return current(host,expected);}
    template<class Host> [[nodiscard]] core::Result<TextContent> read(Host& host,std::uint64_t generation,TextRange range){
        auto editor=current(host,generation);if(!editor)return std::unexpected(editor.error());
        if(!editor->revision)return core::fail(core::ErrorCode::unsupported,"Android text input requires a provider revision");
        if(range.location>editor->length||range.length>editor->length-range.location||range.length>65536)
            return core::fail(core::ErrorCode::out_of_range,"Android text-service read exceeds its bound");
        auto text=host.read(editor->target,range);if(!text)return std::unexpected(text.error());
        if(text->range!=range||text->text.size()>4*65536)
            return core::fail(core::ErrorCode::invalid_argument,"Android editor returned a partial text range");
        auto decoded=core::to_utf16(text->text);
        if(!decoded||decoded->size()!=range.length)
            return core::fail(core::ErrorCode::invalid_argument,"Android editor returned invalid UTF-16 text");
        auto after=current(host,generation);if(!after)return std::unexpected(after.error());
        if(!same_editing_state(*after,*editor))return core::fail(core::ErrorCode::busy,"Android text changed during a read");
        return text;
    }
    // Read the selection and its text in one owner-thread request. Android
    // counts UTF-16 code units and permits a returned slice to split a pair.
    template<class Host> [[nodiscard]] core::Result<std::optional<std::u16string>> query_text(
            Host& host,std::uint64_t generation,AndroidImeQuery query,std::int32_t requested){
        if(requested<0||static_cast<unsigned>(query)>2)
            return core::fail(core::ErrorCode::invalid_argument,"Invalid Android text query");
        auto editor=current(host,generation);if(!editor)return std::unexpected(editor.error());
        if(!editor->revision)return core::fail(core::ErrorCode::unsupported,"Android text input requires a provider revision");
        const auto count=std::min(std::size_t(requested),std::size_t(65535));
        std::size_t first=editor->selection.location,last=first+editor->selection.length;
        if(query==AndroidImeQuery::selected){
            if(first==last||last-first>count)return std::optional<std::u16string>{};
        }else if(query==AndroidImeQuery::before){last=first;first-=std::min(first,count);}
        else {first=last;last+=std::min(editor->length-last,count);}
        auto value=read(host,generation,{first,last-first});
        bool trim_first=false,trim_last=false;
        if(!value&&value.error().code==core::ErrorCode::out_of_range){
            if(query==AndroidImeQuery::before&&first>0){--first;trim_first=true;}
            else if(query==AndroidImeQuery::after&&last<editor->length){++last;trim_last=true;}
            if(trim_first||trim_last)value=read(host,generation,{first,last-first});
        }
        if(!value)return std::unexpected(value.error());
        auto text=core::to_utf16(value->text);
        if(!text||text->size()!=last-first)return core::fail(core::ErrorCode::invalid_argument,"Android editor returned invalid text");
        if(trim_first)text->erase(text->begin());if(trim_last)text->pop_back();
        auto latest=current(host,generation);if(!latest)return std::unexpected(latest.error());
        if(!same_editing_state(*latest,*editor))return core::fail(core::ErrorCode::busy,"Android text selection changed during the query");
        return std::optional<std::u16string>{std::move(*text)};
    }
    template<class Host> [[nodiscard]] core::Result<void> select(Host& host,std::uint64_t generation,std::int32_t anchor,std::int32_t caret){
        if(anchor<0||caret<0)return core::fail(core::ErrorCode::invalid_argument,"Android selection cannot be negative");
        const auto first=std::size_t(std::min(anchor,caret)),last=std::size_t(std::max(anchor,caret));
        WidgetInput event;event.kind=WidgetInputKind::text_selection;event.replacement_range=TextRange{first,last-first};event.selection_reversed=anchor>caret;
        return send(host,generation,std::move(event),false);
    }
    template<class Host> [[nodiscard]] core::Result<void> composing_region(Host& host,std::uint64_t generation,std::int32_t start,std::int32_t end){
        auto editor=current(host,generation);if(!editor)return std::unexpected(editor.error());
        if(editor->read_only)return core::fail(core::ErrorCode::unsupported,"Android text editor is read-only");
        const auto clip=[&](std::int32_t value){return std::min(editor->length,std::size_t(std::max<std::int32_t>(0,value)));};
        const auto a=clip(start),b=clip(end),first=std::min(a,b),last=std::max(a,b);
        if(last-first>65536)return core::fail(core::ErrorCode::out_of_range,"Android composition region exceeds its bound");
        if(first!=last){
            auto content=read(host,generation,{first,last-first});if(!content)return std::unexpected(content.error());
            if(content->text.size()>65536)return core::fail(core::ErrorCode::out_of_range,"Android composition text exceeds its byte bound");
        }
        WidgetInput event;event.kind=WidgetInputKind::text_composition_region;event.replacement_range=TextRange{first,last-first};
        return send(host,generation,std::move(event),true,&*editor);
    }
    template<class Host> [[nodiscard]] core::Result<void> composition(Host& host,std::uint64_t generation,std::string text,std::int32_t cursor){
        WidgetInput event;event.kind=WidgetInputKind::composition;event.text=std::move(text);event.text_cursor_offset=cursor;
        return send(host,generation,std::move(event),true);
    }
    template<class Host> [[nodiscard]] core::Result<void> commit(Host& host,std::uint64_t generation,std::string text,std::int32_t cursor){
        WidgetInput event;event.kind=WidgetInputKind::text;event.text=std::move(text);event.text_cursor_offset=cursor;
        return send(host,generation,std::move(event),true);
    }
    template<class Host> [[nodiscard]] core::Result<void> finish(Host& host,std::uint64_t generation){
        auto editor=current(host,generation);if(!editor)return std::unexpected(editor.error());
        if(!editor->revision)return core::fail(core::ErrorCode::unsupported,"Android text input requires a provider revision");
        if(!editor->marked)return {};
        WidgetInput event;event.kind=WidgetInputKind::commit_composition;return send(host,generation,std::move(event),true);
    }
    template<class Host> [[nodiscard]] core::Result<void> erase_surrounding(Host& host,std::uint64_t generation,std::int32_t before,std::int32_t after){
        return erase(host,generation,before,after,false);
    }
    template<class Host> [[nodiscard]] core::Result<void> erase_surrounding_codepoints(Host& host,std::uint64_t generation,std::int32_t before,std::int32_t after){
        return erase(host,generation,before,after,true);
    }
};
} // namespace ii::gui::detail
