#include "../src/Widgets/WidgetState.h"
#include <iostream>
#include <stdexcept>
#include <thread>
using namespace ii;
namespace {
void check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
void good(core::Result<void> result) { if (!result) throw std::runtime_error(result.error().message); }
}
int main() {
    try {
        auto state = std::make_shared<gui::detail::WidgetState>([](runtime::Task task) { task(); });
        int calls = 0, destroyed = 0;
        bool admission_closed = false, ordered = false, closed_before_notification = false;
        struct Owner final {
            int& destroyed; bool& closed; bool& ordered;
            Owner(int& d, bool& c, bool& o) : destroyed(d), closed(c), ordered(o) {}
            ~Owner() { ordered = closed; ++destroyed; }
        };
        good(state->retain(std::make_shared<Owner>(destroyed, admission_closed, ordered)));
        good(state->on_retire([&] {
            ++calls; admission_closed = true;
            closed_before_notification = !state->ready() && !state->on_retire([] {});
            state->close();
        }));
        good(state->on_retire([] { throw std::runtime_error("isolated retirement failure"); }));
        good(state->on_retire([&] { ++calls; }));
        check(!state->on_retire({}), "Empty hook accepted");
        bool foreign_rejected = false;
        std::thread worker([&] { foreign_rejected = !state->on_retire([] {}); });
        worker.join();
        check(foreign_rejected && calls == 0, "Foreign thread changed the retirement list");
        state->close(); state->close();
        check(calls == 2 && destroyed == 1 && ordered && closed_before_notification,
              "Retirement was not ordered, isolated and exactly once");
        auto bounded = std::make_shared<gui::detail::WidgetState>([](runtime::Task task) { task(); });
        int bounded_calls = 0;
        for (int n = 0; n < 4096; ++n) good(bounded->on_retire([&] { ++bounded_calls; }));
        auto overflow = bounded->on_retire([] {});
        check(!overflow && overflow.error().code == core::ErrorCode::full, "Unbounded retirement admission");
        bounded.reset();
        check(bounded_calls == 4096, "Destructor skipped admitted retirement hooks");
        auto self_releasing = std::make_shared<gui::detail::WidgetState>([](runtime::Task task) { task(); });
        std::weak_ptr<gui::detail::WidgetState> weak = self_releasing;
        bool kept_during_callback = false;
        good(self_releasing->on_retire([&] {
            self_releasing.reset();
            kept_during_callback = !weak.expired();
        }));
        auto* current = self_releasing.get();
        current->close();
        check(kept_during_callback && weak.expired(), "Closing did not protect its active generation stack");
        auto layout = std::make_shared<gui::detail::WidgetState>([](runtime::Task task) { task(); });
        auto slot = layout->content_host({}, "conditional", {});
        check(bool(slot), "Natural content host was not constructed");
        good(slot->set_content([](gui::WidgetBuilder& builder) -> core::Result<void> {
            auto root = builder.vstack({}, "branch", {.minimum_width=120,.minimum_height=80});
            if (!root) return std::unexpected(root.error());
            return {};
        }));
        auto measured = slot->measure(400);
        check(measured && measured->width == 400 && measured->height == 80, "Natural branch height was clipped to zero");
        auto root = slot->content_item();
        check(bool(root), "Content root was not inspectable");
        good(root->set_extent({0,140}));
        measured = slot->measure(400);
        check(measured && measured->height == 140, "Changed branch height did not invalidate its parent");
        good(slot->clear_content());
        measured = slot->measure(400);
        check(measured && measured->height == 0, "Empty conditional content retained its previous height");
        layout->close();
        std::cout << "Widget generation retirement passed without a window or renderer\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
