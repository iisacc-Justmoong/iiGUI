#pragma once
#include <iiGUI/VirtualCollectionPlanner.h>
#include <iiSDKCore/Core/Property/Observable.h>
#include <algorithm>
#include <cmath>
#include <concepts>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
namespace ii::gui {
template<class Policy> class BasicVirtualMeasurements final {
    using T=typename Policy::Item;
    using Source=typename Policy::Source;
    using VirtualCollectionLayout=typename Policy::Layout;
    using VirtualViewportState=typename Policy::Viewport;
    using VirtualAnchor=typename Policy::Anchor;
    using VirtualRowMeasurement=typename Policy::Measurement;
    using Planner=typename Policy::PlannerType;
public:
    BasicVirtualMeasurements()=default;
    BasicVirtualMeasurements(const BasicVirtualMeasurements&)=delete;
    BasicVirtualMeasurements& operator=(const BasicVirtualMeasurements&)=delete;
    template<class Context> void track(Context& context){owner();(void)context.read(revision_);}
    auto plan(std::shared_ptr<const Source> source,VirtualCollectionLayout layout,
              VirtualViewportState viewport,VirtualAnchor anchor={},std::shared_ptr<const void> ambient={}){
        owner();if(!active_)throw std::logic_error("Measurement owner retired");
        if(!source)throw std::invalid_argument("Measurements require a model snapshot");
        if(!layout.measure_height){
            auto frame=Planner::frame(*source,layout,viewport,std::move(anchor));
            if(mode_){
                mode_=false;source_.reset();ambient_.reset();
                std::unordered_map<std::string,double>{}.swap(known_);
                std::vector<double>{}.swap(cells_);index_.reset({},0);
            }
            frame.source=std::move(source);return frame;
        }
        if(layout.horizontal)throw std::invalid_argument("Height measurement requires a vertical list or grid");
        auto uniform=layout;uniform.measure_height=false;
        const auto shape=Planner::frame(*source,uniform,VirtualViewportState{.width=viewport.width});
        const auto width=double(float((std::max)(0.,(viewport.width-layout.spacing*double(shape.columns-1))/double(shape.columns))));
        if(!mode_||source_!=source||layout_!=layout||width_!=width||ambient_!=ambient){
            if(generation_==UINT64_MAX)throw std::overflow_error("Measurement generation exhausted");
            if(width_!=width||ambient_!=ambient)known_.clear();
            else std::erase_if(known_,[&](const auto& entry){
                const auto next=source->find(entry.first),previous=source_?source_->find(entry.first):std::nullopt;
                if(!next||!previous)return true;
                if constexpr(std::equality_comparable<T>)return source->items()[*next].value!=source_->items()[*previous].value;
                else return true;
            });
            std::vector<double> cells(source->items().size(),layout.item_height),lines((cells.size()+shape.columns-1)/shape.columns,0);
            for(std::size_t n=0;n<cells.size();++n){
                if(auto found=known_.find(source->items()[n].key);found!=known_.end())cells[n]=found->second;
                lines[n/shape.columns]=(std::max)(lines[n/shape.columns],cells[n]);
            }
            index_.reset(std::move(lines),layout.spacing);cells_=std::move(cells);
            source_=std::move(source);ambient_=std::move(ambient);layout_=layout;width_=width;columns_=shape.columns;mode_=true;++generation_;
        }
        auto frame=Planner::frame(*source_,layout,viewport,std::move(anchor),&index_);
        frame.measurement_generation=generation_;frame.source=source_;return frame;
    }
    bool report(std::uint64_t generation,const std::vector<VirtualRowMeasurement>& entries){
        owner();if(!active_||!mode_||generation!=generation_)return false;
        struct Update final {std::size_t index;const VirtualRowMeasurement* value;};
        std::vector<Update> updates;std::vector<std::size_t> lines;
        std::unordered_set<std::string> keys;
        for(const auto& entry:entries){
            if(!std::isfinite(entry.width)||entry.width<0||entry.width>8192||!std::isfinite(entry.height)||entry.height<0||entry.height>32767||!keys.insert(entry.key).second)
                throw std::invalid_argument("Invalid or duplicate native row measurement");
            if(entry.width!=width_)continue;
            if(auto index=source_->find(entry.key)){updates.push_back({*index,&entry});lines.push_back(*index/columns_);}
        }
        std::sort(lines.begin(),lines.end());lines.erase(std::unique(lines.begin(),lines.end()),lines.end());
        for(const auto& update:updates){known_.insert_or_assign(update.value->key,update.value->height);cells_[update.index]=update.value->height;}
        bool changed=false;
        for(auto line:lines){
            const auto begin=cells_.begin()+line*columns_,end=cells_.begin()+(std::min)(cells_.size(),(line+1)*columns_);
            const auto height=*std::max_element(begin,end);
            if(index_.height(line)!=height){index_.set(line,height);changed=true;}
        }
        if(changed){if(revision_.get()==UINT64_MAX)throw std::overflow_error("Measurement revision exhausted");revision_.set(revision_.get()+1);}
        return changed;
    }
    void close(){owner();active_=false;source_.reset();ambient_.reset();known_.clear();cells_.clear();index_.reset({},0);}
private:
    void owner()const{if(owner_!=std::this_thread::get_id())throw std::logic_error("Measurements require their owner thread");}
    const std::thread::id owner_=std::this_thread::get_id();
    bool active_=true,mode_=false;
    std::uint64_t generation_=0;
    double width_=0;
    std::size_t columns_=1;
    VirtualCollectionLayout layout_;
    std::shared_ptr<const Source> source_;
    std::shared_ptr<const void> ambient_;
    std::unordered_map<std::string,double> known_;
    std::vector<double> cells_;
    VirtualLineIndex index_;
    ii::core::Observable<std::uint64_t> revision_{0};
};
} // namespace ii::gui
