#include <iiGUI/InputClearDrawing.h>
#include <cmath>
#include <stdexcept>
int main(){
    using namespace ii::gui;
    WidgetPaintState state;state.size={16,16};WidgetColor ink{.5,.5,.5,.8},background{.1,.2,.3,.5};
    auto fallback=input_field_clear_drawing(state,ink,background,13,13,"Example",false);
    if(!fallback||(*fallback)->panels.size()!=1||(*fallback)->texts.size()!=1||
       (*fallback)->texts[0].text!="×"||(*fallback)->texts[0].color.alpha!=1||
       std::abs((*fallback)->panels[0].top.red-.21404114)>1e-5||(*fallback)->texts[0].font_family!="Example")
        throw std::runtime_error("fallback clear drawing is incorrect");
    auto symbol=input_field_clear_drawing(state,ink,background,13,13,"Example",true);
    if(!symbol||!(*symbol)->panels.empty()||(*symbol)->texts.size()!=1||(*symbol)->texts[0].text=="×")
        throw std::runtime_error("system symbol clear drawing is incorrect");
    state.size.width=-1;
    if(input_field_clear_drawing(state,ink,background,13,13,"Example",false))throw std::runtime_error("invalid size accepted");
}
