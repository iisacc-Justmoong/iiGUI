#include <iiGUI/WidgetState.h>
#include <memory>
#include <stdexcept>

namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(ii::core::Result<T> result){if(!result)throw std::runtime_error(result.error().message);return std::move(*result);}
void good(ii::core::Result<void> result){if(!result)throw std::runtime_error(result.error().message);}
}
int main(){
    namespace gui=ii::gui;
    auto tree=std::make_shared<gui::detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{400,300}));gui::Widget popup,nested;
    gui::PopoverOptions options;options.width=100;options.height=80;options.padding=0;
    options.dismiss_on_outside_press=false;options.dismiss_on_context_request=true;
    good(host.set_content([&](gui::WidgetBuilder& builder)->ii::core::Result<void>{
        auto root=builder.vstack({},"root");if(!root)return std::unexpected(root.error());
        auto result=builder.popover(*root,"popup",options);if(!result)return std::unexpected(result.error());popup=*result;
        auto content=builder.absolute_stack(popup,"content",{100,80});if(!content)return std::unexpected(content.error());
        auto inner=builder.popover(*content,"nested",{.width=80,.height=60,.padding=0});if(!inner)return std::unexpected(inner.error());nested=*inner;
        auto body=builder.absolute_stack(nested,"nested-content",{80,60});if(!body)return std::unexpected(body.error());return {};
    }));
    const auto context=[&]{good(tree->input({.kind=gui::WidgetInputKind::context_request,.x=350,.y=250}));};
    const auto press=[&]{good(tree->input({.kind=gui::WidgetInputKind::pointer_down,.x=350,.y=250}));
        good(tree->input({.kind=gui::WidgetInputKind::pointer_up,.x=350,.y=250}));};
    good(popup.open_at(20,20));press();require(take(popup.snapshot()).opened,"Outside press ignored its disabled policy");
    context();require(!take(popup.snapshot()).opened,"Outside context request did not dismiss the popup");
    options.dismiss_on_outside_press=true;options.dismiss_on_context_request=false;good(popup.set_popover_options(options));
    good(popup.open_at(20,20));context();require(take(popup.snapshot()).opened,"Context request ignored its disabled policy");
    press();require(!take(popup.snapshot()).opened,"Outside press did not dismiss the popup");
    options.dismiss_on_outside_press=false;options.dismiss_on_context_request.reset();good(popup.set_popover_options(options));
    good(popup.open_at(20,20));context();require(take(popup.snapshot()).opened,"Default context policy did not follow the outside-press setting");
    good(popup.close_popup());options.dismiss_on_outside_press=true;options.dismiss_on_context_request=true;good(popup.set_popover_options(options));
    good(popup.open_at(20,20));good(nested.open_at(160,20));
    auto bounds=take(nested.snapshot()).bounds;
    good(tree->input({.kind=gui::WidgetInputKind::context_request,.x=bounds.x+bounds.width/2,.y=bounds.y+bounds.height/2}));
    require(take(popup.snapshot()).opened&&take(nested.snapshot()).opened,"Context request inside a descendant dismissed its parent popup");
    context();require(!take(popup.snapshot()).opened&&!take(nested.snapshot()).opened,"Outside context request did not close nested popups");
    int closed=0,canceled=0;bool reopen_then_close=false;
    good(host.set_content([&](gui::WidgetBuilder& builder)->ii::core::Result<void>{
        auto root=builder.vstack({},"root");if(!root)return std::unexpected(root.error());
        gui::PopoverEvents events;
        events.closed=[&]{++closed;if(reopen_then_close){reopen_then_close=false;good(popup.open_at(20,20));good(popup.close_popup());}};
        events.canceled=[&]{++canceled;};
        auto value=builder.popover(*root,"popup",options,std::move(events));if(!value)return std::unexpected(value.error());popup=*value;
        auto body=builder.absolute_stack(popup,"content",{100,80});if(!body)return std::unexpected(body.error());return {};
    }));
    good(popup.open_at(20,20));good(popup.cancel_popup());
    require(closed==1&&canceled==1,"A current popup cancellation was lost");
    good(popup.open_at(20,20));reopen_then_close=true;good(popup.cancel_popup());
    require(closed==3&&canceled==1&&!take(popup.snapshot()).opened,
            "A stale cancellation reached a popup that reopened and closed again");
    tree->close();
}
