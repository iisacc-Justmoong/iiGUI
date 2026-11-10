#pragma once

#include <iiGUI/NativeWidgets.h>
#include <iiGUI/TextBoundary.h>
#include <iiSDKCore/Core/Text/Text.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <utility>

namespace ii::gui::detail {

enum class UIKitTextDirection { left,right,up,down };

struct UIKitTextInputState final {
    std::uint64_t generation=0;
    std::string target;
    std::size_t length=0;
    TextRange selection;
    std::optional<TextRange> marked;
    bool reversed=false,read_only=false,multiline=false,password=false;
    TextInputRevision revision;
    Rect caret;
};

// The UIKit object and the headless tests use the same owner-thread adapter.
// Positions retain generation, not a raw widget pointer or a text snapshot.
class UIKitTextInputProtocol final {
    struct Identity final {
        std::string target;
        std::shared_ptr<const void> owner;
        std::uint64_t focus=0;
        friend bool operator==(const Identity&,const Identity&)=default;
    };
    std::optional<Identity> identity_;
    std::uint64_t generation_=0;
    bool exhausted_=false;

    [[nodiscard]] static bool same(const UIKitTextInputState& a,const UIKitTextInputState& b){
        return a.generation==b.generation&&a.target==b.target&&a.length==b.length&&
            a.selection==b.selection&&a.marked==b.marked&&a.reversed==b.reversed&&a.revision==b.revision;
    }
    template<class Host> [[nodiscard]] core::Result<UIKitTextInputState> current(Host& host,std::uint64_t expected){
        auto focused=host.focus();
        if(focused&&(!focused->focused||!focused->text_input||!focused->enabled||!focused->visible||focused->input_blocked))focused.reset();
        if(focused&&focused->text_revision&&!focused->text_revision->identity)
            return core::fail(core::ErrorCode::invalid_argument,"UIKit text revision has no owner identity");
        std::optional<Identity> identity;
        if(focused)identity=Identity{focused->id,focused->text_revision?focused->text_revision->identity:nullptr,
            focused->text_revision?focused->text_revision->focus:0};
        if(identity!=identity_){
            if(generation_==(std::numeric_limits<std::uint64_t>::max)())exhausted_=true;
            else ++generation_;
            identity_=std::move(identity);
        }
        if(exhausted_)return core::fail(core::ErrorCode::closed,"UIKit text input generation exhausted");
        if(!focused)return core::fail(core::ErrorCode::unsupported,"No focused UIKit text editor");
        if(expected&&expected!=generation_)return core::fail(core::ErrorCode::closed,"UIKit text input focus expired");
        if(!focused->text_revision)return core::fail(core::ErrorCode::unsupported,"UIKit text input requires a provider revision");
        const auto valid=[&](TextRange range){return range.location<=focused->text_length&&range.length<=focused->text_length-range.location;};
        if(!valid(focused->text_selection)||(focused->text_selection_reversed&&!focused->text_selection.length)||
           (focused->marked_range&&!valid(*focused->marked_range)))
            return core::fail(core::ErrorCode::invalid_argument,"UIKit editor returned an invalid text range");
        return UIKitTextInputState{generation_,focused->id,focused->text_length,focused->text_selection,focused->marked_range,
            focused->text_selection_reversed,focused->read_only,focused->multiline,focused->password,*focused->text_revision,focused->caret_bounds};
    }
    template<class Host> [[nodiscard]] core::Result<void> unchanged(Host& host,const UIKitTextInputState& before){
        auto after=current(host,before.generation);if(!after)return std::unexpected(after.error());
        if(!same(before,*after))return core::fail(core::ErrorCode::busy,"UIKit text changed during the request");
        return {};
    }
    template<class Host> [[nodiscard]] core::Result<TextContent> exact(Host& host,const UIKitTextInputState& before,TextRange range){
        auto content=host.read(before.target,range);
        if(auto valid=unchanged(host,before);!valid)return std::unexpected(valid.error());
        if(!content)return std::unexpected(content.error());
        if(content->range!=range||content->text.size()>4*65536)
            return core::fail(core::ErrorCode::invalid_argument,"UIKit editor returned a partial text range");
        auto decoded=core::to_utf16(content->text);
        if(!decoded||decoded->size()!=range.length)
            return core::fail(core::ErrorCode::invalid_argument,"UIKit editor returned invalid UTF-16 text");
        return content;
    }
    template<class Host> [[nodiscard]] core::Result<void> boundary(Host& host,const UIKitTextInputState& before,std::size_t position){
        if(position>before.length)return core::fail(core::ErrorCode::out_of_range,"UIKit text boundary exceeds the document");
        auto result=exact(host,before,{position,0});if(!result)return std::unexpected(result.error());return {};
    }
    template<class Host> [[nodiscard]] core::Result<void> send(Host& host,const UIKitTextInputState& expected,WidgetInput event,bool edit){
        auto before=current(host,expected.generation);if(!before)return std::unexpected(before.error());
        if(!same(expected,*before))return core::fail(core::ErrorCode::busy,"UIKit text changed before delivery");
        if(edit&&before->read_only)return core::fail(core::ErrorCode::unsupported,"UIKit text editor is read-only");
        event.target=before->target;event.text_service=true;event.expected_text_revision=before->revision;
        auto result=host.send(std::move(event));if(!result)return result;
        auto after=current(host,expected.generation);if(!after)return std::unexpected(after.error());return {};
    }
public:
    template<class Host> [[nodiscard]] core::Result<UIKitTextInputState> state(Host& host,std::uint64_t expected=0){return current(host,expected);}

    // NSString ranges count UTF-16 units. A returned slice may contain one
    // half of a pair even though all modifications require scalar boundaries.
    template<class Host> [[nodiscard]] core::Result<std::u16string> read(Host& host,std::uint64_t generation,TextRange range){
        auto before=current(host,generation);if(!before)return std::unexpected(before.error());
        if(range.location>before->length||range.length>before->length-range.location||range.length>65536)
            return core::fail(core::ErrorCode::out_of_range,"UIKit text read exceeds its bound");
        auto original=exact(host,*before,range);
        if(original){auto result=core::to_utf16(original->text);if(!result)return core::fail(core::ErrorCode::invalid_argument,"Invalid UIKit text encoding");return *result;}
        if(original.error().code!=core::ErrorCode::out_of_range)return std::unexpected(original.error());
        for(const auto [left,right]:{std::pair{std::size_t(1),std::size_t(0)},std::pair{std::size_t(0),std::size_t(1)},std::pair{std::size_t(1),std::size_t(1)}}){
            if(left>range.location||right>before->length-(range.location+range.length)||range.length+left+right>65536)continue;
            auto expanded=exact(host,*before,{range.location-left,range.length+left+right});
            if(!expanded){if(expanded.error().code==core::ErrorCode::out_of_range)continue;return std::unexpected(expanded.error());}
            auto text=core::to_utf16(expanded->text);if(!text)return core::fail(core::ErrorCode::invalid_argument,"Invalid UIKit text encoding");
            return text->substr(left,range.length);
        }
        return std::unexpected(original.error());
    }
    template<class Host> [[nodiscard]] core::Result<TextRange> character(Host& host,std::uint64_t generation,std::size_t position,bool forward){
        auto before=current(host,generation);if(!before)return std::unexpected(before.error());
        if(position>before->length)return core::fail(core::ErrorCode::out_of_range,"UIKit character position exceeds the document");
        if(auto valid=boundary(host,*before,position);!valid)return std::unexpected(valid.error());
        if((forward&&position==before->length)||(!forward&&!position))return TextRange{position,0};
        constexpr std::size_t radius=4096;
        auto first=position-std::min(position,radius),last=position+std::min(before->length-position,radius);
        auto text=exact(host,*before,{first,last-first});
        if(!text&&text.error().code==core::ErrorCode::out_of_range){
            bool found=false;
            for(const auto [left,right]:{std::pair{std::size_t(1),std::size_t(0)},std::pair{std::size_t(0),std::size_t(1)},std::pair{std::size_t(1),std::size_t(1)}}){
                if(left>first||right>before->length-last)continue;
                auto candidate=exact(host,*before,{first-left,last-first+left+right});
                if(!candidate){if(candidate.error().code==core::ErrorCode::out_of_range)continue;return std::unexpected(candidate.error());}
                first-=left;last+=right;text=std::move(candidate);found=true;break;
            }
            if(!found)return std::unexpected(text.error());
        }
        if(!text)return std::unexpected(text.error());
        auto index=TextIndex::create(std::move(text->text));if(!index)return std::unexpected(index.error());
        const auto local=position-first;
        if(!index->byte_for_utf16(local))return core::fail(core::ErrorCode::out_of_range,"UIKit position splits a Unicode scalar");
        const auto previous=index->previous_grapheme(local);
        const auto containing_end=index->next_grapheme(previous);
        const bool interior=local>0&&containing_end>local;
        const auto edge=interior?(forward?containing_end:previous):(forward?index->next_grapheme(local):previous);
        if(((!forward||interior)&&!previous&&first)||((forward||interior)&&
           (interior?containing_end:edge)==index->utf16_size()&&last<before->length))
            return core::fail(core::ErrorCode::out_of_range,"UIKit grapheme exceeds the bounded context");
        const auto start=first+(interior?previous:std::min(local,edge)),end=first+(interior?containing_end:std::max(local,edge));
        return TextRange{start,end-start};
    }
    template<class Host> [[nodiscard]] core::Result<void> select(Host& host,std::uint64_t generation,TextRange range){
        auto before=current(host,generation);if(!before)return std::unexpected(before.error());
        if(range.location>before->length||range.length>before->length-range.location)
            return core::fail(core::ErrorCode::out_of_range,"UIKit selection exceeds the document");
        if(auto r=boundary(host,*before,range.location);!r)return r;
        if(auto r=boundary(host,*before,range.location+range.length);!r)return r;
        if(before->marked&&(range.location<before->marked->location||
           range.location+range.length>before->marked->location+before->marked->length)){
            if(before->marked->length>65536)return core::fail(core::ErrorCode::out_of_range,"UIKit candidate exceeds its text bound");
            auto candidate=exact(host,*before,*before->marked);if(!candidate)return std::unexpected(candidate.error());
            if(candidate->text.size()>65536)return core::fail(core::ErrorCode::out_of_range,"UIKit candidate exceeds its byte bound");
            WidgetInput event;event.kind=WidgetInputKind::text;event.text=std::move(candidate->text);
            event.replacement_range=*before->marked;event.text_selection_after=range;
            return send(host,*before,std::move(event),true);
        }
        WidgetInput event;event.kind=WidgetInputKind::text_selection;event.replacement_range=range;
        return send(host,*before,std::move(event),false);
    }
    template<class Host> [[nodiscard]] core::Result<void> compose(Host& host,std::uint64_t generation,std::string text,TextRange selected){
        auto before=current(host,generation);if(!before)return std::unexpected(before.error());
        if(text.size()>65536)return core::fail(core::ErrorCode::out_of_range,"UIKit composition exceeds its byte bound");
        auto units=core::to_utf16(text);if(!units)return core::fail(core::ErrorCode::invalid_argument,"Invalid UIKit composition encoding");
        if(units->size()>65536||selected.location>units->size()||selected.length>units->size()-selected.location)
            return core::fail(core::ErrorCode::out_of_range,"UIKit composition selection exceeds the candidate");
        const auto split=[&](std::size_t at){return at>0&&at<units->size()&&(*units)[at]>=0xdc00&&(*units)[at]<=0xdfff;};
        if(split(selected.location)||split(selected.location+selected.length))
            return core::fail(core::ErrorCode::out_of_range,"UIKit composition selection splits a Unicode scalar");
        WidgetInput event;event.kind=WidgetInputKind::composition;event.text=std::move(text);event.composition_selection=selected;
        if(event.text.empty())event.text_cursor_offset=1;
        return send(host,*before,std::move(event),true);
    }
    template<class Host> [[nodiscard]] core::Result<void> replace(Host& host,std::uint64_t generation,TextRange range,std::string text){
        auto before=current(host,generation);if(!before)return std::unexpected(before.error());
        if(text.size()>65536||!core::valid_utf8(text)||range.location>before->length||range.length>before->length-range.location)
            return core::fail(core::ErrorCode::out_of_range,"Invalid UIKit text replacement");
        if(auto r=boundary(host,*before,range.location);!r)return r;
        if(auto r=boundary(host,*before,range.location+range.length);!r)return r;
        WidgetInput event;event.kind=WidgetInputKind::text;event.text=std::move(text);event.replacement_range=range;event.text_cursor_offset=1;
        return send(host,*before,std::move(event),true);
    }
    template<class Host> [[nodiscard]] core::Result<void> insert(Host& host,std::uint64_t generation,std::string text){
        auto before=current(host,generation);if(!before)return std::unexpected(before.error());
        if(text.size()>65536||!core::valid_utf8(text))return core::fail(core::ErrorCode::invalid_argument,"Invalid UIKit text input");
        WidgetInput event;event.kind=WidgetInputKind::text;event.text=std::move(text);event.text_cursor_offset=1;
        return send(host,*before,std::move(event),true);
    }
    template<class Host> [[nodiscard]] core::Result<void> delete_backward(Host& host,std::uint64_t generation){
        auto before=current(host,generation);if(!before)return std::unexpected(before.error());
        if(before->read_only)return core::fail(core::ErrorCode::unsupported,"UIKit text editor is read-only");
        auto target=before->selection;
        if(!target.length){
            auto previous=character(host,generation,target.location,false);if(!previous)return std::unexpected(previous.error());
            target=*previous;if(!target.length)return {};
        }
        if(before->marked){
            const auto mark=*before->marked;const auto mark_end=mark.location+mark.length;
            if(!before->selection.length&&target.location<mark.location&&target.location+target.length==mark.location){
                WidgetInput event;event.kind=WidgetInputKind::text_surrounding_delete;event.replacement_range=target;
                return send(host,*before,std::move(event),true);
            }
            if(target.location<mark.location||target.location+target.length>mark_end)
                return core::fail(core::ErrorCode::unsupported,"UIKit deletion is outside marked text");
            auto candidate=read(host,generation,mark);if(!candidate)return std::unexpected(candidate.error());
            candidate->erase(target.location-mark.location,target.length);
            auto value=core::to_utf8(*candidate);if(!value)return core::fail(core::ErrorCode::invalid_argument,"UIKit deletion produced invalid Unicode");
            WidgetInput event;event.kind=WidgetInputKind::composition;event.text=std::move(*value);
            event.composition_selection=TextRange{target.location-mark.location,0};
            if(event.text.empty())event.text_cursor_offset=1;
            return send(host,*before,std::move(event),true);
        }
        if(auto r=boundary(host,*before,target.location);!r)return r;
        if(auto r=boundary(host,*before,target.location+target.length);!r)return r;
        WidgetInput event;event.kind=WidgetInputKind::text;event.replacement_range=target;event.text_cursor_offset=1;
        return send(host,*before,std::move(event),true);
    }
    template<class Host> [[nodiscard]] core::Result<void> finish(Host& host,std::uint64_t generation){
        auto before=current(host,generation);if(!before)return std::unexpected(before.error());
        if(!before->marked)return {};
        WidgetInput event;event.kind=WidgetInputKind::commit_composition;return send(host,*before,std::move(event),true);
    }
    template<class Host> [[nodiscard]] core::Result<TextInputGeometry> geometry(Host& host,std::uint64_t generation,TextRange range){
        auto before=current(host,generation);if(!before)return std::unexpected(before.error());
        if(range.location>before->length||range.length>before->length-range.location)
            return core::fail(core::ErrorCode::out_of_range,"UIKit geometry exceeds the document");
        auto value=host.geometry(before->target,range);
        if(auto checked=unchanged(host,*before);!checked)return std::unexpected(checked.error());
        return value;
    }
    template<class Host> [[nodiscard]] core::Result<std::size_t> position(Host& host,std::uint64_t generation,double x,double y){
        if(!std::isfinite(x)||!std::isfinite(y))return core::fail(core::ErrorCode::invalid_argument,"UIKit text point must be finite");
        auto before=current(host,generation);if(!before)return std::unexpected(before.error());
        auto value=host.position(before->target,x,y);
        if(auto checked=unchanged(host,*before);!checked)return std::unexpected(checked.error());
        if(!value)return std::unexpected(value.error());
        if(*value>before->length)return core::fail(core::ErrorCode::out_of_range,"UIKit hit exceeds the document");
        if(auto checked=boundary(host,*before,*value);!checked)return std::unexpected(checked.error());
        return *value;
    }
    template<class Host> [[nodiscard]] core::Result<std::size_t> move(Host& host,std::uint64_t generation,std::size_t at,UIKitTextDirection direction,std::size_t steps){
        auto before=current(host,generation);if(!before)return std::unexpected(before.error());
        if(at>before->length||steps>1024||unsigned(direction)>unsigned(UIKitTextDirection::down))
            return core::fail(core::ErrorCode::out_of_range,"Invalid UIKit directional movement");
        for(std::size_t step=0;step<steps;++step){
            if(direction==UIKitTextDirection::left||direction==UIKitTextDirection::right){
                const bool forward=direction==UIKitTextDirection::right;
                auto span=character(host,generation,at,forward);if(!span)return std::unexpected(span.error());
                if(!span->length)return core::fail(core::ErrorCode::out_of_range,"UIKit movement exceeds the document");
                at=span->location+(forward?span->length:0);
            }else{
                auto value=geometry(host,generation,{at,0});if(!value)return std::unexpected(value.error());
                const auto caret=value->start_caret;
                if(caret.width<=0||caret.height<=0)return core::fail(core::ErrorCode::unsupported,"UIKit caret is outside the visible viewport");
                const auto direction_sign=direction==UIKitTextDirection::down?1.:-1.;
                bool found=false;
                for(unsigned distance=1;distance<=8;++distance){
                    const auto y=double(caret.y)+double(caret.height)*(.5+direction_sign*double(distance));
                    auto next=position(host,generation,double(caret.x)+double(caret.width)/2,y);
                    if(!next)return std::unexpected(next.error());
                    if(*next!=at){at=*next;found=true;break;}
                }
                if(!found)return core::fail(core::ErrorCode::out_of_range,"UIKit movement has no adjacent visible line");
            }
            if(auto checked=unchanged(host,*before);!checked)return std::unexpected(checked.error());
        }
        return at;
    }
};
} // namespace ii::gui::detail
