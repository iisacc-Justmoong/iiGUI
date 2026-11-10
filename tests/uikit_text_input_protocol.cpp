#include "../src/Platform/UIKitTextInputProtocol.h"
#include "../src/Widgets/WidgetState.h"
#include <array>
#include <limits>
#include <memory>
#include <stdexcept>

using namespace ii;
using namespace ii::gui;
using namespace ii::gui::detail;
namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(core::Result<T> value){if(!value)throw std::runtime_error(value.error().message);return std::move(*value);}
void good(core::Result<void> value){if(!value)throw std::runtime_error(value.error().message);}
struct Host final {
    std::shared_ptr<WidgetState> tree=std::make_shared<WidgetState>([](runtime::Task task){task();});
    std::optional<WidgetSnapshot> focus(){
        auto nodes=tree->accessibility_snapshots();if(!nodes)return {};
        for(const auto& node:*nodes)if(node.focused&&node.text_input&&node.visible&&node.enabled&&!node.input_blocked)return node;
        return {};
    }
    core::Result<TextContent> read(std::string_view id,TextRange range){auto w=tree->find(id);if(!w)return std::unexpected(w.error());return w->text_content(range);}
    core::Result<TextInputGeometry> geometry(std::string_view id,TextRange range){auto w=tree->find(id);if(!w)return std::unexpected(w.error());return w->text_geometry(range);}
    core::Result<std::size_t> position(std::string_view id,double x,double y){auto w=tree->find(id);if(!w)return std::unexpected(w.error());return w->text_position(x,y);}
    core::Result<void> send(WidgetInput event){return tree->input(event);}
};
void composition(){
    Host host;auto field=take(host.tree->input_field({},"field",{.text="A😀Z"},{},{}));
    (void)take(host.tree->drawing_snapshot({200,40}));good(field.focus());
    UIKitTextInputProtocol input;const auto generation=take(input.state(host)).generation;
    check(take(input.read(host,generation,{0,4}))==u"A😀Z","UIKit read lost UTF-16 text");
    const auto half=take(input.read(host,generation,{1,1}));
    check(half.size()==1&&half[0]==0xd83d,"UIKit text read failed to return a partial surrogate");
    good(input.select(host,generation,{1,2}));
    check(!input.select(host,generation,{2,0})&&!input.compose(host,generation,"😀",{1,0})&&
          !input.compose(host,generation,"한",{0,2}),"UIKit accepted a split or invalid selection");
    good(input.compose(host,generation,"하나",{0,1}));auto first=take(input.state(host,generation));
    check(first.marked==TextRange{1,2}&&first.selection==TextRange{1,1}&&
          take(input.read(host,generation,{0,4}))==u"A하나Z","UIKit marked selection was not relative to its candidate");
    check(!input.select(host,generation,{5,0})&&take(input.state(host,generation)).marked==first.marked,
          "UIKit accepted an out-of-range selection or changed the candidate");
    good(input.compose(host,generation,"한",{1,0}));auto second=take(input.state(host,generation));
    check(second.marked==TextRange{1,1}&&second.selection==TextRange{2,0},"UIKit candidate update lost its caret");
    good(input.finish(host,generation));check(take(field.snapshot()).text=="A한Z"&&!take(input.state(host,generation)).marked,
        "UIKit failed to commit its marked candidate");
    good(input.replace(host,generation,{0,1},"K"));
    check(take(field.snapshot()).text=="K한Z"&&take(input.state(host,generation)).selection==TextRange{1,0},
          "UIKit replacement failed to update text and caret together");
    good(input.select(host,generation,{1,1}));good(input.compose(host,generation,"",{0,0}));
    check(take(input.state(host,generation)).marked==TextRange{1,0}&&take(input.read(host,generation,{0,2}))==u"KZ",
          "An empty UIKit candidate canceled instead of replacing the selection");
    good(input.finish(host,generation));check(take(field.snapshot()).text=="KZ","Empty UIKit composition did not commit");
    host.tree->close();
}
void selection_outside_composition(){
    Host host;unsigned edits=0;
    auto field=take(host.tree->input_field({},"field",{.text="A😀Z"},[&](std::string){++edits;},{}));
    (void)take(host.tree->drawing_snapshot({200,40}));good(field.focus());UIKitTextInputProtocol input;
    const auto generation=take(input.state(host)).generation;
    good(input.select(host,generation,{1,2}));good(input.compose(host,generation,"하나",{1,0}));
    good(input.select(host,generation,{1,1}));
    check(take(input.state(host,generation)).marked==TextRange{1,2}&&edits==0,
          "Selecting inside a UIKit candidate committed it");
    good(input.select(host,generation,{0,1}));
    auto state=take(input.state(host,generation));
    check(!state.marked&&state.selection==TextRange{0,1}&&take(field.snapshot()).text=="A하나Z"&&edits==1,
          "UIKit failed to commit the candidate and set the outside selection together");
    good(input.select(host,generation,{1,2}));good(input.compose(host,generation,"",{0,0}));
    good(input.select(host,generation,{0,0}));
    state=take(input.state(host,generation));
    check(!state.marked&&state.selection==TextRange{0,0}&&take(field.snapshot()).text=="AZ"&&edits==2,
          "UIKit failed to commit an empty candidate while moving selection");
    host.tree->close();
}
void selection_outside_composition_invalid(){
    Host host;unsigned edits=0;
    auto field=take(host.tree->input_field({},"field",{.text="😀Z"},[&](std::string){++edits;},{}));
    (void)take(host.tree->drawing_snapshot({200,40}));good(field.focus());UIKitTextInputProtocol input;
    const auto generation=take(input.state(host)).generation;
    good(input.select(host,generation,{2,0}));good(input.compose(host,generation,"한",{1,0}));
    auto before=take(input.state(host,generation));
    check(!input.select(host,generation,{1,0})&&take(input.state(host,generation)).marked==before.marked&&
          take(input.state(host,generation)).selection==before.selection&&take(input.read(host,generation,{0,4}))==u"😀한Z"&&edits==0,
          "A selection that splits a surrogate partially committed its candidate");
    host.tree->close();
    Host bounded;auto limited=take(bounded.tree->input_field({},"limited",{.text="AZ",.maximum_length=2},{},{}));
    (void)take(bounded.tree->drawing_snapshot({200,40}));good(limited.focus());UIKitTextInputProtocol limited_input;
    const auto token=take(limited_input.state(bounded)).generation;
    good(limited_input.select(bounded,token,{1,0}));good(limited_input.compose(bounded,token,"하나",{1,0}));
    const auto old=take(limited_input.state(bounded,token));
    check(!limited_input.select(bounded,token,{4,0})&&take(limited_input.state(bounded,token)).marked==old.marked&&
          take(limited_input.read(bounded,token,{0,4}))==u"A하나Z"&&take(limited.snapshot()).text=="AZ",
          "A selection invalid after maximum-length clipping partially committed its candidate");
    bounded.tree->close();
}
void replacement_during_composition(){
    struct Case {TextRange range;const char* inserted;const char* expected;std::size_t caret;};
    const Case cases[]={{{0,1},"X","X한글CDE",1},{{4,1},"Q","A한글CQE",5},
        {{2,1},"나","A한나CDE",3},{{0,4},"K","KDE",1},{{6,0},"!","A한글CDE!",7},
        {{0,0},"!","!A한글CDE",1},{{1,2},"","ACDE",1},{{2,0},"😀","A한😀글CDE",4}};
    for(const auto& c:cases){
        Host host;unsigned edits=0;
        auto field=take(host.tree->input_field({},"field",{.text="ABCDE"},[&](std::string){++edits;},{}));
        (void)take(host.tree->drawing_snapshot({200,40}));good(field.focus());UIKitTextInputProtocol input;
        const auto generation=take(input.state(host)).generation;
        good(input.select(host,generation,{1,1}));good(input.compose(host,generation,"한글",{1,0}));
        good(input.replace(host,generation,c.range,c.inserted));const auto state=take(input.state(host,generation));
        check(!state.marked&&state.selection==TextRange{c.caret,0}&&take(field.snapshot()).text==c.expected&&edits==1,
              "UIKit replacement failed to commit the projected text and caret atomically");
        host.tree->close();
    }
    Host host;auto field=take(host.tree->input_field({},"limited",{.text="AZ",.maximum_length=2},{},{}));
    (void)take(host.tree->drawing_snapshot({200,40}));good(field.focus());UIKitTextInputProtocol input;
    const auto generation=take(input.state(host)).generation;
    good(input.select(host,generation,{1,0}));good(input.compose(host,generation,"하나",{1,0}));
    good(input.replace(host,generation,{0,1},"Q"));
    check(take(field.snapshot()).text=="하나"&&take(input.state(host,generation)).selection==TextRange{0,0},
          "UIKit replacement exceeded the field limit when the candidate was overlong");
    host.tree->close();
}
void replacement_composition_matrix(){
    const std::string original="abc";
    for(std::size_t begin=0;begin<=original.size();++begin)
    for(std::size_t end=begin;end<=original.size();++end)
    for(const std::string candidate:std::array<std::string,3>{"","x","yz"}){
        const auto visible=original.substr(0,begin)+candidate+original.substr(end);
        for(std::size_t first=0;first<=visible.size();++first)
        for(std::size_t last=first;last<=visible.size();++last)
        for(const std::string inserted:std::array<std::string,2>{"","K"}){
            Host host;auto field=take(host.tree->input_field({},"field",{.text=original},{},{}));
            (void)take(host.tree->drawing_snapshot({100,40}));good(field.focus());UIKitTextInputProtocol input;
            const auto generation=take(input.state(host)).generation;
            good(input.select(host,generation,{begin,end-begin}));good(input.compose(host,generation,candidate,{candidate.size(),0}));
            good(input.replace(host,generation,{first,last-first},inserted));const auto state=take(input.state(host,generation));
            const auto expected=visible.substr(0,first)+inserted+visible.substr(last);
            check(!state.marked&&state.selection==TextRange{first+inserted.size(),0}&&take(field.snapshot()).text==expected,
                  "UIKit replacement matrix lost projected text or its caret");
            host.tree->close();
        }
    }
}
void focus_and_versions(){
    Host host;auto root=take(host.tree->absolute_stack({},"root",{200,80}));
    auto first=take(host.tree->input_field(root,"first",{.text="ab"},{},{}));
    auto other=take(host.tree->input_field(root,"other",{.text="cd"},{},{}));
    (void)take(host.tree->drawing_snapshot({200,80}));good(first.focus());UIKitTextInputProtocol input;
    auto old=take(input.state(host)).generation;good(other.focus());good(first.focus());
    auto current=take(input.state(host)).generation;
    check(current!=old&&!input.replace(host,old,{0,1},"X"),"An old UIKit position survived a focus ABA");
    check(!input.select(host,current,{(std::numeric_limits<std::size_t>::max)(),1})&&
          !input.read(host,current,{0,65537}),"UIKit accepted an unbounded range");
    host.tree->close();
}
void graphemes(){
    Host host;auto field=take(host.tree->input_field({},"field",{.text="Aé😀Z"},{},{}));
    (void)take(host.tree->drawing_snapshot({200,40}));good(field.focus());UIKitTextInputProtocol input;
    const auto generation=take(input.state(host)).generation;
    check(take(input.character(host,generation,1,true))==TextRange{1,2}&&
          take(input.character(host,generation,3,false))==TextRange{1,2}&&
          take(input.character(host,generation,2,true))==TextRange{1,2}&&
          take(input.character(host,generation,3,true))==TextRange{3,2}&&
          !input.character(host,generation,4,true),"UIKit character movement split a grapheme or surrogate");
    host.tree->close();
}
void deletion(){
    Host host;auto field=take(host.tree->input_field({},"field",{.text="Aé😀Z"},{},{}));
    (void)take(host.tree->drawing_snapshot({200,40}));good(field.focus());UIKitTextInputProtocol input;
    const auto generation=take(input.state(host)).generation;
    good(input.select(host,generation,{5,0}));good(input.delete_backward(host,generation));
    check(take(field.snapshot()).text=="AéZ"&&take(input.state(host,generation)).selection==TextRange{3,0},
          "UIKit backward deletion split an emoji");
    good(input.delete_backward(host,generation));
    check(take(field.snapshot()).text=="AZ"&&take(input.state(host,generation)).selection==TextRange{1,0},
          "UIKit backward deletion split a combining grapheme");
    good(input.compose(host,generation,"하나",{2,0}));good(input.delete_backward(host,generation));
    check(take(input.read(host,generation,{0,3}))==u"A하Z"&&take(input.state(host,generation)).marked==TextRange{1,1},
          "UIKit deletion committed or lost the marked candidate");
    good(input.delete_backward(host,generation));
    check(take(input.read(host,generation,{0,2}))==u"AZ"&&take(input.state(host,generation)).marked==TextRange{1,0},
          "UIKit deletion did not preserve an empty marked candidate");
    good(input.delete_backward(host,generation));
    check(take(input.read(host,generation,{0,1}))==u"Z"&&take(input.state(host,generation)).marked==TextRange{0,0},
          "UIKit deletion before an empty candidate lost its composition");
    host.tree->close();
}
void provider_geometry(){
    Host host;auto field=take(host.tree->absolute_stack({},"editor",{100,40}));
    std::uint64_t revision=0;bool alter=false,alter_nonempty=false,versioned=true;unsigned edits=0;
    std::optional<TextRange> marked;
    auto provider=std::make_shared<TextInputProvider>();
    provider->state=[&]()->core::Result<TextInputState>{return TextInputState{.length=3,.selection={1,0},.marked=marked,.revision=versioned?std::optional{revision}:std::nullopt};};
    provider->read=[&](TextRange r)->core::Result<TextContent>{if(alter||(alter_nonempty&&r.length)){++revision;alter=false;alter_nonempty=false;}return TextContent{r,std::string(r.length,'a')};};
    provider->position=[](double,double y)->core::Result<std::size_t>{return y>=13?2:0;};
    good(field.set_text_input_provider(provider));
    good(field.set_input_handler([&](const WidgetInput&)->core::Result<bool>{++edits;return true;}));
    good(field.set_text_geometry_provider([](TextRange range)->core::Result<TextInputGeometry>{
        const auto at=[](std::size_t position){return Rect{float(2+(position%2)*8),float(position>=2?13:3),1,10};};
        return TextInputGeometry{at(range.location),at(range.location+range.length),{{2,3,8,10}}};
    }));
    (void)take(host.tree->drawing_snapshot({100,40}));good(field.focus());UIKitTextInputProtocol input;
    const auto generation=take(input.state(host)).generation;
    check(take(input.geometry(host,generation,{0,2})).selection.size()==1&&take(input.position(host,generation,10,25))==2,
          "UIKit geometry or hit testing did not use the focused provider");
    check(take(input.move(host,generation,0,UIKitTextDirection::down,1))==2&&
          take(input.move(host,generation,2,UIKitTextDirection::up,1))==0&&
          take(input.move(host,generation,0,UIKitTextDirection::right,1))==1&&
          !input.move(host,generation,0,UIKitTextDirection::left,1),
          "UIKit directional movement ignored line geometry or text boundaries");
    alter=true;auto rejected=input.replace(host,generation,{0,1},"x");
    check(!rejected&&rejected.error().code==core::ErrorCode::busy&&edits==0,
          "UIKit applied an edit validated against an obsolete text revision");
    alter=true;auto stale=input.read(host,generation,{0,1});
    check(!stale&&stale.error().code==core::ErrorCode::busy,"UIKit accepted a read from an obsolete text revision");
    marked=TextRange{1,1};alter_nonempty=true;auto selection=input.select(host,generation,{0,0});
    check(!selection&&selection.error().code==core::ErrorCode::busy&&edits==0,
          "UIKit committed an obsolete candidate after it changed during selection validation");
    versioned=false;check(!input.state(host)&&!input.compose(host,generation,"x",{1,0})&&edits==0,
          "UIKit accepted composition from an unversioned provider");
    host.tree->close();
}
void external_change_scope(){
    std::string text="old";std::vector<std::string> events;WidgetFrameHooks hooks;
    hooks.text_input_change=[&](bool value_changed)->runtime::Task{
        events.push_back(value_changed?"will-text:"+text:"will-selection:"+text);
        return [&]{events.push_back("did:"+text);};
    };
    auto tree=std::make_shared<WidgetState>([](runtime::Task task){task();},WidgetAppearance{},hooks);
    auto field=take(tree->absolute_stack({},"field",{100,40}));
    auto provider=std::make_shared<TextInputProvider>();
    provider->state=[&]()->core::Result<TextInputState>{return TextInputState{.length=text.size(),.revision=0};};
    provider->read=[&](TextRange range)->core::Result<TextContent>{return TextContent{range,text.substr(range.location,range.length)};};
    good(field.set_text_input_provider(provider));(void)take(tree->drawing_snapshot({100,40}));good(field.focus());
    {
        auto scope=take(field.begin_text_input_change(true));
        check(events==std::vector<std::string>{"will-text:old"},"Text will-change arrived after the mutation");
        text="new";
    }
    check(events==std::vector<std::string>({"will-text:old","did:new"}),"Text change scope did not close after mutation");
    {auto system=take(field.begin_text_input_change(true,true));text="ime";}
    check(events.size()==2,"UIKit-originated text notified its own input delegate");
    tree->close();
}
void builtin_external_change(){
    std::vector<std::string> events;Widget field;WidgetFrameHooks hooks;
    hooks.text_input_change=[&](bool value_changed)->runtime::Task{
        events.push_back(std::string(value_changed?"will-text:":"will-selection:")+take(field.snapshot()).text);
        return [&]{events.push_back("did:"+take(field.snapshot()).text);};
    };
    auto tree=std::make_shared<WidgetState>([](runtime::Task task){task();},WidgetAppearance{},hooks);
    field=take(tree->input_field({},"field",{.text="old"},{},{}));
    (void)take(tree->drawing_snapshot({100,40}));good(field.focus());
    good(field.set_text("new"));
    check(events==std::vector<std::string>({"will-text:old","did:new"}),
          "A native field changed before UIKit's will-change notification");
    tree->close();
}
void focus_publication(){
    unsigned notifications=0;WidgetFrameHooks hooks;hooks.accessibility_changed=[&]{++notifications;};
    auto tree=std::make_shared<WidgetState>([](runtime::Task task){task();},WidgetAppearance{},hooks);
    auto field=take(tree->input_field({},"field",{.text="A"},{},{}));
    (void)take(tree->drawing_snapshot({100,40}));const auto before=notifications;good(field.focus());
    check(notifications>before,"A top-level programmatic editor focus was not published");
    tree->close();
}
void field_traits_publication(){
    std::vector<WidgetSnapshot> published;Widget field;WidgetFrameHooks hooks;
    hooks.accessibility_changed=[&]{auto snapshot=field.snapshot();if(snapshot)published.push_back(std::move(*snapshot));};
    auto tree=std::make_shared<WidgetState>([](runtime::Task task){task();},WidgetAppearance{},hooks);
    field=take(tree->input_field({},"field",{.text="A"},{},{}));
    (void)take(tree->drawing_snapshot({100,40}));good(field.focus());published.clear();
    good(field.set_input_options({.text="A",.read_only=true,.multiline=true,.password=true}));
    check(!published.empty()&&published.back().read_only&&published.back().multiline&&published.back().password,
          "Focused input trait changes were not published after the update");
    published.clear();good(field.set_enabled(false));
    check(!published.empty()&&!published.back().enabled&&!published.back().focused,
          "Disabling the focused editor did not publish its final state");
    tree->close();
}
}
int main(){composition();selection_outside_composition();selection_outside_composition_invalid();replacement_during_composition();replacement_composition_matrix();focus_and_versions();graphemes();deletion();provider_geometry();external_change_scope();builtin_external_change();focus_publication();field_traits_publication();}
