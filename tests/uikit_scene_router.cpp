#include "../src/Platform/UIKitSceneRouter.h"
#include <stdexcept>
#include <string>

namespace {
using Router = ii::gui::detail::UIKitSceneRouter;
using Status = Router::Status;
void check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }

void routes_out_of_order_and_reconnects() {
    Router router;
    int first = 0, second = 0, replacement = 0, foreign = 0;
    auto first_request = router.reserve();
    auto second_request = router.reserve();
    check(first_request && second_request && *first_request != *second_request, "Requests were not reserved");
    auto two = router.connect("session-two", &second, *second_request);
    auto one = router.connect("session-one", &first, *first_request);
    check(two.status == Status::connected && two.token == *second_request &&
          one.status == Status::connected && one.token == *first_request, "Out-of-order sessions reached the wrong request");
    check(router.find(&first).token == *first_request && router.find(&second).token == *second_request,
          "A scene event did not resolve to its own request");
    check(router.connect("session-one", &first, *first_request).status == Status::duplicate,
          "Duplicate scene connection was republished");
    check(router.connect("session-one", &foreign, *second_request).status == Status::conflict,
          "Another request stole a bound session");
    check(router.connect("session-three", &first, *second_request).status == Status::conflict,
          "A scene instance was bound to two sessions");
    check(router.connect("session-one", &replacement, {}).status == Status::conflict,
          "An overlapping scene displaced the live scene");
    auto detached = router.disconnect(&first);
    check(detached.status == Status::disconnected && detached.token == *first_request, "Disconnect lost its owner");
    check(router.find(&first).status == Status::unknown, "A disconnected scene still routed events");
    check(router.disconnect(&first).status == Status::unknown, "A stale scene disconnected twice");
    check(router.connect("session-one", &replacement, {}).status == Status::reconnected,
          "The existing session did not reconnect without a new token");
    check(router.disconnect(&first).status == Status::unknown, "A stale scene retired the replacement");
    auto discarded = router.discard("session-one");
    check(discarded.status == Status::discarded && discarded.token == *first_request && router.size() == 1,
          "Discard did not retire exactly one session");
    check(router.connect("session-one", &replacement, {}).status == Status::unknown,
          "Discarded session was reattached without a request");
    check(router.discard("session-two").token == *second_request && router.size() == 0,
          "Unrelated session did not remain independently owned");
}

void cancelled_and_failed_requests_do_not_reach_new_windows() {
    Router router;
    int late = 0, replacement = 0;
    auto cancelled = router.reserve();
    check(cancelled && router.cancel(*cancelled), "Pending request could not be cancelled");
    auto fresh = router.reserve();
    check(fresh && *fresh != *cancelled && router.fail(*fresh).status == Status::failed,
          "A cancelled token was reused before its result arrived");
    auto arrival = router.connect("late-session", &late, *cancelled);
    check(arrival.status == Status::cancelled && arrival.token == *cancelled,
          "A late cancelled request was delivered as a live window");
    check(router.connect("late-session", &late, *cancelled).status == Status::duplicate,
          "A cancelled session requested destruction twice on duplicate connect");
    check(router.find(&late).status == Status::cancelled, "A cancelled scene routed an active event");
    check(router.disconnect(&late).status == Status::disconnected, "Cancelled scene disconnect was lost");
    check(router.connect("late-session", &replacement, {}).status == Status::cancelled,
          "A cancelled session reconnected as a live window");
    check(router.discard("late-session").token == *cancelled, "Cancelled session was not released");
    auto failed = router.reserve();
    check(failed && router.fail(*failed).status == Status::failed && router.size() == 0,
          "Failed activation did not release its reservation");
    auto live = router.reserve();
    check(live && router.connect("live-session", &replacement, *live).status == Status::connected,
          "The live request did not connect");
    check(router.fail(*live).status == Status::ignored && router.size() == 1,
          "A late activation error removed a connected scene");
}

void invalid_and_foreign_inputs_are_bounded() {
    Router router;
    int scene = 0;
    auto known = router.reserve();
    check(bool(known), "A request was not reserved");
    check(router.connect("", &scene, *known).status == Status::invalid &&
          router.connect("session", nullptr, *known).status == Status::invalid &&
          router.connect(std::string(4097, 'x'), &scene, *known).status == Status::invalid &&
          router.connect(std::string("x\0y", 3), &scene, *known).status == Status::invalid,
          "Invalid scene identity was accepted");
    check(router.connect("foreign", &scene, *known + 1).status == Status::unknown && router.size() == 1,
          "An unknown request changed a reserved slot");
    check(router.fail(*known + 1).status == Status::unknown && !router.cancel(*known + 1),
          "Unknown requests affected the router");
    router.clear();
    check(router.size() == 0, "Router shutdown retained pending requests");
    auto after_clear = router.reserve();
    check(after_clear && *after_clear > *known, "Router shutdown reused a previous token");
}

void request_capacity_is_bounded_without_token_reuse() {
    Router router;
    Router::Token first = 0, last = 0;
    for (std::size_t index = 0; index < Router::maximum_requests; ++index) {
        auto token = router.reserve();
        check(bool(token), "The router reached capacity early");
        if (!index) first = *token;
        last = *token;
    }
    check(!router.reserve(), "The router accepted more than its request bound");
    check(router.fail(first).status == Status::failed, "A completed request did not release capacity");
    auto next = router.reserve();
    check(next && *next > last, "Releasing capacity reused a retired token");
}
}

int main() {
    routes_out_of_order_and_reconnects();
    cancelled_and_failed_requests_do_not_reach_new_windows();
    invalid_and_foreign_inputs_are_bounded();
    request_capacity_is_bounded_without_token_reuse();
}
