#include "../src/Widgets/WidgetState.h"

#include <cstdlib>
#include <iostream>
#include <memory>
#include <new>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
thread_local bool count_allocations = false;
thread_local std::size_t allocated = 0;
}
void* operator new(std::size_t size) {
    if (void* value = std::malloc(size ? size : 1)) {
        if (count_allocations) allocated += size;
        return value;
    }
    throw std::bad_alloc();
}
void operator delete(void* value) noexcept { std::free(value); }
void operator delete(void* value, std::size_t) noexcept { std::free(value); }

using namespace ii;
namespace {
void check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
void good(core::Result<void> result) {
    if (!result) throw std::runtime_error(result.error().message);
}
template<class T> T take(core::Result<T> result) {
    if (!result) throw std::runtime_error(result.error().message);
    return std::move(*result);
}
gui::WidgetDrawing drawing(float red) {
    gui::WidgetDrawing value;
    value.panels.push_back({.bounds={0,0,20,20},.top={red,0,0,1},.bottom={red,0,0,1}});
    return value;
}
}
int main() try {
    std::size_t redraws=0;
    gui::detail::WidgetFrameHooks hooks;
    hooks.invalidated=[&]{++redraws;};
    auto state=std::make_shared<gui::detail::WidgetState>([](runtime::Task task){task();},gui::WidgetAppearance{},std::move(hooks));
    gui::Widget canvas;
    std::size_t deliveries=0;
    std::vector<gui::WidgetInteraction> canvas_changes;
    good(state->construct([&](gui::WidgetBuilder& builder)->core::Result<void> {
        auto root=take(builder.absolute_stack({},"root",{300,300}));
        canvas=take(builder.absolute_stack(root,"canvas",{100,100}));
        good(canvas.set_frame({20,20,100,100}));
        good(canvas.set_interaction_handler([&](const auto& value){canvas_changes.push_back(value);}));
        for (int i=0;i<128;++i) {
            auto node=take(builder.button(root,"watch"+std::to_string(i),{.height=1,.width=1},{}));
            good(node.set_interaction_handler([&](const auto& value){if(value.alive)++deliveries;}));
        }
        return {};
    }));
    good(state->activate_paint_view());
    (void)take(state->drawing_snapshot({300,300}));
    (void)take(canvas.snapshot());
    check(deliveries==128,"Initial interaction state was not delivered");
    const auto initial_changes=canvas_changes.size();
    good(canvas.set_drawing(drawing(.25f)));
    (void)take(canvas.snapshot());
    const auto current=take(canvas.interaction());
    check(current.clip.width==100 && current.clip.height==100 &&
          canvas_changes.size()==initial_changes+1 && canvas_changes.back().clip==current.clip,
          "Initial retained drawing did not notify its interaction clip");
    const auto measure_snapshot=[&] {
        allocated=0;count_allocations=true;
        const auto result=canvas.snapshot();
        count_allocations=false;
        check(bool(result),"Could not synchronize interaction state");
        return allocated;
    };
    const auto baseline=measure_snapshot();
    const auto before=deliveries;
    const auto redraws_before=redraws;
    good(canvas.set_drawing(drawing(.75f)));
    const auto actual=measure_snapshot();
    check(redraws>redraws_before,"Paint-only update failed to request a redraw");
    check(deliveries==before,"Paint-only update changed an interaction notification");
    if (actual>baseline) {
        std::cerr<<"Paint-only synchronization allocated "<<actual
                 <<" bytes versus "<<baseline<<" bytes at rest\n";
        throw std::runtime_error("Paint-only update unnecessarily traversed interaction observers");
    }
    good(canvas.set_frame({30,30,100,100}));
    (void)take(canvas.snapshot());
    check(take(canvas.interaction()).bounds.x==30,"Layout update was not observed");
    state->close();

    std::size_t parent_redraws=0;
    gui::detail::WidgetFrameHooks parent_hooks;
    parent_hooks.invalidated=[&]{++parent_redraws;};
    auto parent=std::make_shared<gui::detail::WidgetState>([](runtime::Task task){task();},gui::WidgetAppearance{},std::move(parent_hooks));
    gui::Widget host,embedded;
    std::size_t parent_deliveries=0;
    good(parent->construct([&](gui::WidgetBuilder& builder)->core::Result<void> {
        auto root=take(builder.absolute_stack({},"parent",{300,300}));
        host=take(builder.content_host(root,"host",{100,100}));
        for(int i=0;i<128;++i) {
            auto node=take(builder.button(root,"parent-watch"+std::to_string(i),{.height=1,.width=1},{}));
            good(node.set_interaction_handler([&](const auto& value){if(value.alive)++parent_deliveries;}));
        }
        return {};
    }));
    good(parent->activate_paint_view());
    good(host.set_content([&](gui::WidgetBuilder& builder)->core::Result<void> {
        embedded=take(builder.absolute_stack({},"embedded",{100,100}));
        return embedded.set_drawing(drawing(.25f));
    }));
    (void)take(parent->drawing_snapshot({300,300}));
    (void)take(host.snapshot());
    check(parent_deliveries==128,"Embedded fixture did not initialize parent observers");
    const auto measure_parent=[&] {
        allocated=0;count_allocations=true;
        const auto result=host.snapshot();
        count_allocations=false;
        check(bool(result),"Could not synchronize embedded state");
        return allocated;
    };
    const auto parent_baseline=measure_parent();
    const auto parent_redraws_before=parent_redraws;
    good(embedded.set_drawing(drawing(.75f)));
    const auto parent_actual=measure_parent();
    check(parent_redraws>parent_redraws_before,"Embedded paint failed to request a parent redraw");
    if(parent_actual>parent_baseline) {
        std::cerr<<"Embedded paint-only synchronization allocated "<<parent_actual
                 <<" bytes versus "<<parent_baseline<<" bytes at rest\n";
        throw std::runtime_error("Embedded paint needlessly traversed parent observers");
    }
    check(parent_deliveries==128,"Embedded paint altered parent interaction state");
    parent->close();
    std::cout<<"PAINT_ONLY_INVALIDATION_OK\n";
    return 0;
} catch (const std::exception& error) {
    count_allocations=false;
    std::cerr<<error.what()<<'\n';
    return 1;
}
