#pragma once
#include "../Text/NativeText.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <optional>
#include <vector>

namespace ii::gui::detail {
enum class DamageKind { none, partial, full };
struct FrameDamage final {DamageKind kind=DamageKind::none;Rect bounds{};std::vector<Rect> regions;};

// Coordinates are logical canvas coordinates. A retained attachment may be
// patched only when all pixels outside these bounds are provably unchanged.
inline FrameDamage frame_damage(const CanvasFrame* previous,const CanvasFrame& next,double scale){
    if(!previous||!std::isfinite(scale)||scale<=0)return {DamageKind::full,{}, {}};
    const auto& old=*previous;
    if(old==next)return {};
    if(old.viewport!=next.viewport||old.background!=next.background||next.background.alpha!=1||
       old.renderer!=next.renderer||old.acceleration!=next.acceleration||
       !old.scenes.empty()||!next.scenes.empty())return {DamageKind::full,{}, {}};
    const auto effect=[](const CanvasFrame& frame){
        return std::ranges::any_of(frame.overlays,[](const auto& layer){return layer.backdrop||layer.shadow;});
    };
    if(effect(old)||effect(next))return {DamageKind::full,{}, {}};
    std::vector<Rect> regions;
    const auto combine=[](Rect a,Rect b){
        const auto left=std::min(a.x,b.x),top=std::min(a.y,b.y);
        const auto right=std::max(a.x+a.width,b.x+b.width),bottom=std::max(a.y+a.height,b.y+b.height);
        return Rect{left,top,right-left,bottom-top};
    };
    const auto overlaps=[](Rect a,Rect b){return a.x<=b.x+b.width&&b.x<=a.x+a.width&&
        a.y<=b.y+b.height&&b.y<=a.y+a.height;};
    const auto append=[&](Rect value){
        for(std::size_t i=0;i<regions.size();){
            if(overlaps(value,regions[i])){value=combine(value,regions[i]);regions.erase(regions.begin()+std::ptrdiff_t(i));i=0;}
            else ++i;
        }
        regions.push_back(value);
        // Bound render passes while preserving independent areas where possible.
        constexpr std::size_t maximum_regions=16;
        if(regions.size()<=maximum_regions)return;
        std::size_t first=0,second=1;double best=std::numeric_limits<double>::infinity();
        for(std::size_t i=0;i<regions.size();++i)for(std::size_t j=i+1;j<regions.size();++j){
            const auto merged=combine(regions[i],regions[j]);
            const double cost=double(merged.width)*merged.height-double(regions[i].width)*regions[i].height-
                              double(regions[j].width)*regions[j].height;
            if(cost<best){best=cost;first=i;second=j;}
        }
        auto merged=combine(regions[first],regions[second]);
        regions.erase(regions.begin()+std::ptrdiff_t(second));regions.erase(regions.begin()+std::ptrdiff_t(first));
        for(std::size_t i=0;i<regions.size();){
            if(overlaps(merged,regions[i])){merged=combine(merged,regions[i]);regions.erase(regions.begin()+std::ptrdiff_t(i));i=0;}
            else ++i;
        }
        regions.push_back(merged);
    };
    const auto include=[&](Rect value,const ContentTransform& transform){
        value=transform_rect(value,transform);
        if(value.width<=0||value.height<=0)return;
        // Outward-round with a two-device-pixel allowance for antialiasing.
        const auto left=std::clamp(std::floor(double(value.x)*scale)-2.,0.,double(next.viewport.width)*scale);
        const auto top=std::clamp(std::floor(double(value.y)*scale)-2.,0.,double(next.viewport.height)*scale);
        const auto right=std::clamp(std::ceil(double(value.x+value.width)*scale)+2.,left,double(next.viewport.width)*scale);
        const auto bottom=std::clamp(std::ceil(double(value.y+value.height)*scale)+2.,top,double(next.viewport.height)*scale);
        if(right>left&&bottom>top)append({float(left/scale),float(top/scale),float((right-left)/scale),float((bottom-top)/scale)});
    };
    const auto include_layer=[&](const CanvasLayerView& layer){
        for(const auto& value:layer.panels)include(value.bounds,layer.content_transform);
        for(const auto& value:layer.texts)include(value.bounds,layer.content_transform);
        for(const auto& value:layer.images)include(value.bounds,layer.content_transform);
    };
    const auto difference=[&](auto before,auto after,const ContentTransform& transform){
        const auto count=std::max(before.size(),after.size());
        for(std::size_t i=0;i<count;++i){
            if(i<before.size()&&i<after.size()&&before[i]==after[i])continue;
            if(i<before.size())include(before[i].bounds,transform);
            if(i<after.size())include(after[i].bounds,transform);
        }
    };
    const auto before=canvas_layers(old),after=canvas_layers(next);
    const auto count=std::max(before.size(),after.size());
    for(std::size_t i=0;i<count;++i){
        if(i>=before.size()){include_layer(after[i]);continue;}
        if(i>=after.size()){include_layer(before[i]);continue;}
        const auto& a=before[i];const auto& b=after[i];
        if(a.clip!=b.clip||a.content_transform!=b.content_transform||a.images_before_text!=b.images_before_text){
            include_layer(a);include_layer(b);continue;
        }
        difference(a.panels,b.panels,a.content_transform);
        difference(a.texts,b.texts,a.content_transform);
        difference(a.images,b.images,a.content_transform);
    }
    if(regions.empty())return {};
    auto bounds=regions.front();for(std::size_t i=1;i<regions.size();++i)bounds=combine(bounds,regions[i]);
    return {DamageKind::partial,bounds,std::move(regions)};
}
} // namespace ii::gui::detail
