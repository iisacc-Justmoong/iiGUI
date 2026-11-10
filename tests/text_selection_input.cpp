#include "../src/Widgets/WidgetState.h"
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
using namespace ii;
using namespace ii::gui;
using namespace ii::gui::detail;
namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(core::Result<T> value){if(!value)throw std::runtime_error(value.error().message);return std::move(*value);}
void good(core::Result<void> value){if(!value)throw std::runtime_error(value.error().message);}
WidgetInput selection(std::size_t first,std::size_t length){
    WidgetInput input;input.kind=WidgetInputKind::text_selection;input.replacement_range=TextRange{first,length};return input;
}
WidgetInput reverse_selection(std::size_t first,std::size_t length){
    auto input=selection(first,length);input.selection_reversed=true;return input;
}
void retirement_during_query(bool during_read){
    auto tree=std::make_shared<WidgetState>([](runtime::Task task){task();});
    auto root=take(tree->absolute_stack({},"root",{100,30}));
    bool retire=false;
    auto provider=std::make_shared<TextInputProvider>();
    provider->state=[&]()->core::Result<TextInputState>{
        if(retire&&!during_read)tree->close();return TextInputState{.length=1};
    };
    provider->read=[&](TextRange range)->core::Result<TextContent>{
        if(retire&&during_read)tree->close();return TextContent{range,{}};
    };
    good(root.set_text_input_provider(provider));
    good(root.set_input_handler([](const WidgetInput&)->core::Result<bool>{return true;}));
    (void)take(tree->drawing_snapshot({100,30}));good(root.focus());retire=true;
    auto result=tree->input(selection(0,0));
    check(!result&&result.error().code==core::ErrorCode::closed,"Retired text owner accepted a selection");
}
void popup_selection(){
    auto tree=std::make_shared<WidgetState>([](runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{200,100}));Widget popup,field;
    good(host.set_content([&](WidgetBuilder& builder)->core::Result<void>{
        auto root=take(builder.absolute_stack({},"root",{200,100}));
        popup=take(builder.popover(root,"popup",{.width=120,.height=80}));return {};
    }));
    good(popup.set_popup_content([&](WidgetBuilder& builder)->core::Result<void>{
        auto root=take(builder.vstack({},"body"));field=take(builder.input_field(root,"field",{.text="A😀B"}));return {};
    }));
    good(popup.open_at(20,10));(void)take(tree->drawing_snapshot({200,100}));good(field.focus());
    good(tree->input(selection(1,2)));
    check(take(field.snapshot()).text_selection==TextRange{1,2}&&take(popup.snapshot()).opened,
          "Selection did not follow focus into the popup");
    tree->close();
}
void limited_commit_selection(){
    auto tree=std::make_shared<WidgetState>([](runtime::Task task){task();});
    auto field=take(tree->input_field({},"field",{.text="AB",.maximum_length=2},{},{}));
    (void)take(tree->drawing_snapshot({120,40}));good(field.focus());good(tree->input(selection(1,1)));
    good(tree->input({.kind=WidgetInputKind::composition,.text="😀한"}));
    good(tree->input(reverse_selection(1,3)));good(tree->input({.kind=WidgetInputKind::commit_composition}));
    const auto result=take(field.snapshot());
    check(result.text=="A😀"&&result.text_selection==TextRange{1,2}&&result.text_selection_reversed,
          "Length-limited composition lost its truncated directional selection");
    tree->close();
}
void replacement_cursor(){
    auto tree=std::make_shared<WidgetState>([](runtime::Task task){task();});
    auto field=take(tree->input_field({},"field",{.text="A😀Z"},{},{}));
    (void)take(tree->drawing_snapshot({120,40}));good(field.focus());good(tree->input(selection(0,1)));
    auto invalid=tree->input({.kind=WidgetInputKind::composition,.text="K",.text_cursor_offset=2});
    check(!invalid&&take(field.snapshot()).text=="A😀Z"&&!take(field.snapshot()).marked_range&&
          take(field.snapshot()).text_selection==TextRange{0,1},"A split cursor partially changed legacy input");
    auto invalid_selection=tree->input({.kind=WidgetInputKind::text,.text="K",.replacement_range=TextRange{0,1},
                                        .text_selection_after=TextRange{2,0}});
    check(!invalid_selection&&take(field.snapshot()).text=="A😀Z"&&take(field.snapshot()).text_selection==TextRange{0,1},
          "A split post-replacement selection partially changed legacy input");
    good(tree->input(selection(1,2)));
    good(tree->input({.kind=WidgetInputKind::composition,.text="한",.text_cursor_offset=-1}));
    check(take(field.text_content({0,3})).text=="A한Z"&&take(field.snapshot()).text_selection==TextRange{0,0},
          "Legacy composition cursor did not move before the marked range");
    good(tree->input({.kind=WidgetInputKind::composition,.text="하",.text_cursor_offset=2}));
    check(take(field.snapshot()).text_selection==TextRange{3,0},"Legacy composition cursor did not move after the marked range");
    good(tree->input({.kind=WidgetInputKind::text,.text="한",.text_cursor_offset=2}));
    check(take(field.snapshot()).text=="A한Z"&&take(field.snapshot()).text_selection==TextRange{3,0},
          "Legacy committed input ignored the cursor offset");
    good(tree->input(selection(1,1)));good(tree->input({.kind=WidgetInputKind::composition,.text_cursor_offset=1}));
    check(take(field.text_content({0,2})).text=="AZ"&&take(field.snapshot()).marked_range.has_value(),
          "An empty legacy composition restored the selected text");
    good(tree->input({.kind=WidgetInputKind::commit_composition}));
    check(take(field.snapshot()).text=="AZ"&&!take(field.snapshot()).marked_range,
          "Empty legacy composition did not commit deletion");
    tree->close();
}
void text_service_target(){
    auto tree=std::make_shared<WidgetState>([](runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{200,100}));Widget field,second,other;int interceptions=0;
    good(host.set_input_handler([&](const WidgetInput& event)->core::Result<bool>{
        if(event.text_service){++interceptions;return true;}return false;
    }));
    good(host.set_content([&](WidgetBuilder& builder)->core::Result<void>{
        auto root=take(builder.vstack({},"root"));
        good(root.set_input_handler([&](const WidgetInput& event)->core::Result<bool>{
            if(event.text_service){++interceptions;return true;}return false;
        }));
        field=take(builder.input_field(root,"field",{.text="A"}));
        second=take(builder.input_field(root,"second",{.text="second"}));
        other=take(builder.action_surface(root,"other",{.accessibility_name="Other",.width=60,.height=30}));return {};
    }));
    (void)take(tree->drawing_snapshot({200,100}));good(field.focus());
    auto snapshots=take(tree->accessibility_snapshots());std::string target;
    for(const auto& item:snapshots)if(item.focused&&item.text_input)target=item.id;
    check(!target.empty(),"Focused service target was not exposed");
    good(tree->input({.kind=WidgetInputKind::text,.text="B",.target=target,.text_cursor_offset=1,.text_service=true}));
    check(take(field.snapshot()).text=="AB"&&interceptions==0,"Ancestor consumed a focused text-service edit");
    good(second.focus());auto stale_finish=tree->input({.kind=WidgetInputKind::commit_composition,.target=target,.text_service=true});
    check(!stale_finish&&take(second.snapshot()).text=="second","A stale composition finish was reported as successful");
    good(other.focus());auto stale=tree->input({.kind=WidgetInputKind::text,.text="C",.target=target,.text_cursor_offset=1,.text_service=true});
    check(!stale&&take(field.snapshot()).text=="AB"&&interceptions==0,"A stale text-service target accepted input");
    tree->close();
}
void text_revision_guards(){
    auto tree=std::make_shared<WidgetState>([](runtime::Task task){task();});
    auto field=take(tree->input_field({},"field",{.text="ab"},{},{}));
    (void)take(tree->drawing_snapshot({120,40}));good(field.focus());
    const auto original=take(field.snapshot());check(original.text_revision.has_value(),"Built-in input has no revision");
    good(field.set_text("cd"));auto after=take(field.snapshot());
    check(after.text_revision&&after.text_revision!=original.text_revision,"A same-length input replacement reused a revision");
    WidgetInput stale;stale.kind=WidgetInputKind::text;stale.text="x";stale.target=after.id;stale.text_service=true;
    stale.expected_text_revision=original.text_revision;
    check(!tree->input(stale)&&take(field.snapshot()).text=="cd","A stale input reached the built-in field");
    auto fresh=stale;fresh.expected_text_revision=take(field.snapshot()).text_revision;good(tree->input(fresh));
    const auto value=take(field.snapshot()).text;
    check(!tree->input(fresh)&&take(field.snapshot()).text==value,"A delivered input revision was reused");
    tree->close();

    auto second=std::make_shared<WidgetState>([](runtime::Task task){task();});
    auto same_id=take(second->input_field({},"field",{.text="ab"},{},{}));
    (void)take(second->drawing_snapshot({120,40}));good(same_id.focus());
    check(take(same_id.snapshot()).text_revision!=original.text_revision,"A replacement owner reused the old input identity");
    second->close();

    std::uint64_t revision=0;bool mutate=false,change_before_handler=false;unsigned delivered=0,invoked=0;
    auto custom_tree=std::make_shared<WidgetState>([&](runtime::Task task){
        if(change_before_handler){++revision;change_before_handler=false;}task();
    });
    auto custom=take(custom_tree->absolute_stack({},"custom",{100,30}));
    auto provider=std::make_shared<TextInputProvider>();
    provider->state=[&]()->core::Result<TextInputState>{return TextInputState{.length=2,.revision=revision};};
    provider->read=[&](TextRange range)->core::Result<TextContent>{if(mutate){++revision;mutate=false;}return TextContent{range,{}};};
    good(custom.set_text_input_provider(provider));good(custom.set_input_handler([&](const WidgetInput& input)->core::Result<bool>{
        ++invoked;
        if(!input.expected_text_revision||input.expected_text_revision->value!=revision)
            return core::fail(core::ErrorCode::busy,"Source revision changed before edit");
        ++revision;++delivered;return true;
    }));
    (void)take(custom_tree->drawing_snapshot({100,30}));good(custom.focus());
    const auto snapshot=take(custom.snapshot());check(snapshot.text_revision.has_value(),"Versioned custom input has no revision");
    WidgetInput selected;selected.kind=WidgetInputKind::text_selection;selected.target=snapshot.id;
    selected.replacement_range=TextRange{1,0};selected.text_service=true;selected.expected_text_revision=snapshot.text_revision;
    mutate=true;check(!custom_tree->input(selected)&&delivered==0,"A change during boundary validation reached the custom handler");
    selected.expected_text_revision=take(custom.snapshot()).text_revision;good(custom_tree->input(selected));
    check(delivered==1&&!custom_tree->input(selected)&&delivered==1,"A guarded provider edit was not atomic or a stale token was reused");
    selected.expected_text_revision=take(custom.snapshot()).text_revision;const auto before_invoke=invoked;change_before_handler=true;
    check(!custom_tree->input(selected)&&invoked==before_invoke&&delivered==1,
          "An invocation-time source change reached the text handler");
    WidgetInput typed;typed.kind=WidgetInputKind::text;typed.text="x";typed.target=snapshot.id;typed.text_service=true;
    typed.expected_text_revision=take(custom.snapshot()).text_revision;change_before_handler=true;
    check(!custom_tree->input(typed)&&invoked==before_invoke&&delivered==1,
          "An invocation-time source change reached the committed text handler");
    custom_tree->close();
}
void accessibility_text_replacement(){
    unsigned notifications=0;WidgetFrameHooks hooks;hooks.accessibility_value_changed=[&]{++notifications;};
    auto tree=std::make_shared<WidgetState>([](runtime::Task task){task();},WidgetAppearance{},hooks);
    auto root=take(tree->absolute_stack({},"root",{400,160}));unsigned edits=0;std::string last;
    auto field=take(tree->input_field(root,"field",{.text="old"},[&](std::string value){++edits;last=std::move(value);},{}));
    auto other=take(tree->input_field(root,"other",{.text="keep"},{},{}));
    auto readonly=take(tree->input_field(root,"readonly",{.text="fixed",.read_only=true},{},{}));
    auto secret=take(tree->input_field(root,"secret",{.text="private",.password=true},{},{}));
    good(field.set_frame({0,0,100,30}));good(other.set_frame({0,40,100,30}));
    good(readonly.set_frame({0,80,100,30}));good(secret.set_frame({0,120,100,30}));
    (void)take(tree->drawing_snapshot({400,160}));good(other.focus());
    auto before=take(field.snapshot());check(before.text_revision.has_value(),"Missing field revision");notifications=0;
    good(tree->replace_accessibility_text(before.id,*before.text_revision,"한😀"));
    check(take(field.snapshot()).text=="한😀"&&edits==1&&last=="한😀"&&take(other.snapshot()).focused&&notifications==1,
          "Unfocused accessibility edit did not preserve focus and normal edit callback");
    check(!tree->replace_accessibility_text(before.id,*before.text_revision,"stale")&&take(field.snapshot()).text=="한😀"&&edits==1,
          "A stale accessibility edit replaced newer text");
    auto fixed=take(readonly.snapshot());auto protected_value=take(secret.snapshot());
    check(!tree->replace_accessibility_text(fixed.id,*fixed.text_revision,"changed")&&take(readonly.snapshot()).text=="fixed",
          "Accessibility edited a read-only field");
    check(!tree->replace_accessibility_text(protected_value.id,*protected_value.text_revision,"changed")&&take(secret.snapshot()).text=="private",
          "Accessibility edited a password field");
    tree->close();
}
}
int main(){
    auto tree=std::make_shared<WidgetState>([](runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{300,130}));Widget field,readonly,custom;
    int ancestor_calls=0,custom_calls=0;bool custom_accepts=true,custom_reversed=false;TextRange custom_selection{};
    good(host.set_input_handler([&](const WidgetInput& event)->core::Result<bool>{
        if(event.kind==WidgetInputKind::text_selection){++ancestor_calls;return true;}return false;
    }));
    good(host.set_content([&](WidgetBuilder& builder)->core::Result<void>{
        auto root=take(builder.absolute_stack({},"root",{300,130}));
        field=take(builder.input_field(root,"field",{.text="A😀B"}));
        readonly=take(builder.input_field(root,"readonly",{.text="한😀",.read_only=true}));
        custom=take(builder.absolute_stack(root,"custom",{100,30}));
        auto provider=std::make_shared<TextInputProvider>();
        provider->state=[&]()->core::Result<TextInputState>{return TextInputState{.length=4,.selection=custom_selection,.selection_reversed=custom_reversed};};
        provider->read=[](TextRange range)->core::Result<TextContent>{
            if(range.location==2)return core::fail(core::ErrorCode::out_of_range,"Split surrogate");
            return TextContent{range,{}};
        };
        good(custom.set_text_input_provider(provider));
        good(custom.set_input_handler([&](const WidgetInput& event)->core::Result<bool>{
            if(event.kind!=WidgetInputKind::text_selection)return false;
            ++custom_calls;if(!custom_accepts)return false;custom_selection=*event.replacement_range;custom_reversed=event.selection_reversed;return true;
        }));
        good(field.set_frame({0,0,100,30}));good(readonly.set_frame({0,40,100,30}));good(custom.set_frame({0,80,100,30}));return {};
    }));
    (void)take(tree->drawing_snapshot({300,130}));good(field.focus());
    good(tree->input(selection(1,2)));
    check(take(field.snapshot()).text_selection==TextRange{1,2},"UTF-16 selection did not reach nested editor");
    good(tree->input(reverse_selection(1,2)));
    check(take(field.snapshot()).text_selection==TextRange{1,2}&&take(field.snapshot()).text_selection_reversed,
          "Legacy editor lost directional selection");
    check(!tree->input(reverse_selection(1,0))&&take(field.snapshot()).text_selection_reversed,
          "An empty reverse selection changed the editor");
    auto invalid=tree->input(selection(2,1));
    check(!invalid&&take(field.snapshot()).text_selection==TextRange{1,2},"Surrogate-splitting selection changed the editor");
    check(!tree->input({.kind=WidgetInputKind::text_selection}),"Selection without a range was accepted");
    check(!tree->input(selection((std::numeric_limits<std::size_t>::max)(),1)),"Overflowing selection was accepted");
    good(tree->input({.kind=WidgetInputKind::text,.text="한"}));
    check(take(field.snapshot()).text=="A한B","Text did not replace the requested selection");
    good(tree->input(selection(1,0)));
    good(tree->input({.kind=WidgetInputKind::composition,.text="😀"}));
    good(tree->input(selection(1,2)));
    check(take(field.snapshot()).marked_range==TextRange{1,2}&&take(field.snapshot()).text_selection==TextRange{1,2},
          "Selection changed or lost the active composition");
    good(tree->input(reverse_selection(1,2)));
    check(take(field.snapshot()).text_selection_reversed&&take(field.snapshot()).marked_range==TextRange{1,2},
          "Composition lost directional selection");
    check(!tree->input(selection(2,0))&&take(field.snapshot()).text_selection==TextRange{1,2},
          "Invalid selection changed legacy composition");
    good(tree->input(selection(0,0)));
    check(take(field.snapshot()).text_selection==TextRange{0,0}&&take(field.snapshot()).marked_range.has_value(),
          "Selection outside the marked range interrupted composition");
    good(tree->input(reverse_selection(1,2)));
    good(tree->input({.kind=WidgetInputKind::commit_composition}));
    check(take(field.snapshot()).text=="A😀한B"&&take(field.snapshot()).text_selection==TextRange{1,2}&&
          take(field.snapshot()).text_selection_reversed&&!take(field.snapshot()).marked_range,
          "Legacy composition commit lost directional selection");
    good(readonly.focus());good(tree->input(selection(1,2)));
    check(take(readonly.snapshot()).text_selection==TextRange{1,2}&&take(readonly.snapshot()).text=="한😀",
          "Read-only editor rejected selection or changed its value");
    good(custom.focus());good(tree->input(reverse_selection(1,2)));
    check(custom_calls==1&&custom_selection==TextRange{1,2}&&custom_reversed&&take(custom.snapshot()).text_selection_reversed&&
          ancestor_calls==0,"Directional selection was intercepted before its focused editor");
    check(!tree->input(selection(2,0))&&custom_calls==1,"Invalid custom Unicode boundary reached its handler");
    custom_accepts=false;auto declined=tree->input(selection(0,0));
    check(!declined&&declined.error().code==core::ErrorCode::unsupported&&custom_selection==TextRange{1,2},
          "A declined selection was reported as successful");
    custom_selection={0,0};custom_reversed=true;auto bad_state=tree->input(selection(0,0));
    check(!bad_state&&bad_state.error().code==core::ErrorCode::invalid_argument,"An empty reverse provider selection was accepted");
    good(tree->input({.kind=WidgetInputKind::focus_lost}));
    auto unfocused=tree->input(selection(0,0));
    check(!unfocused&&unfocused.error().code==core::ErrorCode::unsupported,"Unfocused selection reported success");
    tree->close();
    retirement_during_query(false);retirement_during_query(true);popup_selection();limited_commit_selection();replacement_cursor();text_service_target();text_revision_guards();accessibility_text_replacement();
}
