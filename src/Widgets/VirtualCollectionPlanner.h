#pragma once
#include <iiGUI/VirtualLineIndex.h>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <optional>
#include <stdexcept>
#include <utility>
namespace ii::gui {
// Plans only the visible and overscan cells of an already indexed snapshot.
template<class Policy> struct BasicVirtualCollectionPlanner final {
    using VirtualCollectionLayout=typename Policy::Layout;
    using VirtualViewportState=typename Policy::Viewport;
    using VirtualAnchor=typename Policy::Anchor;
    using VirtualCell=typename Policy::Cell;
    using CollectionReveal=typename Policy::Reveal;
template<class Source>
static auto frame(const Source& source,VirtualCollectionLayout layout,
                  VirtualViewportState viewport,VirtualAnchor anchor={},const VirtualLineIndex* measured=nullptr){
    using T=typename Source::Value;
    const auto metric=[](double n){return std::isfinite(n)&&n>=0&&n<=32767;};
    if(!metric(layout.item_height)||!layout.item_height||!metric(layout.item_width)||!metric(layout.spacing)||
       layout.columns>Policy::maximum_items||layout.overscan>Policy::maximum_items||
       (layout.horizontal&&(!layout.item_width||layout.columns>1)))
        throw std::invalid_argument("Invalid virtual collection layout");
    if(!metric(viewport.width)||!metric(viewport.height)||viewport.width>8192||viewport.height>8192||
       !std::isfinite(viewport.x)||!std::isfinite(viewport.y)||viewport.x<0||viewport.y<0||
       !std::isfinite(anchor.delta)||anchor.delta<0||static_cast<unsigned>(viewport.reveal_alignment)>3)
        throw std::invalid_argument("Invalid virtual collection viewport or anchor");
    const auto& items=source.items();
    typename Policy::template Frame<T> result;
    result.width=viewport.width;result.height=viewport.height;
    result.movement=viewport.movement;
    result.layout=layout;result.reveal_serial=viewport.reveal_serial;
    if(!layout.horizontal){
        result.columns=layout.columns?layout.columns:layout.item_width?
            std::size_t(std::clamp(std::floor((viewport.width+layout.spacing)/(layout.item_width+layout.spacing)),1.,double(Policy::maximum_items))):1;
    }
    const auto cell_width=layout.horizontal?layout.item_width:
        (std::max)(0.,(viewport.width-layout.spacing*double(result.columns-1))/double(result.columns));
    const auto line_count=layout.horizontal?items.size():(items.size()+result.columns-1)/result.columns;
    if(layout.measure_height&&(layout.horizontal||!measured||measured->size()!=line_count))
        throw std::invalid_argument("Measured heights require a vertical native line index");
    result.measure_height=layout.measure_height;
    const auto stride=(layout.horizontal?layout.item_width:layout.item_height)+layout.spacing;
    const auto logical=layout.measure_height?measured->extent():line_count?double(line_count)*stride-layout.spacing:0.;
    const auto start=[&](std::size_t line){return layout.measure_height?measured->start(line):double(line)*stride;};
    const auto height=[&](std::size_t line){return layout.measure_height?measured->height(line):layout.item_height;};
    result.content_width=layout.horizontal?logical:viewport.width;
    result.content_height=layout.horizontal?viewport.height:logical;
    auto offset=layout.horizontal?viewport.x:viewport.y;
    std::optional<std::size_t> anchored_index;
    if(!anchor.key.empty()&&anchor.movement==viewport.movement&&!items.empty()){
        const auto index=source.find(anchor.key).value_or((std::min)(anchor.index,items.size()-1));
        anchored_index=index;
        const auto line=layout.horizontal?index:index/result.columns;
        const auto length=layout.measure_height?height(line)+layout.spacing:stride;
        offset=start(line)+(std::min)(anchor.delta,(std::max)(0.,std::nextafter(length,0.)));
    }
    const auto visible_extent=layout.horizontal?viewport.width:viewport.height;
    if(viewport.width>0&&viewport.height>0&&!viewport.reveal_key.empty())if(auto index=source.find(viewport.reveal_key)){
        const auto line=layout.horizontal?*index:*index/result.columns;
        const auto beginning=start(line),length=layout.horizontal?layout.item_width:height(line);
        if(viewport.reveal_alignment==CollectionReveal::begin||length>=visible_extent)offset=beginning;
        else if(viewport.reveal_alignment==CollectionReveal::end)offset=beginning+length-visible_extent;
        else if(viewport.reveal_alignment==CollectionReveal::center)offset=beginning+(length-visible_extent)/2;
        else if(beginning<offset)offset=beginning;
        else if(beginning+length>offset+visible_extent)offset=beginning+length-visible_extent;
        result.revealed_key=viewport.reveal_key;
    }
    offset=std::clamp(offset,0.,(std::max)(0.,logical-visible_extent));
    result.x=layout.horizontal?offset:0;result.y=layout.horizontal?0:offset;
    std::size_t first=0,count=0;
    if(viewport.width>0&&viewport.height>0&&!items.empty()){
        const auto leading=layout.measure_height?measured->at(offset):std::size_t(std::clamp(std::floor(offset/stride),0.,double(line_count-1)));
        const auto last=layout.measure_height?(logical==0?line_count:measured->end((std::min)(offset+visible_extent,logical))):std::size_t(std::clamp(std::ceil((offset+visible_extent)/stride),0.,double(line_count)));
        const auto begin=leading>layout.overscan?leading-layout.overscan:0;
        const auto end=(std::min)(line_count,last+layout.overscan);
        const auto per_line=layout.horizontal?1:result.columns;
        first=begin*per_line;count=(std::min)(items.size(),end*per_line)-first;
        const auto index=anchored_index&&*anchored_index/per_line==leading?*anchored_index:leading*per_line;
        result.anchor={items[index].key,index,offset-start(leading),viewport.movement};
        if(logical==0)result.anchor={}; // An invisible probe is not a review anchor.
        if(layout.measure_height&&logical==0){first=items.size()-1;count=1;}
    }
    result.collection.total=items.size();
    result.collection.options={.first=first,.count=count,.spacing=layout.spacing,.horizontal=layout.horizontal,.sparse=layout.measure_height};
    if(layout.measure_height){
        for(std::size_t n=first;n<first+count;++n)if(logical==0||height(n/result.columns)>0)result.collection.items.push_back(items[n]);
        result.collection.options.first=result.collection.items.empty()?first:result.collection.items.front().index;
        result.collection.options.count=result.collection.items.size();
    }else result.collection.items.assign(items.begin()+first,items.begin()+first+count);
    result.cells.reserve(result.collection.items.size());
    for(const auto& item:result.collection.items){
        result.cells.push_back(layout.horizontal?
            VirtualCell{double(item.index)*stride-offset,0,cell_width,viewport.height}:
            VirtualCell{double(item.index%result.columns)*(cell_width+layout.spacing),start(item.index/result.columns)-offset,cell_width,height(item.index/result.columns)});
    }
    return result;
}
};
} // namespace ii::gui
