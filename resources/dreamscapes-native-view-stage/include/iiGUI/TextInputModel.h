#pragma once
#include <iiGUI/TextBoundary.h>
#include <iiGUI/TextSurroundingPlan.h>
#include <iiSDKCore/Core/Text/Utf8.h>
#include <algorithm>
#include <cstddef>
#include <expected>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
namespace ii::gui {
struct TextEditRange final {std::size_t location=0,length=0;friend bool operator==(const TextEditRange&,const TextEditRange&)=default;};
enum class TextEditErrorCode {invalid_text,invalid_range,invalid_boundary,unavailable,too_large,composing};
enum class TextEditLengthUnit {utf16_code_units,unicode_scalars};
struct TextEditError final {TextEditErrorCode code;std::string message;};
template<class T>using TextEditResult=std::expected<T,TextEditError>;
class TextEditBoundary {public:virtual ~TextEditBoundary()=default;
 [[nodiscard]] virtual std::size_t length()const noexcept=0;[[nodiscard]] virtual std::optional<std::size_t> byte_offset(std::size_t)const=0;
 [[nodiscard]] virtual std::size_t previous_grapheme(std::size_t)const noexcept=0;[[nodiscard]] virtual std::size_t next_grapheme(std::size_t)const noexcept=0;
 [[nodiscard]] virtual std::size_t previous_word(std::size_t)const noexcept=0;[[nodiscard]] virtual std::size_t next_word(std::size_t)const noexcept=0;
 [[nodiscard]] virtual std::optional<TextEditRange> word_range(std::size_t)const noexcept{return std::nullopt;}};
using TextEditBoundaryFactory=std::function<TextEditResult<std::shared_ptr<const TextEditBoundary>>(std::string_view)>;
class TextIndexEditBoundary final:public TextEditBoundary {
 TextIndex index_;
public:explicit TextIndexEditBoundary(TextIndex index):index_(std::move(index)){}
 [[nodiscard]] std::size_t length()const noexcept override{return index_.utf16_size();}
 [[nodiscard]] std::optional<std::size_t> byte_offset(std::size_t unit)const override{auto offset=index_.byte_for_utf16(unit);return offset?std::optional<std::size_t>{*offset}:std::nullopt;}
 [[nodiscard]] std::size_t previous_grapheme(std::size_t n)const noexcept override{return index_.previous_grapheme(n);}
 [[nodiscard]] std::size_t next_grapheme(std::size_t n)const noexcept override{return index_.next_grapheme(n);}
 [[nodiscard]] std::size_t previous_word(std::size_t n)const noexcept override{return index_.previous_word(n);}
 [[nodiscard]] std::size_t next_word(std::size_t n)const noexcept override{return index_.next_word(n);}
 [[nodiscard]] std::optional<TextEditRange> word_range(std::size_t position)const noexcept override{
  const auto length=index_.utf16_size();if(position>length)return std::nullopt;if(!length)return TextEditRange{};
  auto at=position==length?index_.previous_grapheme(position):position;
  if(at&&index_.next_grapheme(index_.previous_grapheme(at))>at)at=index_.previous_grapheme(at);
  if(!index_.word_at(at))return TextEditRange{at,index_.next_grapheme(at)-at};
  const auto first=index_.previous_word(index_.next_grapheme(at));return TextEditRange{first,index_.next_word(at)-first};}
};
[[nodiscard]] inline TextEditResult<std::shared_ptr<const TextEditBoundary>> make_text_input_boundary(std::string_view value){
 try{auto index=TextIndex::create(std::string(value));if(!index)return std::unexpected(TextEditError{TextEditErrorCode::unavailable,index.error().message});
  return std::shared_ptr<const TextEditBoundary>(std::make_shared<TextIndexEditBoundary>(std::move(*index)));
 }catch(...){return std::unexpected(TextEditError{TextEditErrorCode::unavailable,"Cannot create native Unicode boundary"});}}
struct DefaultTextInputModelPolicy final {using Range=TextEditRange;using ErrorCode=TextEditErrorCode;using Error=TextEditError;
 using LengthUnit=TextEditLengthUnit;using Boundary=TextEditBoundary;using BoundaryFactory=TextEditBoundaryFactory;
 [[nodiscard]] static bool valid_text(std::string_view value)noexcept{return value.size()<=65536&&value.find('\0')==std::string_view::npos&&ii::core::valid_utf8(value);}};
// Owns the editing transaction; neither the Unicode service nor a platform
// text-input host owns its text, selection or in-progress IME composition.
template<class Policy>
class BasicTextInput final {
    using TextInputRange=typename Policy::Range;using TextInputErrorCode=typename Policy::ErrorCode;
    using TextInputError=typename Policy::Error;using TextLengthUnit=typename Policy::LengthUnit;
    using TextInputBoundary=typename Policy::Boundary;using TextInputBoundaryFactory=typename Policy::BoundaryFactory;
    template<class T>using TextInputResult=std::expected<T,TextInputError>;
    struct Original final {
        std::string text;
        std::shared_ptr<const TextInputBoundary> boundary;
        std::size_t anchor = 0, caret = 0;
    };
    struct Replacement final {
        std::string text;
        std::shared_ptr<const TextInputBoundary> boundary;
        std::size_t inserted_length = 0;
    };
    struct HistoryEntry final {
        std::shared_ptr<const std::string> text;
        std::size_t anchor = 0, caret = 0;
        std::shared_ptr<const HistoryEntry> older;
        std::size_t count = 1, bytes = 0;
    };
    using History = std::shared_ptr<const HistoryEntry>;
    TextInputBoundaryFactory factory_;
    std::string text_;
    std::shared_ptr<const TextInputBoundary> boundary_;
    std::size_t anchor_ = 0, caret_ = 0;
    std::optional<TextInputRange> marked_;
    std::optional<Original> original_;
    History undo_, redo_;
    bool history_enabled_ = true;
    std::optional<std::size_t> maximum_length_;
    TextLengthUnit length_unit_ = TextLengthUnit::utf16_code_units;

    explicit BasicTextInput(TextInputBoundaryFactory factory) : factory_(std::move(factory)) {}
    template<class T> [[nodiscard]] static TextInputResult<T> failure(TextInputErrorCode code, std::string message) {
        return std::unexpected(TextInputError{code,std::move(message)});
    }
    [[nodiscard]] TextInputResult<std::shared_ptr<const TextInputBoundary>> index(std::string_view text) const {
        if (text.size() > maximum_bytes)
            return failure<std::shared_ptr<const TextInputBoundary>>(TextInputErrorCode::too_large,"Text input exceeds 64 KiB");
        if (!Policy::valid_text(text))
            return failure<std::shared_ptr<const TextInputBoundary>>(TextInputErrorCode::invalid_text,"Text input must be NUL-free UTF-8");
        if (!factory_)
            return failure<std::shared_ptr<const TextInputBoundary>>(TextInputErrorCode::unavailable,"Text boundary service is unavailable");
        auto value = factory_(text);
        if (!value) return std::unexpected(value.error());
        if (!*value || (*value)->length() > text.size() || (*value)->byte_offset(0) != std::optional<std::size_t>{0} ||
            (*value)->byte_offset((*value)->length()) != std::optional<std::size_t>{text.size()})
            return failure<std::shared_ptr<const TextInputBoundary>>(TextInputErrorCode::invalid_boundary,"Text boundary service returned an invalid index");
        return value;
    }
    [[nodiscard]] TextInputResult<std::pair<std::size_t,std::size_t>> bytes(TextInputRange range) const {
        if (range.location > length() || range.length > length() - range.location)
            return failure<std::pair<std::size_t,std::size_t>>(TextInputErrorCode::invalid_range,"Text range exceeds the document");
        const auto first = boundary_->byte_offset(range.location), last = boundary_->byte_offset(range.location + range.length);
        if (!first || !last || *first > *last || *last > text_.size())
            return failure<std::pair<std::size_t,std::size_t>>(TextInputErrorCode::invalid_range,"Text range splits a Unicode scalar");
        return std::pair{*first,*last};
    }
    // Call only after validating the complete string as UTF-8. Counts are
    // independent of the host's grapheme segmentation and never split a scalar.
    [[nodiscard]] static std::size_t prefix_bytes(std::string_view text,std::size_t limit,TextLengthUnit unit) noexcept {
        std::size_t offset=0,used=0;
        while(offset<text.size()) {
            const auto lead=static_cast<unsigned char>(text[offset]);
            const std::size_t width=lead<0x80?1:lead<0xe0?2:lead<0xf0?3:4;
            const std::size_t cost=unit==TextLengthUnit::utf16_code_units&&width==4?2:1;
            if(cost>limit-used)break;
            used+=cost;offset+=width;
        }
        return offset;
    }
    [[nodiscard]] static std::size_t length_in(std::string_view text,TextLengthUnit unit) noexcept {
        std::size_t offset=0,count=0;
        while(offset<text.size()) {
            const auto lead=static_cast<unsigned char>(text[offset]);
            const std::size_t width=lead<0x80?1:lead<0xe0?2:lead<0xf0?3:4;
            count+=unit==TextLengthUnit::utf16_code_units&&width==4?2:1;offset+=width;
        }
        return count;
    }
    [[nodiscard]] TextInputResult<Replacement> replace(TextInputRange range,std::string_view inserted,bool enforce_limit=false) const {
        auto offsets = bytes(range); if (!offsets) return std::unexpected(offsets.error());
        auto inserted_index = index(inserted); if (!inserted_index) return std::unexpected(inserted_index.error());
        if(enforce_limit&&maximum_length_) {
            const auto retained_length=length_in(std::string_view(text_).substr(0,offsets->first),length_unit_)+
                length_in(std::string_view(text_).substr(offsets->second),length_unit_);
            const auto available=retained_length<*maximum_length_?*maximum_length_-retained_length:0;
            const auto count=prefix_bytes(inserted,available,length_unit_);
            if(count!=inserted.size()) {
                inserted=inserted.substr(0,count);
                inserted_index=index(inserted);if(!inserted_index)return std::unexpected(inserted_index.error());
            }
        }
        const auto retained = text_.size() - (offsets->second - offsets->first);
        if (inserted.size() > maximum_bytes - retained)
            return failure<Replacement>(TextInputErrorCode::too_large,"Text input exceeds 64 KiB");
        Replacement value;
        value.text.reserve(retained + inserted.size());
        value.text.append(text_,0,offsets->first);
        value.text.append(inserted);
        value.text.append(text_,offsets->second,std::string::npos);
        // A platform may commit a range outside an overlong active preedit.
        // Bound the whole result even in that case and clamp its cursor later.
        bool clipped_result=false;
        if(enforce_limit&&maximum_length_) {
            const auto count=prefix_bytes(value.text,*maximum_length_,length_unit_);
            clipped_result=count!=value.text.size();value.text.resize(count);
        }
        auto candidate = index(value.text); if (!candidate) return std::unexpected(candidate.error());
        value.inserted_length = (*inserted_index)->length();
        const auto expected=length()-range.length+value.inserted_length;
        if ((*candidate)->length()!=length_in(value.text,TextLengthUnit::utf16_code_units) ||
            (!clipped_result&&(*candidate)->length()!=expected) ||
            ((*candidate)->length()>=range.location+value.inserted_length &&
             !(*candidate)->byte_offset(range.location+value.inserted_length)))
            return failure<Replacement>(TextInputErrorCode::invalid_boundary,"Text boundary service returned an inconsistent replacement");
        value.boundary = std::move(*candidate);
        return value;
    }
    template<class T, class F> [[nodiscard]] static TextInputResult<T> protect(F&& operation) {
        try { return operation(); }
        catch (...) { return failure<T>(TextInputErrorCode::unavailable,"Text input operation failed"); }
    }
    // Persistent bounded history keeps copies of BasicTextInput cheap, so preparing
    // a candidate edit does not duplicate every prior text snapshot.
    [[nodiscard]] static History push_history(History older, std::string text,
                                               std::size_t anchor, std::size_t caret) {
        constexpr std::size_t max_entries = 64, max_bytes = 2 * 1024 * 1024;
        if (older && (older->count >= max_entries || older->bytes > max_bytes - text.size())) {
            std::vector<const HistoryEntry*> keep;
            auto remaining = max_bytes - text.size();
            for (auto entry = older.get(); entry && keep.size() < max_entries - 1; entry = entry->older.get()) {
                if (entry->text->size() > remaining) break;
                keep.push_back(entry); remaining -= entry->text->size();
            }
            History trimmed;
            for (auto it = keep.rbegin(); it != keep.rend(); ++it) {
                const auto* entry = *it;
                trimmed = std::make_shared<HistoryEntry>(HistoryEntry{entry->text, entry->anchor, entry->caret,
                    trimmed, 1 + (trimmed ? trimmed->count : 0), entry->text->size() + (trimmed ? trimmed->bytes : 0)});
            }
            older = std::move(trimmed);
        }
        const auto count = 1 + (older ? older->count : 0);
        const auto bytes = text.size() + (older ? older->bytes : 0);
        auto snapshot = std::make_shared<const std::string>(std::move(text));
        return std::make_shared<HistoryEntry>(HistoryEntry{std::move(snapshot),anchor,caret,std::move(older),count,bytes});
    }
    [[nodiscard]] TextInputResult<bool> restore_history(History& source, History& destination) {
        if (original_) {
            auto canceled = cancel_composition();
            if (!canceled) return std::unexpected(canceled.error());
            return true;
        }
        if (!history_enabled_ || !source) return false;
        auto candidate = index(*source->text);
        if (!candidate) return std::unexpected(candidate.error());
        auto restored = *source->text;
        auto next_destination = push_history(destination,text_,anchor_,caret_);
        auto entry = source;
        source = entry->older;
        destination = std::move(next_destination);
        text_ = std::move(restored);
        boundary_ = std::move(*candidate);
        anchor_ = entry->anchor; caret_ = entry->caret;
        return true;
    }
public:
    static constexpr std::size_t maximum_bytes = 64 * 1024;
    [[nodiscard]] static TextInputResult<BasicTextInput> create(TextInputBoundaryFactory factory,std::string text={}) {
        return protect<BasicTextInput>([&]() -> TextInputResult<BasicTextInput> {
            BasicTextInput result(std::move(factory));
            auto boundary = result.index(text); if (!boundary) return std::unexpected(boundary.error());
            result.text_ = std::move(text); result.boundary_ = std::move(*boundary);
            result.anchor_ = result.caret_ = result.length();
            return result;
        });
    }
    [[nodiscard]] const std::string& text() const noexcept { return text_; }
    // During preedit this remains the value from before the composition.
    [[nodiscard]] const std::string& committed_text() const noexcept { return original_ ? original_->text : text_; }
    [[nodiscard]] std::size_t length() const noexcept { return boundary_->length(); }
    [[nodiscard]] std::size_t anchor() const noexcept { return anchor_; }
    [[nodiscard]] std::size_t caret() const noexcept { return caret_; }
    [[nodiscard]] TextInputRange selection() const noexcept {
        return {(std::min)(anchor_,caret_),(std::max)(anchor_,caret_)-(std::min)(anchor_,caret_)};
    }
    [[nodiscard]] std::optional<TextInputRange> marked() const noexcept { return marked_; }
    [[nodiscard]] TextInputResult<bool> set_maximum_length(std::optional<std::size_t> limit,TextLengthUnit unit) {
        return protect<bool>([&]() -> TextInputResult<bool> {
            if(unit!=TextLengthUnit::utf16_code_units&&unit!=TextLengthUnit::unicode_scalars)
                return failure<bool>(TextInputErrorCode::invalid_range,"Unknown text length unit");
            if(limit==maximum_length_&&unit==length_unit_)return false;
            auto next=*this;
            if(auto canceled=next.cancel_composition();!canceled)return std::unexpected(canceled.error());
            next.maximum_length_=limit;next.length_unit_=unit;
            auto changed=next.set_text(next.text_);if(!changed)return std::unexpected(changed.error());
            next.undo_.reset();next.redo_.reset();*this=std::move(next);return true;
        });
    }
    // Disabling history also drops previously retained text snapshots.
    void set_history_enabled(bool enabled) noexcept {
        history_enabled_ = enabled;
        if (!enabled) { undo_.reset(); redo_.reset(); }
    }
    [[nodiscard]] TextInputResult<bool> undo() {
        return protect<bool>([&] { return restore_history(undo_,redo_); });
    }
    [[nodiscard]] TextInputResult<bool> redo() {
        return protect<bool>([&] { return restore_history(redo_,undo_); });
    }
    [[nodiscard]] TextInputResult<std::string> read(TextInputRange range) const {
        return protect<std::string>([&]() -> TextInputResult<std::string> {
            auto value = bytes(range); if (!value) return std::unexpected(value.error());
            return text_.substr(value->first,value->second-value->first);
        });
    }
    [[nodiscard]] TextInputResult<bool> set_text(std::string value) {
        return protect<bool>([&]() -> TextInputResult<bool> {
            auto boundary = index(value); if (!boundary) return std::unexpected(boundary.error());
            if(maximum_length_) {
                const auto count=prefix_bytes(value,*maximum_length_,length_unit_);
                if(count!=value.size()) {
                    value.resize(count);boundary=index(value);if(!boundary)return std::unexpected(boundary.error());
                }
            }
            if (value == text_ && !marked_) return false;
            const bool changed = value != text_ || bool(marked_);
            text_ = std::move(value); boundary_ = std::move(*boundary); anchor_ = caret_ = length();
            marked_.reset(); original_.reset(); undo_.reset(); redo_.reset(); return changed;
        });
    }
    [[nodiscard]] TextInputResult<bool> set_selection(std::size_t anchor,std::size_t caret) {
        return protect<bool>([&]() -> TextInputResult<bool> {
            if (!bytes({(std::min)(anchor,caret),(std::max)(anchor,caret)-(std::min)(anchor,caret)}))
                return failure<bool>(TextInputErrorCode::invalid_range,"Selection must use complete Unicode scalars");
            const bool changed = anchor != anchor_ || caret != caret_; anchor_ = anchor; caret_ = caret; return changed;
        });
    }
    [[nodiscard]] TextInputResult<TextInputRange> word_range(std::size_t position) const {
        return protect<TextInputRange>([&]() -> TextInputResult<TextInputRange> {
            if (position > length() || !boundary_->byte_offset(position))
                return failure<TextInputRange>(TextInputErrorCode::invalid_range,"Word position splits a Unicode scalar");
            const auto range=boundary_->word_range(position);
            if (!range) return failure<TextInputRange>(TextInputErrorCode::unavailable,"Text boundary service cannot select words");
            const auto grapheme_boundary=[&](std::size_t point){
                if(point==0||point==length())return true;
                const auto prior=boundary_->previous_grapheme(point);
                return prior<point&&boundary_->next_grapheme(prior)==point;
            };
            if (range->location > length() || range->length > length()-range->location ||
                position < range->location || position > range->location+range->length || !bytes(*range) ||
                !grapheme_boundary(range->location) || !grapheme_boundary(range->location+range->length))
                return failure<TextInputRange>(TextInputErrorCode::invalid_boundary,"Text boundary service returned an invalid word range");
            return *range;
        });
    }
    // A collapsed selection moves one extended grapheme or word. A non-extended
    // move first collapses an existing selection toward the requested direction.
    [[nodiscard]] TextInputResult<bool> move(int direction,bool extend,bool word=false) {
        return protect<bool>([&]() -> TextInputResult<bool> {
            if (direction != -1 && direction != 1)
                return failure<bool>(TextInputErrorCode::invalid_range,"Text movement direction must be -1 or 1");
            const auto selected = selection();
            const auto next = !extend && selected.length ? (direction < 0 ? selected.location : selected.location + selected.length) :
                direction < 0 ? (word ? boundary_->previous_word(caret_) : boundary_->previous_grapheme(caret_)) :
                                (word ? boundary_->next_word(caret_) : boundary_->next_grapheme(caret_));
            if (next > length() || !boundary_->byte_offset(next) ||
                (selected.length == 0 && ((direction < 0 && next > caret_) || (direction > 0 && next < caret_))))
                return failure<bool>(TextInputErrorCode::invalid_boundary,"Text boundary service returned an invalid movement");
            return set_selection(extend ? anchor_ : next,next);
        });
    }
    [[nodiscard]] TextInputResult<bool> insert(std::string_view inserted,std::optional<TextInputRange> range=std::nullopt) {
        return protect<bool>([&]() -> TextInputResult<bool> {
            const auto target = range.value_or(marked_.value_or(selection()));
            auto value = replace(target,inserted,true); if (!value) return std::unexpected(value.error());
            const bool changed = value->text != (original_ ? original_->text : text_);
            const auto position = (std::min)(target.location + value->inserted_length,value->boundary->length());
            History history;
            if (changed && history_enabled_) {
                history = original_ ? push_history(undo_,original_->text,original_->anchor,original_->caret)
                                    : push_history(undo_,text_,anchor_,caret_);
            }
            text_ = std::move(value->text); boundary_ = std::move(value->boundary);
            anchor_ = caret_ = position; marked_.reset(); original_.reset();
            if (changed && history_enabled_) { undo_ = std::move(history); redo_.reset(); }
            return changed;
        });
    }
    // A host may commit marked text and delete in the same undo transaction.
    [[nodiscard]] TextInputResult<bool> erase(int direction,bool word=false,bool commit_marked=false) {
        return protect<bool>([&]() -> TextInputResult<bool> {
            if (marked_ && !commit_marked) return failure<bool>(TextInputErrorCode::composing,"Commit or cancel composition before deleting");
            if (direction != -1 && direction != 1)
                return failure<bool>(TextInputErrorCode::invalid_range,"Delete direction must be -1 or 1");
            auto target = selection();
            if (!target.length) {
                const auto edge = direction < 0 ? (word ? boundary_->previous_word(caret_) : boundary_->previous_grapheme(caret_)) :
                                                  (word ? boundary_->next_word(caret_) : boundary_->next_grapheme(caret_));
                if (edge > length() || !boundary_->byte_offset(edge) || (direction < 0 && edge > caret_) || (direction > 0 && edge < caret_))
                    return failure<bool>(TextInputErrorCode::invalid_boundary,"Text boundary service returned an invalid deletion");
                target = {(std::min)(caret_,edge),(std::max)(caret_,edge)-(std::min)(caret_,edge)};
            }
            return insert({},target);
        });
    }
    // Delete around the current selection while retaining the composition.
    // Only deletions outside the marked candidate are applied to the original.
    [[nodiscard]] TextInputResult<bool> erase_surrounding(TextInputRange range) {
        return protect<bool>([&]() -> TextInputResult<bool> {
            using namespace text_surrounding_detail;
            if(!original_||!marked_)return failure<bool>(TextInputErrorCode::composing,"Surrounding deletion requires a composition");
            const auto selected=selection();
            auto plan=Plan::create(length(),range.location,range.length,selected.location,selected.length);
            if(!plan)return failure<bool>(TextInputErrorCode::invalid_range,"Surrounding deletion must contain the selection");
            const auto visible=plan->visible();
            for(std::size_t i=0;i<visible.count;++i){
                const auto item=visible.values[i];auto valid=bytes({item.first,item.last-item.first});
                if(!valid)return std::unexpected(valid.error());
            }
            if(plan->empty())return false;
            const auto mark=*marked_;const auto base_length=original_->boundary->length();
            if(mark.location>length()||mark.length>length()-mark.location||base_length<length()-mark.length)
                return failure<bool>(TextInputErrorCode::composing,"Composition cannot be mapped to its original text");
            const auto original_length=base_length-(length()-mark.length);
            if(mark.location>base_length||original_length>base_length-mark.location)
                return failure<bool>(TextInputErrorCode::composing,"Composition original range is invalid");
            const auto visible_first=boundary_->byte_offset(mark.location),visible_last=boundary_->byte_offset(mark.location+mark.length);
            const auto base_first=original_->boundary->byte_offset(mark.location),base_last=original_->boundary->byte_offset(mark.location+original_length);
            if(!visible_first||!visible_last||!base_first||!base_last||*visible_first>*visible_last||*base_first>*base_last||
               *visible_last>text_.size()||*base_last>original_->text.size()||
               std::string_view(text_).substr(0,*visible_first)!=std::string_view(original_->text).substr(0,*base_first)||
               std::string_view(text_).substr(*visible_last)!=std::string_view(original_->text).substr(*base_last))
                return failure<bool>(TextInputErrorCode::composing,"Composition has edits outside its marked range");
            const auto base=plan->base(mark.location,mark.length,original_length);
            auto without=[&](std::string_view input,const TextInputBoundary& boundary,const Ranges& ranges)->TextInputResult<std::string>{
                std::string result;result.reserve(input.size());std::size_t previous=0;
                for(std::size_t i=0;i<ranges.count;++i){
                    const auto item=ranges.values[i];auto first=boundary.byte_offset(item.first),last=boundary.byte_offset(item.last);
                    if(!first||!last||*first<previous||*last<*first||*last>input.size())
                        return failure<std::string>(TextInputErrorCode::invalid_boundary,"Surrounding deletion has invalid Unicode boundaries");
                    result.append(input.substr(previous,*first-previous));previous=*last;
                }
                result.append(input.substr(previous));return result;
            };
            auto next_text=without(text_,*boundary_,visible);if(!next_text)return std::unexpected(next_text.error());
            auto next_boundary=index(*next_text);if(!next_boundary)return std::unexpected(next_boundary.error());
            if((*next_boundary)->length()!=visible.map(length()))
                return failure<bool>(TextInputErrorCode::invalid_boundary,"Surrounding deletion returned an inconsistent index");
            auto next_original=*original_;History history;
            if(base.count){
                auto text=without(original_->text,*original_->boundary,base);if(!text)return std::unexpected(text.error());
                auto boundary=index(*text);if(!boundary)return std::unexpected(boundary.error());
                if((*boundary)->length()!=base.map(base_length))
                    return failure<bool>(TextInputErrorCode::invalid_boundary,"Surrounding deletion returned an inconsistent original index");
                if(history_enabled_)history=push_history(undo_,original_->text,original_->anchor,original_->caret);
                next_original={std::move(*text),std::move(*boundary),base.map(original_->anchor),base.map(original_->caret)};
            }
            const auto next_marked=TextInputRange{visible.map(mark.location),visible.map(mark.location+mark.length)-visible.map(mark.location)};
            const auto next_anchor=visible.map(anchor_),next_caret=visible.map(caret_);
            text_=std::move(*next_text);boundary_=std::move(*next_boundary);marked_=next_marked;
            anchor_=next_anchor;caret_=next_caret;original_=std::move(next_original);
            if(base.count&&history_enabled_){undo_=std::move(history);redo_.reset();}
            return true;
        });
    }
    // Marked text is included in the visible document and native UTF-16 ranges.
    // It becomes an edit only on commit; cancel restores the complete original.
    [[nodiscard]] TextInputResult<bool> set_composition(std::string_view inserted,
            std::optional<TextInputRange> replacement=std::nullopt,std::optional<TextInputRange> selection=std::nullopt,
            bool selection_reversed=false) {
        return protect<bool>([&]() -> TextInputResult<bool> {
            const auto target = replacement.value_or(marked_.value_or(this->selection()));
            auto value = replace(target,inserted); if (!value) return std::unexpected(value.error());
            const auto chosen = selection.value_or(TextInputRange{value->inserted_length,0});
            if ((selection_reversed&&!chosen.length)||chosen.location > value->inserted_length || chosen.length > value->inserted_length - chosen.location ||
                !value->boundary->byte_offset(target.location + chosen.location) ||
                !value->boundary->byte_offset(target.location + chosen.location + chosen.length))
                return failure<bool>(TextInputErrorCode::invalid_range,"Composition selection splits a Unicode scalar");
            auto original = original_ ? *original_ : Original{text_,boundary_,anchor_,caret_};
            const TextInputRange next_marked{target.location,value->inserted_length};
            const auto first = target.location + chosen.location,last=first+chosen.length;
            const auto next_anchor = selection_reversed?last:first;
            const auto next_caret = selection_reversed?first:last;
            const bool changed = text_ != value->text || marked_ != std::optional<TextInputRange>{next_marked} ||
                                 anchor_ != next_anchor || caret_ != next_caret;
            text_ = std::move(value->text); boundary_ = std::move(value->boundary);
            anchor_ = next_anchor; caret_ = next_caret; marked_ = next_marked; original_ = std::move(original);
            return changed;
        });
    }
    // Mark existing visible text without moving the selection. A previous
    // candidate becomes committed only after the next state is fully prepared.
    [[nodiscard]] TextInputResult<bool> set_composition_region(TextInputRange range) {
        return protect<bool>([&]() -> TextInputResult<bool> {
            if(range.location>length()||range.length>length()-range.location)
                return failure<bool>(TextInputErrorCode::invalid_range,"Composition region exceeds the document");
            if(range.length){auto valid=bytes(range);if(!valid)return std::unexpected(valid.error());}
            if(!range.length&&!original_)return false;
            auto next=*this;const auto old_anchor=anchor_,old_caret=caret_;
            if(next.original_){
                auto finished=next.commit_composition();if(!finished)return std::unexpected(finished.error());
                if(next.text_!=text_||next.anchor_!=old_anchor||next.caret_!=old_caret)
                    return failure<bool>(TextInputErrorCode::composing,"Finishing composition would change visible text or selection");
            }
            if(range.length){
                auto offsets=next.bytes(range);if(!offsets)return std::unexpected(offsets.error());
                auto candidate=next.text_.substr(offsets->first,offsets->second-offsets->first);
                auto marked=next.set_composition(candidate,range);if(!marked)return std::unexpected(marked.error());
                auto selected=next.set_selection(old_anchor,old_caret);if(!selected)return std::unexpected(selected.error());
            }
            const bool changed=marked_!=next.marked_||committed_text()!=next.committed_text()||
                anchor_!=next.anchor_||caret_!=next.caret_;
            *this=std::move(next);return changed;
        });
    }
    [[nodiscard]] TextInputResult<bool> commit_composition() {
        return protect<bool>([&]() -> TextInputResult<bool> {
            if (!original_) return false;
            const auto range=*marked_;
            auto offsets=bytes(range);if(!offsets)return std::unexpected(offsets.error());
            const auto marked_text=std::string_view(text_).substr(offsets->first,offsets->second-offsets->first);
            auto value=replace(range,marked_text,true);if(!value)return std::unexpected(value.error());
            const auto map_position=[&](std::size_t position) {
                if(position<=range.location)return (std::min)(position,value->boundary->length());
                const auto end=range.location+range.length;
                const auto mapped=position<=end?(std::min)(position,range.location+value->inserted_length):
                    position-range.length+value->inserted_length;
                return (std::min)(mapped,value->boundary->length());
            };
            const auto next_anchor=map_position(anchor_),next_caret=map_position(caret_);
            const bool changed = value->text != original_->text;
            History history;
            if (changed && history_enabled_)
                history = push_history(undo_,original_->text,original_->anchor,original_->caret);
            text_=std::move(value->text);boundary_=std::move(value->boundary);
            anchor_=next_anchor;caret_=next_caret;
            marked_.reset(); original_.reset();
            if (changed && history_enabled_) { undo_ = std::move(history); redo_.reset(); }
            return changed;
        });
    }
    [[nodiscard]] TextInputResult<bool> cancel_composition() {
        if (!original_) return false;
        const bool changed = text_ != original_->text || anchor_ != original_->anchor || caret_ != original_->caret;
        text_ = std::move(original_->text); boundary_ = std::move(original_->boundary);
        anchor_ = original_->anchor; caret_ = original_->caret; marked_.reset(); original_.reset(); return changed;
    }
};
using TextInputModel=BasicTextInput<DefaultTextInputModelPolicy>;
} // namespace ii::gui
