#include <iiGUI/TextFieldMaterial.h>
#include <stdexcept>
struct TestPolicy {
    using Color=ii::gui::WidgetColor;enum class Variant{standard,inline_};
    struct Stop{double position;Color color;};struct Shadow{double radius,offset_x,offset_y;Color color;};
    inline static constexpr Stop fill_gradient[]={{0,{0,0,0,.2}},{1,{1,1,1,.1}}};
    inline static constexpr Stop disabled_fill_gradient[]={{0,{0,0,0,.1}},{1,{1,1,1,.05}}};
    inline static constexpr Stop inline_fill_gradient[]={{0,{0,0,0,.1}},{1,{1,1,1,.1}}};
    inline static constexpr Stop stroke_gradient[]={{0,{0,0,0,.2}},{1,{1,1,1,.2}}};
    inline static constexpr Shadow inner_shadows[]={{1,0,0,{0,0,0,.1}}};
    inline static constexpr Shadow inline_inner_shadows[]={{1,0,0,{0,0,0,.1}}};
    inline static constexpr Color reflection{1,1,1,.01},background{.2,.3,.4,.7},disabled_background{.3,.3,.3,.5},
        inline_background{.2,.3,.4,.2},inline_disabled_background{.3,.3,.3,.2};
    inline static constexpr double border_width=.5,radius=4,blur_radius=8,inline_blur_radius=6;
};
void require(bool v,const char* m){if(!v)throw std::runtime_error(m);}
int main(){
    using namespace ii::gui;BasicTextFieldMaterial<TestPolicy> material;WidgetPaintState s;s.size={30,20};
    auto first=material.paint(s,true);require(first&&(*first)->images.size()==1&&(*first)->backdrop,"enabled material");
    auto cached=material.paint(s,true);require(cached&&(*cached)->images[0].image==(*first)->images[0].image,"same material was rerasterized");
    auto invalid=s;invalid.size.width=-1;require(!material.paint(invalid,true),"invalid geometry accepted");
    auto retained=material.paint(s,true);require(retained&&(*retained)->images[0].image==(*first)->images[0].image,"invalid update displaced cache");
    auto disabled=material.paint(s,false);require(disabled&&!(*disabled)->backdrop&&(*disabled)->images[0].image!=(*first)->images[0].image,"disabled material unchanged");
    auto inlined=material.paint(s,true,0,TestPolicy::Variant::inline_);require(inlined&&(*inlined)->backdrop&&(*inlined)->images.size()==1,"inline material");
    require(!material.paint(s,true,2,TestPolicy::Variant::inline_),"inline nonzero radius accepted");
}
