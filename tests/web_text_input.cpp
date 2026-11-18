#include "../src/Platform/WebTextInput.h"
#include "../src/Widgets/WidgetState.h"
#include "input_field_policy.h"
#include <iostream>
#include <stdexcept>

using namespace ii;
namespace {
void check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
void good(core::Result<void> value) { if (!value) throw std::runtime_error(value.error().message); }
template<class T> T take(core::Result<T> value) {
    if (!value) throw std::runtime_error(value.error().message);
    return std::move(*value);
}
void run() {
    using namespace gui;
    using namespace gui::detail;
    auto state = std::make_shared<WidgetState>([](runtime::Task task) { task(); });
    Widget field;
    good(state->construct([&](WidgetBuilder& builder)->core::Result<void> {
        auto created = builder.input_field({}, "account", {.text="a😀b"});
        if (!created) return std::unexpected(created.error());
        field = *created; return {};
    }));
    good(state->activate_paint_view());
    good(field.focus());
    auto snapshot = take(state->focused_text_editor());
    check(snapshot && snapshot->text_revision, "Focused editor lacks a stable identity");
    snapshot->attached=true; // The headless test has no OS surface.
    const auto revision = *snapshot->text_revision;
    check(web_text_same_focus(*snapshot, "account", revision), "Current focus rejected");
    auto other = *snapshot; other.id="other";
    check(!web_text_same_focus(other, "account", revision), "A different editor was accepted");
    other=*snapshot;++other.text_revision->focus;
    check(!web_text_same_focus(other, "account", revision), "A stale focus generation was accepted");

    WebTextMessage select{.action=WebTextAction::selection, .id="account", .selection_start=1, .selection_length=2};
    auto event=web_text_input(select); check(bool(event), "Selection event was rejected"); good(state->input(*event));
    const auto before_edit=*take(field.snapshot()).text_revision;
    WebTextMessage replace{.action=WebTextAction::replace, .id="account", .text="한", .start=1, .length=2,
                           .selection_start=2, .selection_length=0, .has_before=true};
    replace.before="a😀b";
    auto current=*take(state->focused_text_editor());current.attached=true;
    check(web_text_admissible(replace,current,"account",revision,"a😀b"),"Matching browser edit was rejected");
    check(!web_text_admissible(replace,current,"account",revision,"changed"),"Stale browser text was accepted");
    current.read_only=true;check(!web_text_admissible(replace,current,"account",revision,"a😀b"),"Read-only edit was accepted");
    current.read_only=false;replace.has_before=false;
    check(!web_text_admissible(replace,current,"account",revision,"a😀b"),"Unversioned browser edit was accepted");
    replace.has_before=true;
    event=web_text_input(replace); check(bool(event), "Replacement event was rejected");event->expected_text_revision=before_edit;good(state->input(*event));
    check(take(field.text_content({0,3})).text=="a한b", "UTF-16 selection replacement was incorrect");
    event=web_text_input(replace);event->expected_text_revision=before_edit;
    auto stale=state->input(*event);check(!stale&&stale.error().code==core::ErrorCode::busy && take(field.text_content({0,3})).text=="a한b",
          "A stale browser revision changed the editor");
    WebTextMessage composing{.action=WebTextAction::composition, .id="account", .text="ㅎ", .start=2,
                             .selection_start=1, .first_composition=true};
    event=web_text_input(composing);check(bool(event), "Composition was rejected");good(state->input(*event));
    composing.text="하";composing.first_composition=false;
    event=web_text_input(composing);check(bool(event), "Composition update was rejected");good(state->input(*event));
    check(take(field.snapshot()).composition_text=="하", "Preedit was not retained");
    WebTextMessage commit{.action=WebTextAction::commit, .id="account"};
    event=web_text_input(commit);check(bool(event), "Commit was rejected");good(state->input(*event));
    check(take(field.text_content({0,4})).text=="a한하b", "Composition was not committed once");
    composing.text="ㅎ";composing.start=3;composing.first_composition=true;
    event=web_text_input(composing);good(state->input(*event));
    WebTextMessage cancel{.action=WebTextAction::cancel, .id="account"};
    event=web_text_input(cancel);check(bool(event), "Cancellation was rejected");good(state->input(*event));
    check(take(field.text_content({0,4})).text=="a한하b" && !take(field.snapshot()).marked_range,
          "Canceled composition changed committed text");
    WebTextMessage tab{.action=WebTextAction::tab, .id="account", .shift=true};
    event=web_text_input(tab);check(event && event->key==WidgetKey::tab && event->shift && event->target.empty(),
                                   "Tab must use the focused hierarchy for navigation");
    WebTextMessage invalid{.action=WebTextAction::replace, .id="account", .text=std::string("\xff",1)};
    check(!web_text_input(invalid), "Invalid UTF-8 was accepted");
    invalid.text="ok";invalid.start=web_text_limit;invalid.length=1;
    check(!web_text_input(invalid), "An overflowing browser range was accepted");
    WebTextMessage empty{.action=WebTextAction::composition,.id="account"};
    event=web_text_input(empty);check(event&&event->text_cursor_offset==1,"Empty preedit was confused with cancellation");
    check(web_text_representable(false,false,"account") && !web_text_representable(false,false,"a\nb") &&
          !web_text_representable(false,true,"a\rb") && !web_text_representable(true,true,"secret"),
          "A browser input that changes or exposes its value was accepted");
    state->close();
}
void provider() {
    using namespace gui;
    using namespace gui::detail;
    using Field=BasicNativeInputField<input_field_test::Policy>;
    auto state=std::make_shared<WidgetState>([](runtime::Task task){task();});
    std::optional<Field> field;unsigned edits=0;
    good(state->construct([&](WidgetBuilder& widgets)->core::Result<void>{
        input_field_test::Configuration configuration;configuration.text="A😀Z";
        input_field_test::Callbacks callbacks;callbacks.edited=[&](std::string){++edits;};
        auto made=Field::create(widgets,{},"account",configuration,std::move(callbacks));
        if(!made)return std::unexpected(made.error());field=std::move(*made);return {};
    }));
    good(state->activate_paint_view());good(field->root().focus());
    const auto dispatch=[&](WebTextMessage message){
        auto event=web_text_input(message);check(bool(event),"Provider event was rejected");
        if(event->text_service){auto snapshot=take(field->root().snapshot());check(bool(snapshot.text_revision),"Provider omitted its revision");event->expected_text_revision=*snapshot.text_revision;}
        good(state->input(*event));
    };
    dispatch({.action=WebTextAction::selection,.id="account",.selection_start=1,.selection_length=2});
    dispatch({.action=WebTextAction::composition,.id="account",.text="ㅎ",.start=1,.length=2,.selection_start=1,.first_composition=true});
    dispatch({.action=WebTextAction::composition,.id="account",.text="한",.selection_start=1});
    check(take(field->snapshot()).marked.has_value()&&edits==0,"Provider published a preedit as committed text");
    dispatch({.action=WebTextAction::selection,.id="account",.selection_start=1,.selection_length=1});
    check(take(field->snapshot()).selection.length==1 && take(field->snapshot()).marked.has_value(),
          "Selection inside a preedit did not reach the provider");
    dispatch({.action=WebTextAction::selection,.id="account",.selection_start=2});
    dispatch({.action=WebTextAction::commit,.id="account"});
    check(take(field->snapshot()).text=="A한Z"&&edits==1,"Provider did not commit the composed account text exactly once");
    dispatch({.action=WebTextAction::replace,.id="account",.text="!",.start=2,.selection_start=3});
    check(take(field->snapshot()).text=="A한!Z"&&edits==2,"Provider did not apply a selected text replacement");
    state->close();
}
void nested_navigation() {
    using namespace gui;
    using namespace gui::detail;
    auto state=std::make_shared<WidgetState>([](runtime::Task task){task();});
    Widget first_host,second_host,first,second;
    good(state->construct([&](WidgetBuilder& builder)->core::Result<void>{
        auto root=take(builder.absolute_stack({},"root",{300,120}));
        first_host=take(builder.content_host(root,"first-host",{140,80}));
        second_host=take(builder.content_host(root,"second-host",{140,80}));
        good(first_host.set_frame({0,0,140,80}));
        return second_host.set_frame({150,0,140,80});
    }));
    good(first_host.set_content([&](WidgetBuilder& builder)->core::Result<void>{
        first=take(builder.input_field({},"username",{}));return {};
    }));
    good(second_host.set_content([&](WidgetBuilder& builder)->core::Result<void>{
        second=take(builder.input_field({},"password",{}));return {};
    }));
    (void)take(state->drawing_snapshot({300,120}));
    good(first.focus());
    const auto forward=web_text_input({.action=WebTextAction::tab,.id="username"});
    check(bool(forward),"Forward navigation event was rejected");
    good(state->input(*forward));
    check(take(second.snapshot()).focused,"Tab did not move between content hosts");
    const auto backward=web_text_input({.action=WebTextAction::tab,.id="password",.shift=true});
    check(bool(backward),"Backward navigation event was rejected");
    good(state->input(*backward));
    check(take(first.snapshot()).focused,"Shift-Tab did not return to the previous content host");
    state->close();
}
}
int main(){try{run();provider();nested_navigation();std::cout<<"WEB_TEXT_INPUT_OK\n";}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
