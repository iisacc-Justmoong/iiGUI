#pragma once
#include <algorithm>
#include <bit>
#include <cmath>
#include <stdexcept>
#include <vector>

namespace ii::gui {
// Fenwick prefix index. Starts and visible-range searches do not scan rows.
class VirtualLineIndex final {
public:
    void reset(std::vector<double> heights,double spacing){
        if(heights.size()>65536||!valid(spacing))throw std::invalid_argument("Invalid native line index");
        for(auto height:heights)if(!valid(height))throw std::invalid_argument("Invalid native row height");
        std::vector<double> tree(heights.size()+1);
        for(std::size_t n=1;n<tree.size();++n){tree[n]+=heights[n-1]+spacing;const auto parent=n+(n&(~n+1));if(parent<tree.size())tree[parent]+=tree[n];}
        heights_=std::move(heights);tree_=std::move(tree);spacing_=spacing;
    }
    std::size_t size()const noexcept{return heights_.size();}
    double height(std::size_t row)const{return heights_.at(row);}
    double start(std::size_t row)const{
        if(row>size())throw std::out_of_range("Native line index is out of range");
        double sum=0;for(auto n=row;n;n-=n&(~n+1))sum+=tree_[n];return sum;
    }
    double extent()const{return size()?(std::max)(0.,start(size())-spacing_):0;}
    void set(std::size_t row,double height){
        if(row>=size()||!valid(height))throw std::invalid_argument("Invalid native row update");
        const auto delta=height-heights_[row];heights_[row]=height;
        for(auto n=row+1;n<tree_.size();n+=n&(~n+1))tree_[n]+=delta;
    }
    // Row containing a coordinate, treating its trailing gap as part of the row.
    std::size_t at(double coordinate)const{return size()?(std::min)(search(coordinate,true),size()-1):0;}
    // Exclusive visible end: first row-start not less than this coordinate.
    std::size_t end(double coordinate)const{
        if(!std::isfinite(coordinate)||coordinate<0)throw std::invalid_argument("Invalid line coordinate");
        if(!size()||coordinate==0)return 0;
        return (std::min)(size(),search(coordinate,false)+1);
    }
private:
    static bool valid(double n){return std::isfinite(n)&&n>=0&&n<=32767;}
    std::size_t search(double coordinate,bool inclusive)const{
        if(!std::isfinite(coordinate)||coordinate<0)throw std::invalid_argument("Invalid line coordinate");
        std::size_t row=0;double sum=0;
        for(auto bit=std::bit_floor(size());bit;bit>>=1){
            const auto next=row+bit;
            if(next<=size()&&(inclusive?sum+tree_[next]<=coordinate:sum+tree_[next]<coordinate)){row=next;sum+=tree_[next];}
        }
        return row;
    }
    std::vector<double> heights_,tree_;
    double spacing_=0;
};
} // namespace ii::gui
