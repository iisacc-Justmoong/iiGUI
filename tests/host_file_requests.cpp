#include "../src/Platform/HostFileRequests.h"
#include <stdexcept>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace {
using Requests = ii::gui::detail::HostFileRequests;
void check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
}
int main() {
    Requests requests;
    check(requests.admit({"/하나.iisc", "/same"}) && requests.admit({"/same", "/two"}), "Valid requests were rejected");
    check(!requests.admit({"/ok", ""}) && !requests.admit({std::string("/x\0y", 4)}) &&
          !requests.admit({std::string("\xff", 1)}) &&
          !requests.admit({std::string(Requests::maximum_path_bytes + 1, 'x')}),
          "An invalid path was admitted");
    check(requests.take() == std::vector<std::string>({"/하나.iisc", "/same", "/same", "/two"}),
          "A rejected batch mutated pending requests or changed their order");
    check(requests.take().empty(), "Taking native file requests did not clear the queue");
    check(requests.admit(std::vector<std::string>(Requests::maximum_paths, "x")) && !requests.admit({"overflow"}),
          "The native path count limit was not atomic");
    check(requests.take().size() == Requests::maximum_paths, "Path overflow discarded accepted requests");
    for (std::size_t i = 0; i < Requests::maximum_bytes / Requests::maximum_path_bytes; ++i)
        check(requests.admit({std::string(Requests::maximum_path_bytes, 'a')}), "A valid path was rejected before the byte limit");
    check(!requests.admit({"overflow"}), "The native path byte limit was not enforced");
    requests.clear();
    check(requests.take().empty() && requests.admit({"/fresh"}), "Clearing native paths did not reset capacity");
    int wakes = 0;
    requests.deliver([&](auto paths) {
        check(paths == std::vector<std::string>{"/fresh"}, "A native delivery changed its accepted batch");
        check(requests.admit({"/reentrant"}), "A reentrant native request was rejected");
    }, [&] { ++wakes; });
    check(wakes == 1 && requests.take() == std::vector<std::string>{"/reentrant"},
          "A reentrant native request was left without another wakeup");
    auto lifetime = std::make_shared<int>(7);
    std::weak_ptr<int> weak = lifetime;
    std::function<void(std::vector<std::string>)> handler;
    handler = [&, keep = lifetime](auto) {
        handler = {}; lifetime.reset();
        check(!weak.expired() && *keep == 7, "Closing the host destroyed its active callback");
    };
    check(requests.admit({"/close"}), "Cannot enqueue the close-during-callback case");
    requests.deliver(handler, [] {});
    check(weak.expired(), "The completed native callback retained its owner");
}
