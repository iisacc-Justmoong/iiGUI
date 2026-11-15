#pragma once
#include <utility>

namespace ii::gui::detail {
// Owner-thread latch. A callback may invalidate while a frame is being built;
// that change belongs to a later frame and cannot be cleared by the first one.
class SurfaceInvalidation final {
public:
    void invalidate()noexcept{pending_=true;}
    [[nodiscard]] bool pending()const noexcept{return pending_;}
    template<class Work> bool flush(Work&& work){
        if(!pending_ || active_)return false;
        pending_=false;active_=true;
        struct Reset final{bool& active;~Reset(){active=false;}} reset{active_};
        try{std::forward<Work>(work)();}
        catch(...){pending_=true;throw;}
        return true;
    }
private:
    bool pending_=true,active_=false;
};
} // namespace ii::gui::detail
