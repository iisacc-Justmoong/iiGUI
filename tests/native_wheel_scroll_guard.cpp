#include <iiGUI/NativeWheelScrollGuard.h>
#include "../src/Widgets/WidgetState.h"
#include <memory>
#include <optional>
#include <stdexcept>

namespace {
void require(bool value,const char* why){if(!value)throw std::runtime_error(why);}
template<class T>T take(ii::core::Result<T> value){if(!value)throw std::runtime_error(value.error().message);return std::move(*value);}
void good(ii::core::Result<void> value){if(!value)throw std::runtime_error(value.error().message);}
}
int main(){
    using namespace ii::gui;
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{240,180}));std::optional<NativeWheelScrollGuard> guard;Widget target;int routed=0;
    good(host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto made=NativeWheelScrollGuard::create(builder,{},"guard",{.width=120,.height=100},[&](const WheelScrollObservation&){++routed;});
        if(!made)return std::unexpected(made.error());guard=std::move(*made);
        auto scroll=builder.scroll_view(guard->root(),"scroll",{.width=120,.height=100,.virtual_content_height=500});
        if(!scroll)return std::unexpected(scroll.error());target=*scroll;
        auto body=builder.absolute_stack(target,"body",{120,500});if(!body)return std::unexpected(body.error());
        return guard->set_target(target);
    }));
    (void)take(tree->drawing_snapshot({240,180}));auto before=take(target.snapshot());
    good(tree->input({.kind=WidgetInputKind::scroll,.x=before.bounds.x+5,.y=before.bounds.y+5,.scroll_y=30}));
    require(take(target.snapshot()).scroll_offset==30&&routed==1,"Wheel guard did not route a scroll to its target");
    good(guard->set_options({.width=120,.height=100,.enabled=false}));
    good(tree->input({.kind=WidgetInputKind::scroll,.x=before.bounds.x+5,.y=before.bounds.y+5,.scroll_y=20}));
    require(take(target.snapshot()).scroll_offset==50&&routed==1,"Disabled guard blocked normal native scrolling");
    good(host.clear_content());require(!guard->set_target(target),"Retired wheel guard accepted a target");
    Widget outer;guard.reset();
    good(host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto parent=builder.scroll_view({},"outer",{.width=120,.height=100,.virtual_content_height=600});
        if(!parent)return std::unexpected(parent.error());outer=*parent;
        auto body=builder.absolute_stack(outer,"outer-body",{120,600});if(!body)return std::unexpected(body.error());
        auto made=NativeWheelScrollGuard::create(builder,*body,"guard",{.width=120,.height=100});
        if(!made)return std::unexpected(made.error());guard=std::move(*made);
        auto child=builder.scroll_view(guard->root(),"scroll",{.width=120,.height=100,.virtual_content_height=500});
        if(!child)return std::unexpected(child.error());target=*child;
        auto content=builder.absolute_stack(target,"inner-body",{120,500});if(!content)return std::unexpected(content.error());
        return guard->set_target(target);
    }));
    (void)take(tree->drawing_snapshot({240,180}));good(target.set_scroll_offset(400));auto inner=take(target.snapshot());
    good(tree->input({.kind=WidgetInputKind::scroll,.x=inner.bounds.x+5,.y=inner.bounds.y+5,.scroll_y=30}));
    require(take(outer.snapshot()).scroll_offset==0,"Consuming guard leaked wheel input at the inner boundary");
    good(guard->set_options({.width=120,.height=100,.consume_inside=false}));
    good(tree->input({.kind=WidgetInputKind::scroll,.x=inner.bounds.x+5,.y=inner.bounds.y+5,.scroll_y=30}));
    require(take(outer.snapshot()).scroll_offset==30,"Non-consuming guard prevented scroll chaining at the inner boundary");
    good(host.clear_content());tree->close();
}
