#include <iiGUI/NativeView.h>
#include <cmath>

namespace ii::gui {
namespace {
bool valid(const NativeViewOptions& o){return std::isfinite(o.width)&&std::isfinite(o.height)&&o.width>=0&&o.height>=0&&o.width<=8192&&o.height<=8192;}
}
struct NativeView::Impl final {
    const std::thread::id owner=std::this_thread::get_id();
    Widget root,slot;
    NativeViewOptions options;
    bool closed=false,changing=false;
};
core::Result<std::shared_ptr<NativeView::Impl>> NativeView::lock()const{
    auto s=state_.lock();if(!s)return core::fail(core::ErrorCode::closed,"Native view owner retired");
    if(s->owner!=std::this_thread::get_id())return core::fail(core::ErrorCode::wrong_executor,"Native view requires its owner thread");
    if(s->closed)return core::fail(core::ErrorCode::closed,"Native view owner retired");
    if(auto live=s->root.snapshot();!live)return std::unexpected(live.error());
    return s;
}
core::Result<NativeView> NativeView::create(WidgetBuilder& b,Widget parent,std::string_view id,NativeViewOptions o){
    if(!valid(o))return core::fail(core::ErrorCode::invalid_argument,"Native view requires finite dimensions from zero through 8192");
    auto root=b.zstack(parent,id,{.spacing=0,.alignment=StackAlignment::stretch});if(!root)return std::unexpected(root.error());
    if(auto r=root->set_extent({o.width,o.height});!r)return std::unexpected(r.error());
    if(auto r=root->set_enabled(o.enabled);!r)return std::unexpected(r.error());
    if(auto r=root->set_clip_content(o.clip);!r)return std::unexpected(r.error());
    auto slot=b.content_host(*root,std::string(id)+"/@native-content");if(!slot)return std::unexpected(slot.error());
    auto s=std::make_shared<Impl>();s->root=*root;s->slot=*slot;s->options=o;s->options.source.reset();
    if(auto r=b.retain(s);!r)return std::unexpected(r.error());
    if(auto r=b.on_retire([weak=std::weak_ptr(s)]{if(auto s=weak.lock()){s->closed=true;s->options.source.reset();}});!r)return std::unexpected(r.error());
    NativeView view;view.root_=*root;view.state_=s;
    if(auto r=view.set_source(std::move(o.source));!r)return std::unexpected(r.error());
    return view;
}
core::Result<NativeViewOptions> NativeView::options()const{auto s=lock();if(!s)return std::unexpected(s.error());return (*s)->options;}
core::Result<Widget> NativeView::content_item(std::string_view id)const{auto s=lock();if(!s)return std::unexpected(s.error());return (*s)->slot.content_item(id);}
core::Result<void> NativeView::set_source(std::shared_ptr<NativeViewSource> source)const{
    auto previous=options();if(!previous)return std::unexpected(previous.error());previous->source=std::move(source);return set_options(std::move(*previous));
}
core::Result<void> NativeView::set_options(NativeViewOptions next)const{
    auto locked=lock();if(!locked)return std::unexpected(locked.error());auto s=*locked;
    if(s->changing)return core::fail(core::ErrorCode::busy,"Native view is constructing a candidate");
    if(!valid(next))return core::fail(core::ErrorCode::invalid_argument,"Native view requires finite dimensions from zero through 8192");
    const auto previous=s->options;if(previous==next)return {};
    s->changing=true;struct Restore{bool& value;~Restore(){value=false;}}restore{s->changing};
    if(previous.source!=next.source){
        WidgetFactory factory;
        if(next.source)factory=[source=next.source](WidgetBuilder& b)->core::Result<void>{
            if(auto r=b.retain(source);!r)return r;
            return source->build(b);
        };
        auto changed=s->slot.set_content(std::move(factory),[s,source=next.source]()->core::Result<void>{
            if(s->closed)return core::fail(core::ErrorCode::closed,"Native view retired during candidate construction");
            s->options.source=source;return {};
        });
        if(!changed)return changed;
    }
    if(s->closed)return core::fail(core::ErrorCode::closed,"Native view owner retired");
    // Provider side effects cannot be rolled back. All option validation and
    // candidate construction precede ordinary retained-widget updates.
    s->options=next;
    if(previous.width!=next.width||previous.height!=next.height)
        if(auto r=s->root.set_extent({next.width,next.height});!r)return r;
    if(previous.enabled!=next.enabled)if(auto r=s->root.set_enabled(next.enabled);!r)return r;
    if(previous.clip!=next.clip)if(auto r=s->root.set_clip_content(next.clip);!r)return r;
    return {};
}
}
