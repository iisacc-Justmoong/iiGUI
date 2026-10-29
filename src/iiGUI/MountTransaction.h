#pragma once
#include <utility>
namespace ii::gui::detail {
// Restore the previous generation's presentation if staging/geometry fails.
// The host supplies its own noexcept callback/exception barrier.
template<class Rollback>class MountTransaction final {
    Rollback rollback_;bool committed_=false;
public:
    explicit MountTransaction(Rollback rollback):rollback_(std::move(rollback)){}
    ~MountTransaction(){if(!committed_)rollback_();}
    void commit()noexcept{committed_=true;}
};
}
