#include "../src/Widgets/SurfaceInvalidation.h"
#include <stdexcept>

namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void reentrant_invalidation_is_not_lost(){
    ii::gui::detail::SurfaceInvalidation state;int draws=0;
    check(state.flush([&]{
        ++draws;state.invalidate();
        check(!state.flush([&]{++draws;}),"A nested presentation was admitted");
    }),"Initial presentation was lost");
    check(state.pending() && draws==1,"A change during presentation was lost");
    check(state.flush([&]{++draws;}) && !state.pending() && draws==2,"The changed frame was not presented once");
    check(!state.flush([&]{++draws;}),"An unchanged frame was presented");
}
void failed_presentation_can_retry(){
    ii::gui::detail::SurfaceInvalidation state;
    try{(void)state.flush([]{throw std::runtime_error("device failed");});}catch(const std::runtime_error&){}
    check(state.pending(),"A failed presentation discarded the invalidation");
    check(state.flush([]{}) && !state.pending(),"A failed presentation could not be retried");
}
}
int main(){reentrant_invalidation_is_not_lost();failed_presentation_can_retry();}
