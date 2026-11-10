#include "../src/Widgets/WidgetState.h"
#include <cmath>
#include <limits>
#include <stdexcept>
using namespace ii;
using namespace ii::gui;
namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(core::Result<T> value){if(!value)throw std::runtime_error(value.error().message);return std::move(*value);}
void good(core::Result<void> value){if(!value)throw std::runtime_error(value.error().message);}
void nested(){
    auto tree=std::make_shared<detail::WidgetState>([](runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{200,100}));Widget outer,inner,editor;
    good(host.set_content([&](WidgetBuilder& builder)->core::Result<void>{
        outer=take(builder.scroll_view({},"outer",{.width=200,.height=100}));
        auto body=take(builder.absolute_stack(outer,"body",{200,400}));
        inner=take(builder.scroll_view(body,"inner",{.width=180,.height=80}));good(inner.set_frame({0,200,180,80}));
        auto content=take(builder.absolute_stack(inner,"content",{180,300}));
        editor=take(builder.absolute_stack(content,"editor",{180,240}));return editor.set_frame({0,0,180,240});
    }));
    (void)take(tree->drawing_snapshot({200,100}));good(editor.reveal_local_rect({2,150,1,16}));
    check(take(inner.snapshot()).scroll_offset>0&&take(outer.snapshot()).scroll_offset>0,
          "Nested scroll ancestors did not reveal the requested rectangle");
    good(host.clear_content());tree->close();
}
}
int main(){
    auto tree=std::make_shared<detail::WidgetState>([](runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{200,100}));Widget scroll,editor;
    good(host.set_content([&](WidgetBuilder& builder)->core::Result<void>{
        scroll=take(builder.scroll_view({},"scroll",{.width=200,.height=100}));
        auto body=take(builder.absolute_stack(scroll,"body",{200,320}));
        editor=take(builder.absolute_stack(body,"editor",{180,240}));
        return editor.set_frame({10,10,180,240});
    }));
    (void)take(tree->drawing_snapshot({200,100}));
    good(editor.reveal_local_rect({4,150,1,16}));
    const auto first=take(scroll.snapshot()).scroll_offset;
    check(first>0&&first<=220,"The caret was not revealed within the scroll bounds");
    good(editor.reveal_local_rect({4,150,1,16}));
    check(take(scroll.snapshot()).scroll_offset==first,"A visible caret caused repeated movement");
    good(editor.reveal_local_rect({4,0,1,16}));
    check(take(scroll.snapshot()).scroll_offset<first,"Moving the caret up did not reveal it");
    check(!editor.reveal_local_rect({0,0,std::numeric_limits<float>::quiet_NaN(),1}),"Invalid rectangle was accepted");
    good(host.clear_content());check(!editor.reveal_local_rect({0,0,1,1}),"Retired widget accepted reveal");tree->close();
    nested();
}
