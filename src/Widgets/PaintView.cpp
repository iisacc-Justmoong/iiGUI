#include <iiGUI/PaintView.h>
#include "WidgetState.h"

#include <exception>
#include <stop_token>
#include <thread>
#include <utility>

namespace ii::gui {
namespace {
struct Invalidation final {
    runtime::EventLoop::Sender sender;
    std::optional<runtime::TaskToken> token;
    std::function<void()> callback;
    std::uint64_t revision=0;
    bool active=true, armed=false, dirty=true;
    ~Invalidation() { retire(); }
    void mark() {
        if(!active)return;
        ++revision;
        if(dirty)return;
        dirty=true;
        if(armed && token)(void)sender.signal(*token,runtime::Clock::now());
    }
    void arm() { armed=true; if(active && dirty && token)(void)sender.signal(*token,runtime::Clock::now()); }
    void retire() noexcept {
        if(!active)return;
        active=false;callback={};
        if(token){(void)token->cancel();token.reset();}
    }
};
detail::WidgetFrameHooks hooks_for(const PaintViewServices& services,const std::shared_ptr<Invalidation>& invalidation) {
    detail::WidgetFrameHooks hooks;
    if(services.events)hooks.events=[events=services.events]()->core::Result<runtime::EventLoop*>{return events;};
    if(services.frames)hooks.frames=[frames=services.frames]()->core::Result<runtime::FrameLoop*>{return frames;};
    hooks.activity=services.frame_activity;
    hooks.window_geometry=services.window_geometry;
    hooks.invalidated=[weak=std::weak_ptr(invalidation)]{if(auto value=weak.lock())value->mark();};
    return hooks;
}
}
struct PaintView::Impl final {
    explicit Impl(std::shared_ptr<PaintDevice> device, WidgetAppearance appearance,
                  PaintViewServices services, std::shared_ptr<Invalidation> value)
        : invalidation(std::move(value)),
          state(std::make_shared<detail::WidgetState>([](runtime::Task task) { task(); },
                  std::move(appearance), hooks_for(services,invalidation))),
          painter(std::move(device)), owner(std::this_thread::get_id()) {}
    std::shared_ptr<Invalidation> invalidation;
    std::shared_ptr<detail::WidgetState> state;
    Painter painter;
    std::thread::id owner;
    std::stop_source cancel;
    bool painting = false;
    bool closing = false;
};

PaintView::PaintView(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}
PaintView::~PaintView() {
    // Retire widgets and subscriptions while the paint device is still alive.
    if (impl_) { impl_->invalidation->retire(); impl_->state->close(); }
}

core::Result<std::shared_ptr<PaintView>> PaintView::create(
    std::shared_ptr<PaintDevice> device, WidgetFactory factory, WidgetAppearance appearance) {
    return create(std::move(device),std::move(factory),std::move(appearance),{});
}
core::Result<std::shared_ptr<PaintView>> PaintView::create(
    std::shared_ptr<PaintDevice> device, WidgetFactory factory, WidgetAppearance appearance, PaintViewServices services) {
    if (!device || !factory) return core::fail(core::ErrorCode::invalid_argument, "Paint view requires a device and widget factory");
    if ((services.invalidated || services.frames) && !services.events)
        return core::fail(core::ErrorCode::invalid_argument,"Invalidation and frame services require an event loop");
    if (services.frames && &services.frames->events()!=services.events)
        return core::fail(core::ErrorCode::invalid_argument,"Frame and event services must share a loop");
    try {
        auto signal=std::make_shared<Invalidation>();
        if(services.invalidated){
            signal->sender=services.events->sender();signal->callback=std::move(services.invalidated);
            auto registered=signal->sender.on_signal([weak=std::weak_ptr(signal)]{
                if(auto value=weak.lock();value&&value->active&&value->dirty){auto callback=value->callback;if(callback)callback();}
            });
            if(!registered)return core::fail(registered.error==runtime::SubmitError::full?core::ErrorCode::full:core::ErrorCode::closed,
                                             "Cannot register paint invalidation on the event loop");
            signal->token=registered.token;
        }
        auto view = std::shared_ptr<PaintView>(new PaintView(std::make_unique<Impl>(std::move(device), std::move(appearance),std::move(services),signal)));
        if (auto result = view->impl_->state->construct(std::move(factory)); !result) return std::unexpected(result.error());
        if (auto result = view->impl_->state->activate_paint_view(); !result) return std::unexpected(result.error());
        signal->arm();
        return view;
    } catch (...) {
        return core::fail(core::ErrorCode::failed, "Cannot create paint view", std::current_exception());
    }
}

core::Result<void> PaintView::paint(Size size, double scale) {
    auto keep_alive = shared_from_this();
    if (impl_->owner != std::this_thread::get_id()) return core::fail(core::ErrorCode::wrong_executor, "Paint view requires its owner thread");
    if (impl_->closing) return core::fail(core::ErrorCode::closed, "Paint view is closed");
    if (impl_->painting) return core::fail(core::ErrorCode::busy, "Paint view cannot paint reentrantly");
    impl_->painting = true;
    struct Painting final {
        Impl& impl;
        ~Painting() { impl.painting = false; if (impl.closing) impl.state->close(); }
    } painting{*impl_};
    auto frame = impl_->state->drawing_snapshot(size);
    if (!frame) return std::unexpected(frame.error());
    if (impl_->closing) return core::fail(core::ErrorCode::cancelled, "Paint view closed during drawing");
    if (auto result = impl_->state->begin_external_paint(); !result) return result;
    struct DevicePaint final {
        detail::WidgetState& state;
        ~DevicePaint() { state.end_external_paint(); }
    } device_paint{*impl_->state};
    const auto revision=impl_->invalidation->revision;
    auto result=impl_->painter.paint(*frame, scale, impl_->cancel.get_token());
    if(result && !impl_->closing && impl_->invalidation->revision==revision)impl_->invalidation->dirty=false;
    return result;
}

core::Result<bool> PaintView::needs_paint() const {
    if(impl_->owner!=std::this_thread::get_id())return core::fail(core::ErrorCode::wrong_executor,"Paint view requires its owner thread");
    if(impl_->closing)return core::fail(core::ErrorCode::closed,"Paint view is closed");
    return impl_->invalidation->dirty;
}

core::Result<void> PaintView::input(const WidgetInput& event) {
    auto keep_alive = shared_from_this();
    if (impl_->owner != std::this_thread::get_id()) return core::fail(core::ErrorCode::wrong_executor, "Paint view requires its owner thread");
    if (impl_->closing) return core::fail(core::ErrorCode::closed, "Paint view is closed");
    return impl_->state->input(event);
}

core::Result<Widget> PaintView::find(std::string_view id) const {
    if (impl_->owner != std::this_thread::get_id()) return core::fail(core::ErrorCode::wrong_executor, "Paint view requires its owner thread");
    if (impl_->closing) return core::fail(core::ErrorCode::closed, "Paint view is closed");
    return impl_->state->find(id);
}

core::Result<void> PaintView::close() {
    auto keep_alive = shared_from_this();
    if (impl_->owner != std::this_thread::get_id()) return core::fail(core::ErrorCode::wrong_executor, "Paint view requires its owner thread");
    if (impl_->closing) return {};
    impl_->closing = true;
    impl_->invalidation->retire();
    impl_->cancel.request_stop();
    if (!impl_->painting) impl_->state->close();
    return {};
}
} // namespace ii::gui
