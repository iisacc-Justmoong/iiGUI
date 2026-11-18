#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>
namespace ii::gui {
template<class Policy>
class BasicListViewportSource final {
    using ListSnapshot=typename Policy::ListSnapshot;
    using ListIndex=typename Policy::ListIndex;
    using ListViewportFrame=typename Policy::ListViewportFrame;
    using ListViewportRequest=typename Policy::ListViewportRequest;
    using ListReveal=typename Policy::ListReveal;
    using ListError=typename Policy::ListError;
    using ListErrorCode=typename Policy::ListErrorCode;
    template<class T>using ListResult=std::expected<T,ListError>;

public:
    [[nodiscard]] static ListResult<BasicListViewportSource> create(std::shared_ptr<const ListSnapshot> snapshot,
            const std::unordered_map<std::string,double>& heights={}){
        auto index=ListIndex::create(std::move(snapshot));if(!index)return std::unexpected(index.error());
        return create(std::move(*index), heights);
    }
    [[nodiscard]] static ListResult<BasicListViewportSource> create(ListIndex index,
            const std::unordered_map<std::string,double>& heights={}){
        BasicListViewportSource result(std::move(index));
        result.heights_.reserve(heights.size());
        for(const auto& [key,height]:heights){
            auto position=result.index_.find(key);
            if(!position||!std::isfinite(height)||height<1||height>8192)return error(ListErrorCode::invalid,"Invalid list row extent");
            result.heights_.push_back({*position, height, 0});
        }
        std::ranges::sort(result.heights_, {}, &RowHeight::index);
        double sum = 0;
        for (auto& row : result.heights_) {
            sum += row.height;
            row.sum = sum;
        }
        return result;
    }
    [[nodiscard]] std::shared_ptr<const ListSnapshot> snapshot()const{return index_.snapshot();}
    [[nodiscard]] const ListIndex& index() const noexcept { return index_; }
    [[nodiscard]] std::optional<double> content_extent(double fallback)const noexcept{
        if(!valid_fallback(fallback))return std::nullopt;return offset_unchecked(index_.size(),fallback);
    }
    [[nodiscard]] std::optional<double> row_extent(std::size_t index,double fallback)const noexcept{
        if(!valid_fallback(fallback)||index>=index_.size())return std::nullopt;return extent_unchecked(index,fallback);
    }
    [[nodiscard]] std::optional<double> row_offset(std::size_t index,double fallback)const noexcept{
        if(!valid_fallback(fallback)||index>index_.size())return std::nullopt;return offset_unchecked(index,fallback);
    }
    [[nodiscard]] std::optional<std::size_t> row_at(double offset,double fallback)const noexcept{
        if(!valid_fallback(fallback)||!std::isfinite(offset)||offset<0||offset>=offset_unchecked(index_.size(),fallback))return std::nullopt;
        std::size_t first=0,last=index_.size();
        while(first<last){const auto middle=first+(last-first)/2;
            if(offset_unchecked(middle+1,fallback)<=offset)first=middle+1;else last=middle;}
        return first;
    }
    [[nodiscard]] ListResult<ListViewportFrame> frame(const ListViewportRequest& request)const{
        const auto extent=[](double value){return std::isfinite(value)&&value>=0&&value<=8192;};
        if(!extent(request.width)||!extent(request.height)||!std::isfinite(request.offset)||request.offset<0||
           !std::isfinite(request.row_height)||request.row_height<18||request.row_height>256||request.overscan>128||
           !std::isfinite(request.anchor.within)||request.anchor.within<0||unsigned(request.reveal)>unsigned(ListReveal::center))
            return error(ListErrorCode::invalid,"Invalid list viewport request");
        const auto count=index_.size();
        ListViewportFrame result{.source_revision=index_.revision(),.content_height=offset_unchecked(count,request.row_height)};
        auto offset=request.offset;
        if(!request.anchor.key.empty()&&request.anchor.movement==request.movement){
            if(auto index=index_.find(request.anchor.key)){
                const auto begin=offset_unchecked(*index,request.row_height),end=offset_unchecked(*index+1,request.row_height);
                offset=(std::min)(begin+request.anchor.within,std::nextafter(end,begin));
            }
        }
        if(!request.reveal_key.empty()){
            auto found=index_.find(request.reveal_key);if(!found)return error(ListErrorCode::not_found,"List reveal key was not found");
            const auto begin=offset_unchecked(*found,request.row_height),end=offset_unchecked(*found+1,request.row_height),height=end-begin;
            if(request.reveal==ListReveal::start)offset=begin;
            else if(request.reveal==ListReveal::end)offset=end-request.height;
            else if(request.reveal==ListReveal::center)offset=begin+(height-request.height)/2;
            else if(height>=request.height)offset=begin;
            else if(begin<offset)offset=begin;
            else if(end>offset+request.height)offset=end-request.height;
            result.revealed_key=request.reveal_key;
        }
        offset=std::clamp(offset,0.,(std::max)(0.,result.content_height-request.height));result.offset=offset;
        if(!count||request.width==0||request.height==0)return result;
        const auto first_visible=row_at(offset,request.row_height).value_or(count-1);
        std::size_t visible_end=first_visible+1,end_search=count;
        while(visible_end<end_search){const auto middle=visible_end+(end_search-visible_end)/2;
            if(offset_unchecked(middle,request.row_height)<offset+request.height)visible_end=middle+1;else end_search=middle;}
        const auto first=first_visible>request.overscan?first_visible-request.overscan:0;
        const auto last=(std::min)(count,visible_end+request.overscan);result.rows.reserve(last-first);
        for(auto index=first;index<last;++index)result.rows.push_back({index,offset_unchecked(index,request.row_height)-offset,extent_unchecked(index,request.row_height)});
        result.anchor={index_.entry(first_visible).key,first_visible,offset-offset_unchecked(first_visible,request.row_height),request.movement};
        return result;
    }
private:
    struct RowHeight { std::size_t index; double height, sum; };
    explicit BasicListViewportSource(ListIndex index):index_(std::move(index)){}
    [[nodiscard]] static bool valid_fallback(double value)noexcept{return std::isfinite(value)&&value>=18&&value<=256;}
    [[nodiscard]] auto height_at_or_after(std::size_t index) const noexcept {
        return std::ranges::lower_bound(heights_, index, {}, &RowHeight::index);
    }
    [[nodiscard]] double extent_unchecked(std::size_t index,double fallback)const noexcept{
        const auto row = height_at_or_after(index);
        return row != heights_.end() && row->index == index ? row->height : fallback;
    }
    [[nodiscard]] double offset_unchecked(std::size_t index,double fallback)const noexcept{
        if (heights_.empty()) return double(index) * fallback;
        const auto row = height_at_or_after(index);
        const auto count = std::size_t(row - heights_.begin());
        const auto sum = count ? (row - 1)->sum : 0;
        return double(index - count) * fallback + sum;
    }
    [[nodiscard]] static std::unexpected<ListError> error(ListErrorCode code,std::string message){return std::unexpected(ListError{code,std::move(message)});}
    ListIndex index_;
    std::vector<RowHeight> heights_;
};

} // namespace ii::gui
