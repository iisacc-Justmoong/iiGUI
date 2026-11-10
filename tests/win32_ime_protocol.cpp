#include "../src/Platform/Win32ImeProtocol.h"
#include "../src/Widgets/WidgetState.h"
#include <cstring>
#include <memory>
#include <stdexcept>

using namespace ii;
using namespace ii::gui;
using namespace ii::gui::detail;

namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(core::Result<T> value){if(!value)throw std::runtime_error(value.error().message);return std::move(*value);}
void good(core::Result<void> value){if(!value)throw std::runtime_error(value.error().message);}
struct Reader final {
    std::u16string committed,composing;std::int32_t cursor=0;
    bool odd=false,short_read=false,failed=false;unsigned calls=0;
    std::int32_t operator()(std::uint32_t kind,void* destination,std::uint32_t capacity){
        ++calls;if(failed)return -1;if(kind==win32_ime_cursor)return cursor;
        const auto& text=kind==win32_ime_result?committed:composing;
        const auto bytes=std::uint32_t(text.size()*sizeof(char16_t));
        if(!destination)return odd?std::int32_t(bytes+1):std::int32_t(bytes);
        if(capacity<bytes)return -1;std::memcpy(destination,text.data(),bytes);
        return std::int32_t(short_read&&bytes?bytes-2:bytes);
    }
};
struct Host final {
    std::shared_ptr<WidgetState> tree=std::make_shared<WidgetState>([](runtime::Task task){task();});
    std::optional<WidgetSnapshot> focus(){
        auto snapshots=tree->accessibility_snapshots();if(!snapshots)return {};
        for(auto& node:*snapshots)if(node.focused&&node.text_input&&node.visible&&node.enabled&&!node.input_blocked)return node;
        return {};
    }
    core::Result<void> send(WidgetInput event){return tree->input(std::move(event));}
};
void capture_contract(){
    Reader reader;reader.committed=u"한";reader.composing=u"😀글";reader.cursor=2;
    auto value=take(win32_ime_capture(reader,win32_ime_result|win32_ime_composition|win32_ime_cursor));
    check(value.committed==u"한"&&value.composing==u"😀글"&&value.cursor==2&&!value.cancel,
          "Windows IME dropped one of two simultaneous strings");
    const auto before=reader.calls;auto cancel=take(win32_ime_capture(reader,0));
    check(cancel.cancel&&reader.calls==before,"Windows IME cancellation read unavailable IMM data");
    auto moved=take(win32_ime_capture(reader,win32_ime_cursor));
    check(moved.composing==u"😀글"&&moved.cursor==2,"Cursor-only notification lost the current candidate");
    reader.cursor=-1;auto without_cursor=take(win32_ime_capture(reader,win32_ime_composition));
    check(!without_cursor.cursor&&without_cursor.composing==u"😀글"&&
          !win32_ime_capture(reader,win32_ime_cursor),"Windows IME mishandled unavailable optional and required cursors");
    reader.cursor=1;check(!win32_ime_capture(reader,win32_ime_composition|win32_ime_cursor),
          "Windows IME accepted a cursor inside a surrogate pair");
    reader.cursor=4;check(!win32_ime_capture(reader,win32_ime_composition|win32_ime_cursor),
          "Windows IME accepted an out-of-range cursor");
    reader.cursor=0;reader.odd=true;check(!win32_ime_capture(reader,win32_ime_result),"Windows IME accepted an odd byte count");
    reader.odd=false;reader.short_read=true;check(!win32_ime_capture(reader,win32_ime_result),"Windows IME accepted a partial IMM read");
    reader.short_read=false;reader.failed=true;check(!win32_ime_capture(reader,win32_ime_result),"Windows IME accepted an IMM error");
    reader.failed=false;reader.composing=std::u16string(1,char16_t(0xd800));
    check(!win32_ime_capture(reader,win32_ime_result|win32_ime_composition),"Windows IME accepted malformed UTF-16");
    reader.composing=std::u16string(1,u'\0');
    check(!win32_ime_capture(reader,win32_ime_composition),"Windows IME accepted an embedded NUL");
    reader.composing=std::u16string(65537,u'a');
    check(!win32_ime_capture(reader,win32_ime_composition),"Windows IME accepted an oversized candidate");
}
void composition_and_cancel(){
    Host host;unsigned edits=0;auto field=take(host.tree->input_field({},"field",{.text="A"},[&](std::string){++edits;},{}));
    (void)take(host.tree->drawing_snapshot({200,40}));good(field.focus());
    good(host.tree->input({.kind=WidgetInputKind::text_selection,.replacement_range=TextRange{1,0}}));
    Win32ImeProtocol ime;good(ime.start(host));Reader reader;reader.composing=u"ㅎ";reader.cursor=1;
    good(ime.apply(host,take(win32_ime_capture(reader,win32_ime_composition|win32_ime_cursor))));
    check(host.focus()->marked_range==TextRange{1,1}&&edits==0,"Windows preedit changed committed text");
    reader.composing=u"하나";reader.cursor=0;
    good(ime.apply(host,take(win32_ime_capture(reader,win32_ime_cursor))));
    check(host.focus()->marked_range==TextRange{1,2}&&host.focus()->text_selection==TextRange{1,0}&&edits==0,
          "Windows cursor-only update did not select the current candidate");
    reader.committed=u"한";reader.composing=u"글";reader.cursor=1;
    good(ime.apply(host,take(win32_ime_capture(reader,win32_ime_result|win32_ime_composition|win32_ime_cursor))));
    check(take(field.snapshot()).text=="A한"&&host.focus()->marked_range==TextRange{2,1}&&edits==1,
          "Windows result and next candidate were not delivered in order");
    good(ime.apply(host,take(win32_ime_capture(reader,0))));
    check(!host.focus()->marked_range&&take(field.snapshot()).text=="A한"&&edits==1,
          "Windows cancellation changed the committed result");
    reader.composing=u"";reader.cursor=0;
    good(ime.apply(host,take(win32_ime_capture(reader,win32_ime_composition|win32_ime_cursor))));
    check(host.focus()->marked_range==TextRange{2,0},"An empty Windows candidate was treated as cancellation");
    good(ime.end(host));check(!host.focus()->marked_range&&take(field.snapshot()).text=="A한","Windows IME end did not clear its candidate");
    host.tree->close();
}
void focus_changes(){
    Host host;Widget first,other;bool switch_and_return=false;
    auto root=take(host.tree->absolute_stack({},"root",{200,40}));
    first=take(host.tree->input_field(root,"first",{.text="A"},[&](std::string){
        good(other.focus());if(switch_and_return)good(first.focus());
    },{}));
    other=take(host.tree->input_field(root,"other",{.text="B"},{},{}));
    (void)take(host.tree->drawing_snapshot({200,40}));good(first.focus());
    Win32ImeProtocol ime;good(ime.start(host));Reader reader;reader.committed=u"한";reader.composing=u"글";reader.cursor=1;
    const auto packet=take(win32_ime_capture(reader,win32_ime_result|win32_ime_composition|win32_ime_cursor));
    auto moved=ime.apply(host,packet);
    check(!moved&&take(first.snapshot()).text=="A한"&&take(other.snapshot()).text=="B"&&!take(other.snapshot()).marked_range,
          "A Windows IME continuation reached the newly focused editor");
    good(ime.end(host));good(first.focus());switch_and_return=true;good(ime.start(host));
    auto aba=ime.apply(host,packet);
    check(!aba&&take(first.snapshot()).text=="A한한"&&!take(first.snapshot()).marked_range,
          "A Windows IME continuation survived a focus ABA");
    good(ime.end(host));host.tree->close();
}
void malformed_packet_is_atomic(){
    Host host;auto field=take(host.tree->input_field({},"field",{.text="A"},{},{}));
    (void)take(host.tree->drawing_snapshot({200,40}));good(field.focus());Win32ImeProtocol ime;good(ime.start(host));
    Win32ImeMessage message;message.committed=u"한";message.composing=std::u16string(1,char16_t(0xd800));message.cursor=0;
    check(!ime.apply(host,message)&&take(field.snapshot()).text=="A"&&!host.focus()->marked_range,
          "A malformed second string partially committed the first");
    host.tree->close();
}
void legacy_and_reentry(){
    Host host;auto field=take(host.tree->absolute_stack({},"legacy",{100,40}));
    auto provider=std::make_shared<TextInputProvider>();
    provider->state=[]()->core::Result<TextInputState>{return TextInputState{.length=0,.selection={0,0}};};
    provider->read=[](TextRange range)->core::Result<TextContent>{return TextContent{range,{}};};
    good(field.set_text_input_provider(provider));Win32ImeProtocol ime;unsigned calls=0;bool nested_rejected=false;
    good(field.set_input_handler([&](const WidgetInput& event)->core::Result<bool>{
        ++calls;nested_rejected=!ime.start(host);return event.target=="legacy"&&event.text_service&&!event.expected_text_revision;
    }));
    (void)take(host.tree->drawing_snapshot({100,40}));good(field.focus());good(ime.start(host));
    Reader reader;reader.composing=u"한";reader.cursor=1;
    good(ime.apply(host,take(win32_ime_capture(reader,win32_ime_composition|win32_ime_cursor))));
    check(calls==1&&nested_rejected,"An unversioned Windows editor lost input or accepted nested IME delivery");
    ime.abandon();check(!ime.apply(host,take(win32_ime_capture(reader,win32_ime_composition))),
                        "An abandoned Windows IME session accepted a late message");
    good(ime.end(host));good(ime.start(host));host.tree->close();
}
void characters(){
    Win32Utf16Input input;WidgetSnapshot first{};first.id="first";
    first.text_revision=TextInputRevision{.identity=std::make_shared<int>(1),.focus=1};
    auto second=first;second.text_revision->focus=2;
    check(!take(input.accept(0xd83d,first))&&take(input.accept(0xde00,first))=="😀",
          "A Windows surrogate pair did not produce one Unicode scalar");
    check(!take(input.accept(0xde00,first)),"An unpaired low surrogate reached the editor");
    check(!take(input.accept(0xd83d,first))&&take(input.accept(u'X',first))=="X",
          "A malformed surrogate consumed the following valid character");
    check(!take(input.accept(0xd83d,first))&&!take(input.accept(0xde00,second)),
          "A surrogate pair crossed Windows focus generations");
    check(!take(input.accept(0xd83d,first))&&!take(input.accept(u'\n',first))&&
          !take(input.accept(0xde00,first)),"A control character did not clear a pending surrogate");
    check(!take(input.accept(0xd83d,first)),"A high surrogate produced premature text");
    input.clear();check(!take(input.accept(0xde00,first)),"A focus-loss reset retained a surrogate");
}
}
int main(){capture_contract();composition_and_cancel();focus_changes();malformed_packet_is_atomic();legacy_and_reentry();characters();}
