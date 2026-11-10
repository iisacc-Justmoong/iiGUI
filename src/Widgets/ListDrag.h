#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <expected>
#include <memory>
#include <optional>
#include <string>
#include <utility>
namespace ii::gui {
template<class Policy>
class BasicListDrag final {
    using ListSnapshot=typename Policy::ListSnapshot;
    using ListIndex=typename Policy::ListIndex;
    using ListDragPoint=typename Policy::ListDragPoint;
    using ListDragProposal=typename Policy::ListDragProposal;
    using ListDragUpdate=typename Policy::ListDragUpdate;
    using ListEdit=typename Policy::ListEdit;
    using ListEditKind=typename Policy::ListEditKind;
    using ListError=typename Policy::ListError;
    using ListErrorCode=typename Policy::ListErrorCode;
    using ListDragViewport=typename Policy::Viewport;
    template<class T>using ListResult=std::expected<T,ListError>;

public:
    [[nodiscard]] static ListResult<BasicListDrag> begin(std::shared_ptr<const ListSnapshot> snapshot,
            std::string key,ListDragPoint origin,double threshold=4,double scroll_edge=16){
        if(!point(origin)||!range(threshold,0,256)||!range(scroll_edge,0,8192))
            return error(ListErrorCode::invalid,"Invalid list drag configuration");
        auto index=ListIndex::create(std::move(snapshot));if(!index)return std::unexpected(index.error());
        auto found=index->find(key);if(!found)return error(ListErrorCode::not_found,"List drag source was not found");
        const auto& entry=index->snapshot()->entries[*found];
        if(!entry.enabled||!entry.draggable)return error(ListErrorCode::invalid,"List row cannot be moved");
        return BasicListDrag(std::move(*index),std::move(key),*found,origin,threshold,scroll_edge);
    }
    [[nodiscard]] ListResult<ListDragUpdate> update(ListDragPoint position,const ListDragViewport& viewport){
        if(!active_)return error(ListErrorCode::invalid,"List drag has ended");
        if(!point(position)||!viewport.source||!range(viewport.width,0,8192)||!range(viewport.height,0,8192)||
           !range(viewport.offset,0,1e15)||!range(viewport.row_height,18,256)){
            proposal_.reset();return error(ListErrorCode::invalid,"Invalid list drag viewport");
        }
        const auto snapshot=viewport.source->snapshot();
        const auto expected=index_.snapshot();
        if(snapshot->revision!=expected->revision||(snapshot!=expected&&snapshot->entries!=expected->entries)){
            proposal_.reset();active_=false;return error(ListErrorCode::stale,"List changed during the drag");
        }
        if(!dragging_&&std::hypot(position.x-origin_.x,position.y-origin_.y)>=threshold_)dragging_=true;
        proposal_.reset();ListDragUpdate result;result.dragging=dragging_;
        if(!dragging_||viewport.width==0||viewport.height==0)return result;
        const auto extent=*viewport.source->content_extent(viewport.row_height);
        const auto edge=(std::min)(scroll_edge_,viewport.height/2);
        if(position.y<edge&&viewport.offset>0)result.scroll_direction=-1;
        else if(position.y>viewport.height-edge&&viewport.offset+viewport.height<extent)result.scroll_direction=1;
        if(position.x<0||position.x>=viewport.width||position.y<0||position.y>=viewport.height)return result;
        const auto& entries=index_.snapshot()->entries;const auto logical=viewport.offset+position.y;
        std::size_t boundary=entries.size();std::string target;bool after=true;
        if(logical<extent){
            auto row=viewport.source->row_at(logical,viewport.row_height);if(!row||*row>=entries.size())return result;
            const auto& entry=entries[*row];if(!entry.enabled)return result;
            const auto start=*viewport.source->row_offset(*row,viewport.row_height);
            const auto height=*viewport.source->row_extent(*row,viewport.row_height);
            after=(logical-start)>=height/2;boundary=*row+std::size_t(after);target=entry.key;
        }else if(!entries.empty())target=entries.back().key;
        const auto destination=boundary-std::size_t(from_<boundary);
        if(destination==from_)return result;
        ListEdit request{.revision=index_.snapshot()->revision,.kind=ListEditKind::move,.key=key_,
                         .before=boundary<entries.size()?entries[boundary].key:std::string{}};
        proposal_=ListDragProposal{std::move(request),from_,destination,std::move(target),after};
        result.proposal=proposal_;return result;
    }
    [[nodiscard]] ListResult<std::optional<ListDragProposal>> finish(){
        if(!active_)return error(ListErrorCode::invalid,"List drag has ended");
        active_=false;return std::exchange(proposal_,std::nullopt);
    }
    void cancel()noexcept{active_=false;proposal_.reset();}
private:
    BasicListDrag(ListIndex index,std::string key,std::size_t from,ListDragPoint origin,double threshold,double edge)
        :index_(std::move(index)),key_(std::move(key)),from_(from),origin_(origin),threshold_(threshold),scroll_edge_(edge){}
    [[nodiscard]] static bool range(double n,double low,double high)noexcept{return std::isfinite(n)&&n>=low&&n<=high;}
    [[nodiscard]] static bool point(ListDragPoint p)noexcept{return std::isfinite(p.x)&&std::isfinite(p.y)&&std::abs(p.x)<=1e9&&std::abs(p.y)<=1e9;}
    [[nodiscard]] static std::unexpected<ListError> error(ListErrorCode code,std::string message){return std::unexpected(ListError{code,std::move(message)});}
    ListIndex index_;
    std::string key_;
    std::size_t from_=0;
    ListDragPoint origin_;
    double threshold_=4,scroll_edge_=16;
    std::optional<ListDragProposal> proposal_;
    bool active_=true,dragging_=false;
};

} // namespace ii::gui
