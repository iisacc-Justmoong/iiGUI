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
        BasicListViewportSource result(std::move(*index));
        result.heights_.resize(result.index_.snapshot()->entries.size());
        for(const auto& [key,height]:heights){
            auto position=result.index_.find(key);
            if(!position||!std::isfinite(height)||height<1||height>8192)return error(ListErrorCode::invalid,"Invalid list row extent");
            result.heights_[*position]=height;
        }
        result.override_count_.reserve(result.heights_.size()+1);result.override_sum_.reserve(result.heights_.size()+1);
        result.override_count_.push_back(0);result.override_sum_.push_back(0);
        for(const auto height:result.heights_){
            result.override_count_.push_back(result.override_count_.back()+std::size_t(height.has_value()));
            result.override_sum_.push_back(result.override_sum_.back()+height.value_or(0));
        }
        return result;
    }
    [[nodiscard]] std::shared_ptr<const ListSnapshot> snapshot()const{return index_.snapshot();}
    [[nodiscard]] std::optional<double> content_extent(double fallback)const noexcept{
        if(!valid_fallback(fallback))return std::nullopt;return offset_unchecked(heights_.size(),fallback);
    }
    [[nodiscard]] std::optional<double> row_extent(std::size_t index,double fallback)const noexcept{
        if(!valid_fallback(fallback)||index>=heights_.size())return std::nullopt;return heights_[index].value_or(fallback);
    }
    [[nodiscard]] std::optional<double> row_offset(std::size_t index,double fallback)const noexcept{
        if(!valid_fallback(fallback)||index>heights_.size())return std::nullopt;return offset_unchecked(index,fallback);
    }
    [[nodiscard]] std::optional<std::size_t> row_at(double offset,double fallback)const noexcept{
        if(!valid_fallback(fallback)||!std::isfinite(offset)||offset<0||offset>=offset_unchecked(heights_.size(),fallback))return std::nullopt;
        std::size_t first=0,last=heights_.size();
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
        const auto data=index_.snapshot();const auto count=data->entries.size();
        ListViewportFrame result{.source_revision=data->revision,.content_height=offset_unchecked(count,request.row_height)};
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
        for(auto index=first;index<last;++index)result.rows.push_back({index,offset_unchecked(index,request.row_height)-offset,heights_[index].value_or(request.row_height)});
        result.anchor={data->entries[first_visible].key,first_visible,offset-offset_unchecked(first_visible,request.row_height),request.movement};
        return result;
    }
private:
    explicit BasicListViewportSource(ListIndex index):index_(std::move(index)){}
    [[nodiscard]] static bool valid_fallback(double value)noexcept{return std::isfinite(value)&&value>=18&&value<=256;}
    [[nodiscard]] double offset_unchecked(std::size_t index,double fallback)const noexcept{
        return double(index-override_count_[index])*fallback+override_sum_[index];
    }
    [[nodiscard]] static std::unexpected<ListError> error(ListErrorCode code,std::string message){return std::unexpected(ListError{code,std::move(message)});}
    ListIndex index_;
    std::vector<std::optional<double>> heights_;
    std::vector<std::size_t> override_count_;
    std::vector<double> override_sum_;
};

} // namespace ii::gui
