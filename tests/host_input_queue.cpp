#include "iiGUI/HostInputQueue.h"
#include <iostream>
int main(){using namespace ii::gui;detail::HostInputQueue queue(2);
    if(!queue.push({.kind=WidgetInputKind::pointer_move,.x=1})||!queue.push({.kind=WidgetInputKind::pointer_move,.x=9}))return 1;
    if(!queue.push({.kind=WidgetInputKind::pointer_down,.x=10}))return 2;
    if(queue.push({.kind=WidgetInputKind::pointer_up,.x=11}))return 3;
    const auto first=queue.pop(),second=queue.pop();if(!first||first->x!=9||!second||second->kind!=WidgetInputKind::pointer_down||!queue.empty())return 4;
    if(!queue.push({.kind=WidgetInputKind::pointer_move,.device_id=1})||!queue.push({.kind=WidgetInputKind::pointer_move,.device_id=2}))return 5;
    if(queue.push({.kind=WidgetInputKind::key_down,.text="preserve"}))return 6;
    (void)queue.pop();(void)queue.pop();if(queue.pop())return 7;
    if(!queue.push({.kind=WidgetInputKind::key_down,.text="한글"})||queue.pop()->text!="한글")return 8;
    std::cout<<"HOST_INPUT_QUEUE_OK\n";
}
