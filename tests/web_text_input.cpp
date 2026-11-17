#include "../src/Platform/WebTextInput.h"
#include "../src/Widgets/WidgetState.h"
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
    const auto revision = *snapshot->text_revision;
    check(web_text_same_focus(*snapshot, "account", revision), "Current focus rejected");
    auto other = *snapshot; other.id="other";
    check(!web_text_same_focus(other, "account", revision), "A different editor was accepted");

    WebTextMessage select{.action=WebTextAction::selection, .id="account", .selection_start=1, .selection_length=2};
    auto event=web_text_input(select); check(bool(event), "Selection event was rejected"); good(state->input(*event));
    WebTextMessage replace{.action=WebTextAction::replace, .id="account", .text="한", .start=1, .length=2,
                           .selection_start=2, .selection_length=0};
    event=web_text_input(replace); check(bool(event), "Replacement event was rejected"); good(state->input(*event));
    check(take(field.text_content({0,3})).text=="a한b", "UTF-16 selection replacement was incorrect");
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
    event=web_text_input(tab);check(event && event->key==WidgetKey::tab && event->shift, "Tab navigation lost direction");
    WebTextMessage invalid{.action=WebTextAction::replace, .id="account", .text=std::string("\xff",1)};
    check(!web_text_input(invalid), "Invalid UTF-8 was accepted");
    state->close();
}
}
int main(){try{run();std::cout<<"WEB_TEXT_INPUT_OK\n";}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
