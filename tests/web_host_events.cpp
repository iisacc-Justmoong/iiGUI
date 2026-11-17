#include "../src/Platform/WebHostEvents.h"
#include <atomic>
#include <thread>

int main() {
    using namespace ii::gui;
    using namespace ii::gui::detail;
    auto focus=web_accessibility_input("scope/button",0),blur=web_accessibility_input("scope/button",8);
    if(!focus||focus->kind!=WidgetInputKind::focus||focus->target!="scope/button")return 10;
    if(!blur||blur->kind!=WidgetInputKind::focus_lost||!blur->target.empty())return 11;
    if(web_accessibility_input("scope/button",9))return 12;
    WebHostEvents queue(3);
    queue.push({.kind=WebHostEventKind::resize});
    queue.push({.kind=WebHostEventKind::resize});
    queue.push({.kind=WebHostEventKind::input, .input={.kind=WidgetInputKind::pointer_down, .x=4}});
    queue.push({.kind=WebHostEventKind::visibility, .hidden=true});
    queue.push({.kind=WebHostEventKind::input, .input={.kind=WidgetInputKind::pointer_up}});
    if (!queue.take_overflow()) return 1;
    if (queue.take_overflow()) return 2;
    auto resize=queue.pop(), down=queue.pop(), hidden=queue.pop();
    if (!resize || resize->kind!=WebHostEventKind::resize) return 3;
    if (!down || down->input.kind!=WidgetInputKind::pointer_down || down->input.x!=4) return 4;
    if (!hidden || hidden->kind!=WebHostEventKind::visibility || !hidden->hidden || queue.pop()) return 5;
    queue.push({.kind=WebHostEventKind::input, .input={.kind=WidgetInputKind::pointer_move,.x=1}});
    queue.push({.kind=WebHostEventKind::input, .input={.kind=WidgetInputKind::pointer_move,.x=8}});
    auto motion=queue.pop();
    if (!motion || motion->input.x!=8 || queue.pop()) return 6;

    // Producer and consumer use separate threads, as in the browser host.
    WebHostEvents concurrent(4096);
    std::atomic<bool> done=false;
    std::thread producer([&] {
        for (int i=0; i<2000; ++i)
            concurrent.push({.kind=WebHostEventKind::input, .input={.kind=WidgetInputKind::pointer_down,.x=double(i)}});
        done.store(true, std::memory_order_release);
    });
    int next=0;
    while (next<2000) {
        auto event=concurrent.pop();
        if (!event && done.load(std::memory_order_acquire)) {
            // The final push may have occurred after the first empty read.
            event=concurrent.pop();
            if (!event) { producer.join(); return 8; }
        }
        if (event && event->input.x!=next++) { producer.join(); return 7; }
    }
    producer.join();
    return concurrent.take_overflow() ? 9 : 0;
}
