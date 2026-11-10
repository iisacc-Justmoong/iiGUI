#include "../src/Graphics/CanvasDamage.h"
#include <stdexcept>

namespace {
using namespace ii::gui;
using namespace ii::gui::detail;
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
bool covers(Rect outer,Rect inner){return outer.x<=inner.x&&outer.y<=inner.y&&
    outer.x+outer.width>=inner.x+inner.width&&outer.y+outer.height>=inner.y+inner.height;}
}
int main(){
    CanvasFrame old{.viewport={0,0,200,100},.background={.1f,.1f,.1f,1}};
    old.panels={{{10,10,20,20},{1,0,0,1},{1,0,0,1}},{{120,20,20,20},{0,1,0,1},{0,1,0,1}}};
    check(frame_damage(nullptr,old,2).kind==DamageKind::full,"First frame did not require full composition");
    check(frame_damage(&old,old,2).kind==DamageKind::none,"Identical frame had damage");
    auto changed=old;changed.panels[1].top={0,0,1,1};
    auto plan=frame_damage(&old,changed,2);
    check(plan.kind==DamageKind::partial&&covers(plan.bounds,{120,20,20,20})&&plan.bounds.x>50,
          "One panel change damaged an unrelated panel");
    changed=old;changed.panels[0].top={0,1,1,1};changed.panels[1].top={0,0,1,1};
    plan=frame_damage(&old,changed,2);
    check(plan.kind==DamageKind::partial&&plan.regions.size()==2&&
          covers(plan.regions[0],{10,10,20,20})&&covers(plan.regions[1],{120,20,20,20}),
          "Distant changes were combined into one full-width update");
    CanvasFrame many{.viewport=old.viewport,.background=old.background};
    for(int i=0;i<17;++i)many.panels.push_back({{float(5+i*10),80,2,2},{1,0,0,1},{1,0,0,1}});
    auto many_changed=many;for(auto& panel:many_changed.panels)panel.top={0,1,0,1};
    auto bounded=frame_damage(&many,many_changed,2);
    check(bounded.kind==DamageKind::partial&&bounded.regions.size()<=16,
          "Many changes exceeded the bounded render-pass count");
    for(const auto& panel:many.panels)
        check(std::ranges::any_of(bounded.regions,[&](Rect region){return covers(region,panel.bounds);}),
              "Coalescing the damage regions dropped changed pixels");
    changed=old;changed.panels[1].bounds={160,30,20,20};plan=frame_damage(&old,changed,2);
    check(plan.kind==DamageKind::partial&&covers(plan.bounds,{120,20,20,20})&&covers(plan.bounds,{160,30,20,20}),
          "Movement did not cover both old and new pixels");
    changed=old;changed.panels.erase(changed.panels.begin()+1);plan=frame_damage(&old,changed,2);
    check(plan.kind==DamageKind::partial&&covers(plan.bounds,{120,20,20,20}),"Removed panel left stale pixels");
    changed=old;changed.background={0,0,0,1};
    check(frame_damage(&old,changed,2).kind==DamageKind::full,"Changed background was not fully cleared");
    changed=old;changed.viewport.width=201;
    check(frame_damage(&old,changed,2).kind==DamageKind::full,"Resize reused the old attachment");

    CanvasLayer layer;layer.content_transform={.origin_x=20,.origin_y=10,.scale=2,.offset_x=5,.offset_y=3};
    layer.texts.push_back({.bounds={30,20,20,10},.text="Before"});old.overlays.push_back(layer);
    changed=old;changed.overlays[0].texts[0].text="After";plan=frame_damage(&old,changed,2);
    check(plan.kind==DamageKind::partial&&covers(plan.bounds,{45,33,40,20}),
          "Transformed text damage did not cover its displayed bounds");
    changed=old;changed.overlays[0].clip=Rect{40,20,10,10};plan=frame_damage(&old,changed,2);
    check(plan.kind==DamageKind::partial&&covers(plan.bounds,{45,33,40,20}),
          "Changed clip did not invalidate the old and new layer extent");
    changed=old;changed.overlays.push_back({.panels={{{170,60,10,10},{1,1,1,1},{1,1,1,1}}}});
    plan=frame_damage(&old,changed,2);
    check(plan.kind==DamageKind::partial&&covers(plan.bounds,{170,60,10,10}),"Added layer was not damaged");
    changed=old;changed.overlays[0].backdrop=BackdropMaterial{.bounds={0,0,20,20},.blur_radius=8};
    check(frame_damage(&old,changed,2).kind==DamageKind::full,"Backdrop did not force conservative recomposition");
    changed=old;changed.scenes.push_back({.viewport=old.viewport});
    check(frame_damage(&old,changed,2).kind==DamageKind::full,"Scene graph did not force conservative recomposition");
}
