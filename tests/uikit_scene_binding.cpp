#include "../src/Platform/UIKitSceneBinding.h"
#include <stdexcept>

namespace {
using Binding = ii::gui::detail::UIKitSceneBinding;
using Connection = Binding::Connection;
void check(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
}

int main() {
    Binding binding;
    int original = 0, replacement = 0, foreign = 0;
    check(binding.connect("", &original) == Connection::invalid, "An empty session was accepted");
    check(binding.connect("one", nullptr) == Connection::invalid, "A null scene was accepted");
    check(binding.connect("one", &original) == Connection::first, "The first scene did not bind");
    const auto first_revision = binding.revision();
    check(binding.accepts(&original) && binding.session_is("one"), "The current scene was not retained");
    check(binding.connect("one", &original) == Connection::duplicate, "A duplicate connection was reopened");
    check(binding.connect("one", &replacement) == Connection::overlap, "An overlapping scene displaced the live one");
    check(binding.connect("two", &foreign) == Connection::foreign, "Another session displaced the live one");
    check(binding.revision() == first_revision, "A rejected connection changed the binding revision");
    check(!binding.disconnect(&foreign) && binding.accepts(&original), "A foreign disconnect changed the binding");
    check(binding.disconnect(&original) && !binding.accepts(&original), "The current scene did not disconnect");
    const auto disconnected_revision = binding.revision();
    check(disconnected_revision != first_revision, "Disconnect did not retire the prior revision");
    check(!binding.disconnect(&original), "The same scene disconnected twice");
    check(binding.connect("two", &foreign) == Connection::foreign, "A disconnected session was stolen");
    check(binding.connect("one", &replacement) == Connection::reconnected, "The retained session did not reconnect");
    check(binding.revision() != disconnected_revision, "Reconnect did not retire pending teardown work");
    check(binding.accepts(&replacement) && !binding.accepts(&original), "A retired scene still received events");
    check(!binding.discard("two") && binding.accepts(&replacement), "A foreign discard changed the binding");
    check(binding.discard("one") && !binding.accepts(&replacement), "Discard did not retire the session");
    check(binding.connect("two", &foreign) == Connection::first, "A discarded session blocked a new one");
    check(!binding.disconnect(&replacement) && binding.accepts(&foreign), "A stale disconnect retired a new session");
    binding.reset();
    check(!binding.accepts(&foreign) && !binding.session_is("two"), "Reset retained a scene");

    Binding discarded_while_disconnected;
    check(discarded_while_disconnected.connect("old", &original) == Connection::first,
          "The initial session was not bound");
    check(discarded_while_disconnected.disconnect(&original), "The old scene did not disconnect");
    check(discarded_while_disconnected.discard("old"), "A disconnected session was not discarded");
    check(discarded_while_disconnected.connect("new", &foreign) == Connection::first,
          "A discarded disconnected session blocked a new session");
    check(!discarded_while_disconnected.discard("old") && discarded_while_disconnected.accepts(&foreign),
          "A delayed old-session discard retired the new session");
}
