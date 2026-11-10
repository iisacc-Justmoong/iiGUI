#include "../src/Platform/AndroidImeProtocol.h"
#include "../src/Platform/AndroidImePublication.h"
#include "../src/Widgets/WidgetState.h"

#include <memory>
#include <limits>
#include <stdexcept>

using namespace ii;
using namespace ii::gui;
using namespace ii::gui::detail;

namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(core::Result<T> value){if(!value)throw std::runtime_error(value.error().message);return std::move(*value);}
void good(core::Result<void> value){if(!value)throw std::runtime_error(value.error().message);}
struct FixtureHost {
    std::shared_ptr<WidgetState> tree=std::make_shared<WidgetState>([](runtime::Task task){task();});
    std::optional<WidgetSnapshot> focus(){
        auto snapshots=tree->accessibility_snapshots();if(!snapshots)return {};
        for(auto& value:*snapshots)if(value.focused&&value.text_input&&value.visible&&value.enabled&&!value.input_blocked)return value;
        return {};
    }
    core::Result<TextContent> read(std::string_view id,TextRange range){auto field=tree->find(id);if(!field)return std::unexpected(field.error());return field->text_content(range);}
    core::Result<void> send(WidgetInput event){return tree->input(event);}
};
struct ChunkHost {
    WidgetSnapshot focused{};
    std::u16string text;
    std::optional<WidgetInput> delivered;
    bool move_during_read=false,truncate_read=false,change_during_read=false,aba_during_read=false,change_during_send=false;
    std::shared_ptr<const int> identity=std::make_shared<const int>(0);
    explicit ChunkHost(std::u16string value,std::size_t caret):text(std::move(value)){
        focused.id="chunk";focused.focused=true;focused.text_input=true;focused.text_length=text.size();
        focused.text_selection={caret,0};
        focused.text_revision=TextInputRevision{identity,0,0};
    }
    std::optional<WidgetSnapshot> focus(){return focused;}
    core::Result<TextContent> read(std::string_view,TextRange range){
        if(range.location>text.size()||range.length>text.size()-range.location)
            return core::fail(core::ErrorCode::out_of_range,"Range outside fixture");
        const auto split=[&](std::size_t i){return i>0&&i<text.size()&&text[i]>=0xdc00&&text[i]<=0xdfff;};
        if(split(range.location)||split(range.location+range.length))
            return core::fail(core::ErrorCode::out_of_range,"Split UTF-16 scalar");
        if(truncate_read&&range.length)--range.length;
        auto value=core::to_utf8(std::u16string_view(text).substr(range.location,range.length));
        if(!value)return core::fail(core::ErrorCode::invalid_argument,"Invalid fixture text");
        if(move_during_read){focused.text_selection={0,0};move_during_read=false;}
        if(change_during_read){text=u"AB";++focused.text_revision->value;change_during_read=false;}
        if(aba_during_read){text=u"AB";text=u"😀";focused.text_revision->value+=2;aba_during_read=false;}
        return TextContent{range,std::move(*value)};
    }
    core::Result<void> send(WidgetInput event){
        if(change_during_send){++focused.text_revision->value;change_during_send=false;}
        if(event.expected_text_revision!=focused.text_revision)return core::fail(core::ErrorCode::busy,"Stale test revision");
        delivered=std::move(event);return {};
    }
};
void revision_conflicts(){
    ChunkHost changed(u"😀",2);AndroidImeProtocol ime;const auto gen=take(ime.state(changed)).generation;
    changed.change_during_read=true;
    check(!ime.erase_surrounding_codepoints(changed,gen,1,0)&&!changed.delivered,
          "A same-length replacement during the read produced a stale deletion");
    ChunkHost aba(u"😀",2);AndroidImeProtocol aba_ime;const auto agen=take(aba_ime.state(aba)).generation;
    aba.aba_during_read=true;
    check(!aba_ime.erase_surrounding_codepoints(aba,agen,1,0)&&!aba.delivered,
          "An ABA text edit during the read was not detected");
    ChunkHost sending(u"abc",2);AndroidImeProtocol sending_ime;const auto sgen=take(sending_ime.state(sending)).generation;
    sending.change_during_send=true;
    check(!sending_ime.erase_surrounding(sending,sgen,1,0)&&!sending.delivered,
          "A change immediately before dispatch applied a stale deletion");
    ChunkHost unversioned(u"abc",2);unversioned.focused.text_revision.reset();AndroidImeProtocol old_ime;
    const auto oldgen=take(old_ime.state(unversioned)).generation;
    check(!old_ime.query_text(unversioned,oldgen,AndroidImeQuery::before,1)&&
          !old_ime.query_text(unversioned,oldgen,AndroidImeQuery::selected,1)&&!old_ime.finish(unversioned,oldgen)&&
          !old_ime.erase_surrounding(unversioned,oldgen,1,0)&&!old_ime.composition(unversioned,oldgen,"x",1)&&!unversioned.delivered,
          "An unversioned provider accepted unsafe Android input");
    ChunkHost invalid(u"abc",2);invalid.focused.text_revision->identity.reset();AndroidImeProtocol invalid_ime;
    check(!invalid_ime.state(invalid),"An input revision without an owner identity was accepted");
    ChunkHost replaced(u"abc",2);AndroidImeProtocol replaced_ime;const auto original=take(replaced_ime.state(replaced)).generation;
    replaced.identity=std::make_shared<const int>(0);replaced.focused.text_revision->identity=replaced.identity;
    const auto next=take(replaced_ime.state(replaced)).generation;
    check(next!=original&&!replaced_ime.composition(replaced,original,"x",1)&&!replaced.delivered,
          "A replacement widget with the same ID retained the old input connection");

    FixtureHost focus;auto container=take(focus.tree->content_host({},"host",{200,80}));Widget field,other;
    good(container.set_content([&](WidgetBuilder& builder)->core::Result<void>{
        auto root=take(builder.vstack({},"root"));field=take(builder.input_field(root,"field",{.text="A"}));
        other=take(builder.input_field(root,"other",{.text="B"}));return {};
    }));
    (void)take(focus.tree->drawing_snapshot({200,80}));good(field.focus());AndroidImeProtocol focus_ime;
    const auto first=take(focus_ime.state(focus)).generation;good(other.focus());good(field.focus());
    const auto restored=take(focus_ime.state(focus)).generation;
    check(restored!=first&&!focus_ime.composition(focus,first,"x",1),"A focus ABA retained the old input connection");
    focus.tree->close();

    FixtureHost replaced_provider;auto custom=take(replaced_provider.tree->absolute_stack({},"custom",{100,30}));
    const auto make_provider=[](){
        auto provider=std::make_shared<TextInputProvider>();
        provider->state=[]()->core::Result<TextInputState>{return TextInputState{.length=1,.revision=0};};
        provider->read=[](TextRange range)->core::Result<TextContent>{return TextContent{range,range.length?"A":""};};return provider;
    };
    good(custom.set_text_input_provider(make_provider()));
    (void)take(replaced_provider.tree->drawing_snapshot({100,30}));good(custom.focus());AndroidImeProtocol provider_ime;
    const auto old_provider=take(provider_ime.state(replaced_provider)).generation;
    good(custom.set_text_input_provider(make_provider()));
    const auto new_provider=take(provider_ime.state(replaced_provider)).generation;
    check(new_provider!=old_provider&&!provider_ime.composition(replaced_provider,old_provider,"x",1),
          "Replacing a focused text provider retained the old input connection");
    replaced_provider.tree->close();

    FixtureHost live;auto editor=take(live.tree->absolute_stack({},"editor",{100,30}));
    std::string text="😀";std::uint64_t revision=0;bool change=false;unsigned edits=0;
    auto source=std::make_shared<TextInputProvider>();
    source->state=[&]()->core::Result<TextInputState>{return TextInputState{.length=2,.selection={2,0},.revision=revision};};
    source->read=[&](TextRange range)->core::Result<TextContent>{
        auto prior=range.length?text:std::string{};if(change){text="AB";++revision;change=false;}
        return TextContent{range,std::move(prior)};
    };
    good(editor.set_text_input_provider(source));good(editor.set_input_handler([&](const WidgetInput&)->core::Result<bool>{++edits;return true;}));
    (void)take(live.tree->drawing_snapshot({100,30}));good(editor.focus());AndroidImeProtocol live_ime;
    const auto livegen=take(live_ime.state(live)).generation;change=true;
    const auto rejected=live_ime.erase_surrounding_codepoints(live,livegen,1,0);
    check(!rejected&&rejected.error().code==core::ErrorCode::busy&&text=="AB"&&edits==0,
          "A real custom provider applied an edit derived from a stale same-length read");
    live.tree->close();
}
void codepoint_deletion(){
    FixtureHost host;auto field=take(host.tree->input_field({},"field",{.text="A😀한Z"},{},{}));
    (void)take(host.tree->drawing_snapshot({200,40}));good(field.focus());AndroidImeProtocol ime;
    auto generation=take(ime.state(host)).generation;
    good(ime.select(host,generation,3,3));
    check(!ime.erase_surrounding(host,generation,1,0),"UTF-16 deletion split an emoji");
    good(ime.erase_surrounding_codepoints(host,generation,1,1));
    check(take(field.snapshot()).text=="AZ"&&take(field.snapshot()).text_selection==TextRange{1,0},
          "Codepoint deletion did not remove one scalar on each side");
    good(ime.erase_surrounding_codepoints(host,generation,(std::numeric_limits<std::int32_t>::max)(),0));
    check(take(field.snapshot()).text=="Z", "Codepoint deletion did not clamp to the document start");
    check(!ime.erase_surrounding_codepoints(host,generation,-1,0),"Negative codepoint deletion was accepted");
    host.tree->close();

    std::u16string long_text(4095,u'a');long_text+=u"😀";long_text+=std::u16string(8191,u'b');
    ChunkHost backward(long_text,long_text.size());AndroidImeProtocol backward_ime;
    auto bgen=take(backward_ime.state(backward)).generation;
    good(backward_ime.erase_surrounding_codepoints(backward,bgen,8192,0));
    check(backward.delivered&&backward.delivered->replacement_range==TextRange{4095,8193},
          "Backward chunk deletion split a surrogate at its boundary");
    std::u16string forward_text(8191,u'b');forward_text+=u"😀";forward_text+=std::u16string(4095,u'a');
    ChunkHost forward(forward_text,0);AndroidImeProtocol forward_ime;
    auto fgen=take(forward_ime.state(forward)).generation;
    good(forward_ime.erase_surrounding_codepoints(forward,fgen,0,8192));
    check(forward.delivered&&forward.delivered->replacement_range==TextRange{0,8193},
          "Forward chunk deletion split a surrogate at its boundary");
    ChunkHost racing(u"abc",2);AndroidImeProtocol racing_ime;auto rgen=take(racing_ime.state(racing)).generation;
    racing.move_during_read=true;
    check(!racing_ime.erase_surrounding_codepoints(racing,rgen,1,0)&&!racing.delivered,
          "Deletion used a selection that changed while reading");
    racing.focused.text_selection={2,0};racing.move_during_read=true;
    check(!racing_ime.query_text(racing,rgen,AndroidImeQuery::before,1),
          "A text query returned content for a selection that changed while reading");
    ChunkHost exact(std::u16string(65536,u'x'),65536);AndroidImeProtocol exact_ime;
    const auto egen=take(exact_ime.state(exact)).generation;
    good(exact_ime.erase_surrounding_codepoints(exact,egen,65536,0));
    check(exact.delivered&&exact.delivered->replacement_range==TextRange{0,65536},
          "The exact codepoint deletion limit was rejected");
    ChunkHost oversized(std::u16string(65537,u'x'),65537);AndroidImeProtocol oversized_ime;
    const auto ogen=take(oversized_ime.state(oversized)).generation;
    check(!oversized_ime.erase_surrounding_codepoints(oversized,ogen,65537,0)&&!oversized.delivered,
          "An oversized codepoint deletion was applied");
}
void selected_deletion(){
    FixtureHost host;int edits=0;
    auto field=take(host.tree->input_field({},"field",{.text="A😀B한Z"},[&](std::string){++edits;},{}));
    (void)take(host.tree->drawing_snapshot({200,40}));good(field.focus());AndroidImeProtocol ime;
    const auto gen=take(ime.state(host)).generation;good(ime.select(host,gen,5,3));
    good(ime.erase_surrounding_codepoints(host,gen,1,1));
    auto result=take(field.snapshot());
    check(result.text=="AB한"&&result.text_selection==TextRange{1,2}&&result.text_selection_reversed&&edits==1,
          "Codepoint deletion did not preserve the reverse selection in one edit");
    good(ime.erase_surrounding(host,gen,1,0));result=take(field.snapshot());
    check(result.text=="B한"&&result.text_selection==TextRange{0,2}&&result.text_selection_reversed&&edits==2,
          "UTF-16 deletion before a selection lost its direction");
    good(ime.erase_surrounding(host,gen,0,0));check(edits==2,"An empty deletion emitted an edit");
    host.tree->close();

    ChunkHost large(std::u16string(100002,u'x'),1);large.focused.text_selection={1,100000};
    large.focused.text_selection_reversed=true;AndroidImeProtocol large_ime;const auto lgen=take(large_ime.state(large)).generation;
    good(large_ime.erase_surrounding(large,lgen,1,0));
    check(large.delivered&&large.delivered->replacement_range==TextRange{0,1}&&
          large.delivered->text_selection_after==TextRange{0,100000}&&large.delivered->selection_reversed,
          "A one-sided deletion copied or lost a large selection");
    large.delivered.reset();
    check(!large_ime.erase_surrounding(large,lgen,1,1)&&!large.delivered,
          "An oversized two-sided replacement was applied nonatomically");
    ChunkHost partial(u"aHELLOz",1);partial.focused.text_selection={1,5};partial.truncate_read=true;
    AndroidImeProtocol partial_ime;const auto pgen=take(partial_ime.state(partial)).generation;
    check(!partial_ime.erase_surrounding(partial,pgen,1,1)&&!partial.delivered,
          "A partial selected-text read was used for an edit");
}
void composing_deletion(){
    FixtureHost host;unsigned edits=0;
    auto field=take(host.tree->input_field({},"field",{.text="A😀xy한Z"},[&](std::string){++edits;},{}));
    (void)take(host.tree->drawing_snapshot({200,40}));good(field.focus());AndroidImeProtocol ime;
    const auto gen=take(ime.state(host)).generation;
    good(ime.select(host,gen,5,4));good(ime.composing_region(host,gen,3,6));
    check(!ime.erase_surrounding(host,gen,2,1),"Deletion split an emoji during composition");
    auto state=take(ime.state(host,gen));
    check(state.marked==TextRange{3,3}&&state.selection==TextRange{4,1}&&state.selection_reversed&&edits==0,
          "A failed composing deletion changed the editor");
    good(ime.erase_surrounding_codepoints(host,gen,2,1));state=take(ime.state(host,gen));
    check(state.marked==TextRange{1,1}&&state.selection==TextRange{1,1}&&state.selection_reversed&&
          take(ime.read(host,gen,{0,3})).text=="AyZ"&&take(field.snapshot()).text=="Axy한Z"&&edits==1,
          "Composing deletion lost selected text or committed the candidate");
    good(ime.finish(host,gen));
    check(take(field.snapshot()).text=="AyZ"&&edits==2,"Finishing the shortened candidate failed");
    host.tree->close();

    FixtureHost different;auto replaced=take(different.tree->input_field({},"different",{.text="A😀YZ"},{},{}));
    (void)take(different.tree->drawing_snapshot({200,40}));good(replaced.focus());AndroidImeProtocol different_ime;
    const auto dgen=take(different_ime.state(different)).generation;
    good(different_ime.select(different,dgen,1,3));good(different_ime.composition(different,dgen,"xyz",1));
    good(different_ime.select(different,dgen,3,2));good(different_ime.erase_surrounding_codepoints(different,dgen,2,2));
    state=take(different_ime.state(different,dgen));
    check(state.marked==TextRange{0,1}&&state.selection==TextRange{0,1}&&state.selection_reversed&&
          take(different_ime.read(different,dgen,{0,2})).text=="yZ"&&take(replaced.snapshot()).text=="😀Z",
          "Deletion misaligned an original range whose candidate had a different length");
    good(different_ime.finish(different,dgen));check(take(replaced.snapshot()).text=="yZ","A resized candidate failed to commit");
    different.tree->close();

    FixtureHost empty;auto other=take(empty.tree->input_field({},"empty",{.text="AxZ"},{},{}));
    (void)take(empty.tree->drawing_snapshot({200,40}));good(other.focus());AndroidImeProtocol empty_ime;
    const auto egen=take(empty_ime.state(empty)).generation;good(empty_ime.select(empty,egen,2,2));
    good(empty_ime.composing_region(empty,egen,1,2));good(empty_ime.erase_surrounding(empty,egen,1,0));
    state=take(empty_ime.state(empty,egen));
    check(state.marked==TextRange{1,0}&&state.selection==TextRange{1,0}&&take(empty_ime.read(empty,egen,{0,2})).text=="AZ",
          "Deleting the complete candidate lost the empty composition");
    empty.tree->close();

    ChunkHost racing(u"AxZ",2);racing.focused.marked_range=TextRange{1,1};AndroidImeProtocol racing_ime;
    const auto rgen=take(racing_ime.state(racing)).generation;racing.move_during_read=true;
    check(!racing_ime.erase_surrounding_codepoints(racing,rgen,1,1)&&!racing.delivered,
          "A composing deletion used a selection changed during its bounded read");
}
void composition_region(){
    FixtureHost host;unsigned edits=0;
    auto field=take(host.tree->input_field({},"field",{.text="A😀Z"},[&](std::string){++edits;},{}));
    (void)take(host.tree->drawing_snapshot({200,40}));good(field.focus());AndroidImeProtocol ime;
    const auto gen=take(ime.state(host)).generation;good(ime.select(host,gen,3,1));
    good(ime.composing_region(host,gen,3,1));auto state=take(ime.state(host,gen));
    check(state.marked==TextRange{1,2}&&state.selection==TextRange{1,2}&&state.selection_reversed&&edits==0&&
          take(field.snapshot()).text=="A😀Z","Marking existing text changed its content or selection");
    check(!ime.composing_region(host,gen,2,3)&&take(ime.state(host,gen)).marked==TextRange{1,2}&&edits==0,
          "A split composition region changed the editor");
    good(ime.composing_region(host,gen,2,2));state=take(ime.state(host,gen));
    check(!state.marked&&state.selection==TextRange{1,2}&&state.selection_reversed&&edits==0,
          "An empty region inside a surrogate did not simply finish composition");
    good(ime.composing_region(host,gen,1,3));
    good(ime.composition(host,gen,"한",1));good(ime.select(host,gen,3,0));
    good(ime.composing_region(host,gen,2,0));state=take(ime.state(host,gen));
    check(state.marked==TextRange{0,2}&&state.selection==TextRange{0,3}&&state.selection_reversed&&
          take(field.snapshot()).text=="A한Z"&&edits==1,"Changing region did not commit the previous candidate");
    good(ime.commit(host,gen,"K",1));
    check(take(field.snapshot()).text=="KZ"&&edits==2,"Marked existing text was not replaced");
    good(ime.composing_region(host,gen,-100,100));state=take(ime.state(host,gen));
    check(state.marked==TextRange{0,2}&&state.selection==TextRange{1,0},"Composition region was not clamped");
    good(ime.composing_region(host,gen,1,1));state=take(ime.state(host,gen));
    check(!state.marked&&state.selection==TextRange{1,0}&&edits==2,"An empty region did not preserve text and selection");
    host.tree->close();

    FixtureHost limited;auto bounded=take(limited.tree->input_field({},"bounded",{.text="A",.maximum_length=1},{},{}));
    (void)take(limited.tree->drawing_snapshot({200,40}));good(bounded.focus());AndroidImeProtocol bounded_ime;
    const auto bgen=take(bounded_ime.state(limited)).generation;good(bounded_ime.composition(limited,bgen,"한글",1));
    auto before=take(bounded_ime.state(limited,bgen));
    check(!bounded_ime.composing_region(limited,bgen,0,0)&&take(bounded_ime.state(limited,bgen)).marked==before.marked&&
          take(bounded_ime.read(limited,bgen,{0,3})).text=="A한글","A clipped candidate was partly committed");
    limited.tree->close();
}
void region_publication(){
    const AndroidImePublication previous{1,5,3,1,1,3,0};
    auto current=previous;current[4]=0;current[5]=2;
    check(!android_ime_notify_selection(previous,current,true),"Region-only change notified the IME");
    check(android_ime_notify_selection(previous,current,false),"Ordinary marked change was not published");
    current[3]=0;check(android_ime_notify_selection(previous,current,true),"A concurrent selection change was suppressed");
    current=previous;current[1]=4;check(android_ime_notify_selection(previous,current,true),"A concurrent text change was suppressed");
    current=previous;current[0]=2;check(android_ime_notify_selection(previous,current,true),"A focus change was suppressed");
    check(android_ime_notify_selection({},previous,true),"Initial selection was suppressed");
    const std::optional<int> old_revision=1,new_revision=2;
    check(!android_ime_publish_required(previous,previous,old_revision,old_revision,false),
          "An unchanged Android publication was repeated");
    check(android_ime_publish_required(previous,previous,old_revision,new_revision,false),
          "A same-length text revision was not published");
    check(!android_ime_publish_required(previous,previous,old_revision,new_revision,true),
          "A region-only revision produced a redundant notification");
    current=previous;current[4]=0;current[5]=2;
    check(android_ime_publish_required(previous,current,old_revision,new_revision,true),
          "A changed composition region was not sent to the Java state");
}
}

int main(){
    FixtureHost host;auto container=take(host.tree->content_host({},"host",{240,120}));Widget field,reader,button;
    good(container.set_content([&](WidgetBuilder& builder)->core::Result<void>{
        auto root=take(builder.vstack({},"root"));
        field=take(builder.input_field(root,"field",{.text="A😀Z"}));
        reader=take(builder.input_field(root,"reader",{.text="read",.read_only=true}));
        button=take(builder.action_surface(root,"button",{.accessibility_name="Button",.width=40,.height=30}));return {};
    }));
    (void)take(host.tree->drawing_snapshot({240,120}));good(field.focus());AndroidImeProtocol ime;
    const auto initial=take(ime.state(host));check(initial.generation>0&&initial.length==4,"IME did not bind the focused editor");
    good(ime.select(host,initial.generation,1,3));
    check(take(ime.query_text(host,initial.generation,AndroidImeQuery::selected,65535))==u"😀",
          "IME selected-text query lost a surrogate pair");
    good(ime.select(host,initial.generation,3,3));
    check(take(ime.query_text(host,initial.generation,AndroidImeQuery::before,1))==std::u16string(1,char16_t(0xde00)),
          "IME UTF-16 query did not preserve a partial trailing surrogate");
    good(ime.select(host,initial.generation,1,1));
    check(take(ime.query_text(host,initial.generation,AndroidImeQuery::after,1))==std::u16string(1,char16_t(0xd83d)),
          "IME UTF-16 query did not preserve a partial leading surrogate");
    check(!take(ime.query_text(host,initial.generation,AndroidImeQuery::selected,65535)),
          "IME selected-text query did not return null for a collapsed selection");
    good(ime.select(host,initial.generation,1,3));
    good(ime.composition(host,initial.generation,"한",-1));auto state=take(ime.state(host,initial.generation));
    check(state.marked==TextRange{1,1}&&state.selection==TextRange{0,0}&&
          take(ime.read(host,initial.generation,{0,3})).text=="A한Z","IME composition or bounded read failed");
    good(ime.composition(host,initial.generation,"하",2));state=take(ime.state(host,initial.generation));
    check(state.selection==TextRange{3,0},"IME cursor offset was not retained");
    good(ime.finish(host,initial.generation));
    check(take(field.snapshot()).text=="A하Z"&&!take(field.snapshot()).marked_range,"IME finish did not commit the marked text");
    good(ime.select(host,initial.generation,0,1));good(ime.composition(host,initial.generation,"",1));
    check(take(ime.read(host,initial.generation,{0,2})).text=="하Z","Empty IME composition restored the selected text");
    good(ime.finish(host,initial.generation));
    check(take(field.snapshot()).text=="하Z","Empty IME composition did not commit");
    good(ime.commit(host,initial.generation,"😀",1));
    state=take(ime.state(host,initial.generation));check(state.selection==TextRange{2,0},"IME commit lost the cursor");
    auto split=ime.erase_surrounding(host,initial.generation,1,0);
    check(!split&&take(field.snapshot()).text=="😀하Z","IME deletion split a surrogate");
    good(ime.erase_surrounding(host,initial.generation,2,0));
    check(take(field.snapshot()).text=="하Z","IME surrounding deletion did not reach the editor");
    good(ime.select(host,initial.generation,0,1));
    good(ime.composition(host,initial.generation,"하",1));
    good(ime.erase_surrounding(host,initial.generation,0,0));
    good(ime.finish(host,initial.generation));
    good(reader.focus());const auto read_state=take(ime.state(host));
    check(read_state.generation!=initial.generation&&read_state.read_only,"IME did not retire the previous focus");
    check(!ime.commit(host,initial.generation,"stale",1)&&!ime.commit(host,read_state.generation,"edit",1)&&
          take(field.snapshot()).text=="하Z"&&take(reader.snapshot()).text=="read","Stale or read-only IME input was accepted");
    good(button.focus());check(!ime.state(host),"IME retained an unfocused editor");
    host.tree->close();check(!ime.state(host),"IME retained a closed widget tree");
    codepoint_deletion();
    revision_conflicts();
    selected_deletion();
    composing_deletion();
    composition_region();
    region_publication();
}
