#include <iiGUI/EventListenerModel.h>
#include <limits>
#include <stdexcept>

namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
}
int main(){
    ii::gui::EventListenerModel listener;auto initial=listener.configuration();
    require(initial.width==640&&initial.height==480&&!initial.size_to_content,"Event listener defaults changed");
    auto hug=initial;hug.width=0;hug.height=0;hug.size_to_content=true;
    require(listener.set_configuration(hug)&&listener.configuration().size_to_content,
            "Content-sized listener configuration was rejected");
    auto next=initial;next.width=320;next.height=180;require(listener.set_configuration(next),"Valid extent was rejected");
    auto automatic=next;automatic.width=0;require(listener.set_configuration(automatic),"Automatic width was rejected");
    auto bad=next;bad.width=-1;require(!listener.set_configuration(bad)&&listener.configuration()==automatic,"Negative extent changed the listener");
    bad=next;bad.height=8193;require(!listener.set_configuration(bad),"Oversized listener was accepted");
    bad=next;bad.width=std::numeric_limits<double>::quiet_NaN();require(!listener.set_configuration(bad),"NaN listener extent was accepted");
}
