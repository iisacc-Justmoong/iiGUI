#include <iiGUI/HierarchyToolbarLayout.h>
#include <limits>
#include <stdexcept>
namespace { void require(bool value,const char* why){if(!value)throw std::runtime_error(why);} }
int main(){
    using namespace ii::gui;
    HierarchyToolbarLayoutSnapshot snapshot;
    snapshot.items={{.icon_name="home"},{.visible=false},{.icon_name="add"}};
    HierarchyToolbarLayoutAppearance input;
    auto result=HierarchyToolbarLayout::geometry(snapshot,input,{0,0,0});
    require(result&&result->width==200&&result->height==26&&result->slots[0].x==8&&result->slots[1].width==0,
            "Hierarchy toolbar default geometry changed");
    input.distribute_spacing=true;result=HierarchyToolbarLayout::geometry(snapshot,input,{0,0,0});
    require(result&&result->slots[2].x+result->slots[2].width==192,"Hierarchy toolbar did not distribute spare space");
    input.width=30;result=HierarchyToolbarLayout::geometry(snapshot,input,{0,0,0});
    require(result&&result->slots[2].x+result->slots[2].width<=22.0001,"Narrow hierarchy toolbar overflowed");
    input={};input.slot_padding=10;input.icon_text_spacing=5;snapshot.items[0].text="Open";
    result=HierarchyToolbarLayout::geometry(snapshot,input,{30,0,0});
    require(result&&result->slots[0].width==63,"Hierarchy toolbar ignored caller-supplied spacing");
    require(!HierarchyToolbarLayout::geometry(snapshot,input,{0,0}),"Mismatched text measurements were accepted");
    require(!HierarchyToolbarLayout::geometry(snapshot,input,{std::numeric_limits<double>::infinity(),0,0}),
            "Invalid text measurement was accepted");
    input.slot_padding=-1;require(!HierarchyToolbarLayout::validate(input),"Invalid padding was accepted");
    input={};input.background.alpha=2;require(!HierarchyToolbarLayout::validate(input),"Invalid color was accepted");
    snapshot.items.resize(257);require(!HierarchyToolbarLayout::geometry(snapshot,{},std::vector<double>(257)),
                                      "More than 256 toolbar slots were accepted");
}
