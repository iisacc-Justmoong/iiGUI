#include <iiGUI/VirtualCollectionPlanner.h>
#include <iiGUI/VirtualViewport.h>
#include <algorithm>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

// An independently supplied policy predating gesture anchors must remain usable.
namespace {
enum class Reveal {contain,begin,end,center};
struct Layout {double item_height=0,item_width=0,spacing=0;bool horizontal=false;std::size_t columns=0,overscan=1;bool measure_height=false;};
struct Viewport {double width=0,height=0,x=0,y=0;std::uint64_t movement=0;std::string reveal_key;Reveal reveal_alignment=Reveal::contain;std::uint64_t reveal_serial=0;friend bool operator==(const Viewport&,const Viewport&)=default;};
struct Anchor {std::string key;std::size_t index=0;double delta=0;std::uint64_t movement=0;};
struct Cell {double x=0,y=0,width=0,height=0;};
struct Item {std::string key;int value=0;std::size_t index=0;};
struct Options {std::size_t first=0,count=0;double spacing=0;bool horizontal=false,sparse=false;};
template<class T>struct Frame {
    struct Collection {std::size_t total=0;Options options;std::vector<Item> items;} collection;
    std::vector<Cell> cells;std::size_t columns=1;double width=0,height=0,content_width=0,content_height=0,x=0,y=0;
    std::uint64_t movement=0;Anchor anchor;bool measure_height=false;Layout layout;std::string revealed_key;std::uint64_t reveal_serial=0;
};
struct Policy {
    using Layout=::Layout;using Viewport=::Viewport;using Anchor=::Anchor;using Cell=::Cell;using Reveal=::Reveal;
    template<class T>using Frame=::Frame<T>;static constexpr std::size_t maximum_items=65536;
};
struct ViewportPolicy {
    using VirtualViewportState=Viewport;using VirtualAnchor=Anchor;using CollectionReveal=Reveal;
    static std::string collection_key(std::string_view value){return std::string(value);}
};
struct Source {
    using Value=int;
    std::vector<Item> values{{"a",0,0},{"b",1,1},{"c",2,2}};
    const std::vector<Item>& items()const{return values;}
    std::optional<std::size_t> find(const std::string& key)const{
        for(std::size_t i=0;i<values.size();++i)if(values[i].key==key)return i;return {};
    }
};
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
}
int main(){
    ii::gui::BasicVirtualViewport<ViewportPolicy> viewport;Source source;
    viewport.observe({.width=100,.height=20,.y=20});
    using Planner=ii::gui::BasicVirtualCollectionPlanner<Policy>;
    auto first=Planner::frame(source,{.item_height=20},viewport.current(),viewport.anchor());
    check(viewport.commit(first)&&viewport.anchor().key=="b","Legacy policy did not commit its anchor");
    check(viewport.begin_focus(5,5)==0,"Legacy policy unexpectedly accepted a proportional anchor");
    auto resized=Planner::frame(source,{.item_height=40},viewport.current(),viewport.anchor());
    check(resized.y==40&&viewport.commit(resized),"Legacy pixel anchor changed after a resize");
}
