#pragma once
#include <algorithm>
#include <memory>
#include <optional>
#include <string_view>
#include <vector>

namespace ii::gui {
// A participant changes its local model without callbacks in assign(); publish
// synchronizes the host and may reenter the group. Only published participants
// take part, so an uncommitted candidate cannot change the current selection.
class RadioGroupMember {
public:
    virtual ~RadioGroupMember()=default;
    [[nodiscard]] virtual std::string_view group()const noexcept=0;
    [[nodiscard]] virtual bool selected()const noexcept=0;
    [[nodiscard]] virtual bool participating()const noexcept=0;
    virtual void assign(bool)noexcept=0;
    virtual void publish()noexcept=0;
};

// Optional navigation port. The opaque domain keeps navigation inside one
// presentation tree without coupling the group model to a particular GUI.
struct RadioNavigationPosition final {
    double x=0,y=0;
    std::shared_ptr<const void> domain;
};
class RadioGroupNavigation {
public:
    virtual ~RadioGroupNavigation()=default;
    [[nodiscard]] virtual std::optional<RadioNavigationPosition> navigation_position()const=0;
    // May report a stale/unavailable target without activating it.
    [[nodiscard]] virtual bool navigate_to(std::string_view expected_group)=0;
};

class RadioGroups final {
public:
    // Weak ownership preserves independent generation lifetimes. Registration
    // reserves membership while the candidate is still being built.
    [[nodiscard]] bool prepare(const std::shared_ptr<RadioGroupMember>& member){
        if(!member)return false;
        for(const auto& weak:members_)if(auto current=weak.lock();current==member)return true;
        if(members_.size()>=4096){std::erase_if(members_,[](const auto& weak){return weak.expired();});if(members_.size()>=4096)return false;}
        try{members_.push_back(member);return true;}catch(...){return false;}
    }
    [[nodiscard]] bool select(const std::shared_ptr<RadioGroupMember>& member,bool value)noexcept{
        if(!member||!member->participating()||!contains(member))return false;
        dirty_=true;
        const auto name=member->group();
        if(value&&!name.empty())for(std::size_t i=0;i<members_.size();++i){
            auto other=members_[i].lock();
            if(other&&other!=member&&other->participating()&&other->group()==name&&other->selected())other->assign(false);
        }
        if(member->selected()!=value)member->assign(value);
        return settle();
    }
    // Called after publication and when a selected participant changes group.
    [[nodiscard]] bool refresh(const std::shared_ptr<RadioGroupMember>& member)noexcept{
        if(!member||!member->participating()||!contains(member))return false;
        return select(member,member->selected());
    }
    [[nodiscard]] std::vector<std::shared_ptr<RadioGroupNavigation>> navigation_members(std::string_view name)const{
        std::vector<std::shared_ptr<RadioGroupNavigation>> result;
        if(name.empty())return result;
        for(const auto& weak:members_)if(auto member=weak.lock();member&&member->participating()&&member->group()==name)
            if(auto navigation=std::dynamic_pointer_cast<RadioGroupNavigation>(member))result.push_back(std::move(navigation));
        return result;
    }
    void retire(const std::shared_ptr<RadioGroupMember>& member)noexcept{
        std::erase_if(members_,[&](const auto& weak){auto current=weak.lock();return !current||current==member;});
        dirty_=true;(void)settle();
    }
private:
    [[nodiscard]] bool contains(const std::shared_ptr<RadioGroupMember>& member)const noexcept{
        for(const auto& weak:members_)if(weak.lock()==member)return true;return false;
    }
    [[nodiscard]] bool settle()noexcept{
        if(flushing_)return true;
        flushing_=true;struct Reset final{bool& value;~Reset(){value=false;}}reset{flushing_};
        for(unsigned pass=0;pass<16;++pass){
            dirty_=false;
            for(std::size_t i=0;i<members_.size();++i){
                auto member=members_[i].lock();if(member&&member->participating())member->publish();
                if(dirty_)break;
            }
            if(!dirty_)return true;
        }
        return false;
    }
    std::vector<std::weak_ptr<RadioGroupMember>> members_;
    bool flushing_=false,dirty_=false;
};
} // namespace ii::gui
