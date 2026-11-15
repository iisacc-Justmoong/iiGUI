#include "../src/Graphics/CompositionResize.h"
#include <stdexcept>

namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void unchanged_frames_do_not_resize(){
    ii::gui::detail::CompositionResize sizes{1,1};int calls=0;
    auto resize=[&](unsigned,unsigned){++calls;};
    check(!sizes.update(1,1,resize) && calls==0,"An unchanged surface was resized");
    check(sizes.update(320,240,resize) && calls==1,"Changed surface did not resize");
    check(!sizes.update(320,240,resize) && calls==1,"Repeated frame resized the surface");
}
void failed_resize_keeps_committed_dimensions(){
    ii::gui::detail::CompositionResize sizes{8,8};int calls=0;
    try{(void)sizes.update(16,8,[&](unsigned,unsigned){++calls;throw std::runtime_error("device failure");});}
    catch(const std::runtime_error&){}
    check(!sizes.update(8,8,[&](unsigned,unsigned){++calls;}),"Failed resize changed committed dimensions");
    check(sizes.update(16,8,[&](unsigned,unsigned){++calls;}) && calls==2,"Failed resize was not retried");
    bool rejected=false;try{(void)sizes.update(0,8,[&](unsigned,unsigned){++calls;});}catch(const std::invalid_argument&){rejected=true;}
    check(rejected && calls==2,"An empty GPU buffer was accepted");
}
}
int main(){unchanged_frames_do_not_resize();failed_resize_keeps_committed_dimensions();}
