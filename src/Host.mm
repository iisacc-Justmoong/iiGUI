#import <AppKit/AppKit.h>
#import <CoreFoundation/CoreFoundation.h>
#include <iiGUI/Host.h>
#include <iiSDKCore/Core/Text/Utf8.h>
#include "DispatchState.h"
#include "HostCommon.h"
#include "CanvasState.h"
#include <cmath>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <thread>

namespace ii::gui::detail {
struct NativeCallbacks final {
    std::function<void()> resize;
    std::function<bool()> may_close;
    std::function<void()> did_close;
    std::function<void()> request_close;
};
}

@interface IIGUIHostDelegate : NSObject <NSWindowDelegate, NSApplicationDelegate> {
@public
    ii::gui::detail::NativeCallbacks* callbacks;
}
- (void)requestQuit:(id)sender;
@end

@implementation IIGUIHostDelegate
- (void)windowDidResize:(NSNotification*)notification {
    (void)notification;
    if (callbacks && callbacks->resize) callbacks->resize();
}
- (void)windowDidChangeBackingProperties:(NSNotification*)notification {
    (void)notification;
    if (callbacks && callbacks->resize) callbacks->resize();
}
- (BOOL)windowShouldClose:(NSWindow*)sender {
    (void)sender;
    return !callbacks || !callbacks->may_close || callbacks->may_close();
}
- (void)windowWillClose:(NSNotification*)notification {
    (void)notification;
    if (callbacks && callbacks->did_close) callbacks->did_close();
}
- (void)requestQuit:(id)sender {
    (void)sender;
    if (callbacks && callbacks->request_close) callbacks->request_close();
}
- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication*)sender {
    (void)sender;
    [self requestQuit:nil];
    // Return through Host::run so C++ owners and Runtime workers can shut down.
    return NSTerminateCancel;
}
@end

namespace ii::gui {
namespace {
bool valid_size(Size size) {
    return detail::valid_size(size);
}
HostOptions validate(HostOptions options) {
    if (![NSThread isMainThread])
        throw std::invalid_argument("iiGUI Host must be constructed on the process main thread");
    return detail::validate_options(std::move(options));
}
NSString* native_text(std::string_view text) {
    return [[NSString alloc] initWithBytes:text.data() length:text.size()
                                 encoding:NSUTF8StringEncoding];
}
std::string native_error(NSException* error) {
    const char* reason = error.reason.UTF8String;
    return reason ? reason : "AppKit exception";
}

// Workers may signal while the UI removes its source. The gate holds retained CF
// references and detaches under the same mutex, without retaining the Host.
struct WakeGate final {
    std::mutex mutex;
    CFRunLoopSourceRef source = nullptr;
    CFRunLoopRef loop = nullptr;
    void attach(CFRunLoopSourceRef value) {
        std::lock_guard lock(mutex);
        source = value;
        CFRetain(source);
        loop = CFRunLoopGetMain();
        CFRetain(loop);
    }
    void signal() {
        std::lock_guard lock(mutex);
        if (source) {
            CFRunLoopSourceSignal(source);
            CFRunLoopWakeUp(loop);
        }
    }
    void detach() {
        std::lock_guard lock(mutex);
        if (source) { CFRelease(source); source = nullptr; }
        if (loop) { CFRelease(loop); loop = nullptr; }
    }
    ~WakeGate() { detach(); }
};
}

struct Host::Impl {
    enum class Phase { created, running, closing, closed };
    explicit Impl(HostOptions value) : options(validate(std::move(value))),
        dispatch(std::make_shared<detail::DispatchState>(options.task_capacity)),
        gate(std::make_shared<WakeGate>()) {

        state.size = options.size;
        state.title = options.title;
        state.text = "HelloGUI\n\nC++23 native macOS host\niiSDKCore + iiSDKBuilder\nQt-free AppKit window";
    }

    HostOptions options;
    std::shared_ptr<detail::DispatchState> dispatch;
    std::shared_ptr<WakeGate> gate;
    const std::thread::id owner = std::this_thread::get_id();
    Phase phase = Phase::created;
    HostCallbacks callbacks;
    detail::NativeCallbacks native;
    WindowSnapshot state;
    HostStats statistics;
    detail::CanvasState canvas;
    std::optional<core::Error> failure;
    bool draining = false;
    bool opened_delivered = false;
    CFRunLoopSourceRef source = nullptr;
    CFRunLoopTimerRef timer = nullptr;
    NSApplication* application = nil;
    NSWindow* window = nil;
    NSTextField* label = nil;
    IIGUIHostDelegate* delegate = nil;
    NSMenu* previous_menu = nil;

    bool on_owner() const noexcept { return owner == std::this_thread::get_id(); }
    core::Result<void> ready() const {
        if (!on_owner()) return core::fail(core::ErrorCode::wrong_executor, "GUI access requires the owning main thread");
        if (phase != Phase::running) return core::fail(core::ErrorCode::closed, "GUI window is not open");
        return {};
    }
    void remember_failure(std::string message, std::exception_ptr cause = {}) {
        if (!failure) failure = core::Error{core::ErrorCode::failed, std::move(message), cause};
    }
    void update_snapshot() {
        if (!window) return;
        auto size = window.contentView.bounds.size;
        state.size = {size.width, size.height};
        state.backing_scale = window.backingScaleFactor;
        state.visible = window.visible;
    }
    void stop_application() {
        [application stop:nil];
        // stop: is observed by NSApplication after nextEvent returns.
        NSEvent* wake = [NSEvent otherEventWithType:NSEventTypeApplicationDefined
            location:NSZeroPoint modifierFlags:0 timestamp:0 windowNumber:0
            context:nil subtype:0 data1:0 data2:0];
        [application postEvent:wake atStart:YES];
        CFRunLoopWakeUp(CFRunLoopGetMain());
    }
    void force_close() noexcept {
        @try {
            if (phase == Phase::running) [window close];
            else if (phase == Phase::closed) stop_application();
        } @catch (NSException* error) {
            remember_failure(native_error(error));
            phase = Phase::closed;
            dispatch->accepting.store(false, std::memory_order_release);
            dispatch->loop.request_stop();
            stop_application();
        }
    }
    template<class Work> void guard(Work&& work) noexcept {
        @try {
            try { work(); }
            catch (...) {
                remember_failure("iiGUI callback failed", std::current_exception());
                force_close();
            }
        } @catch (NSException* error) {
            remember_failure(native_error(error));
            force_close();
        }
    }
    void did_close() noexcept {
        if (phase == Phase::closed || phase == Phase::closing) return;
        phase = Phase::closing;
        dispatch->accepting.store(false, std::memory_order_release);
        dispatch->loop.request_stop();
        canvas.clear();
        [window orderOut:nil];
        update_snapshot();
        state.visible = false;
        state.closed = true;
        phase = Phase::closed;
        guard([&] { if (callbacks.closed) callbacks.closed(); });
        stop_application();
    }
    bool may_close() noexcept {
        if (phase != Phase::running) return true;
        bool accepted = true;
        @try {
            try { accepted = !callbacks.close_requested || callbacks.close_requested(); }
            catch (...) { remember_failure("iiGUI close callback failed", std::current_exception()); }
        } @catch (NSException* error) { remember_failure(native_error(error)); }
        return accepted;
    }
    void discard_timer() {
        if (timer) { CFRunLoopTimerInvalidate(timer); CFRelease(timer); timer = nullptr; }
    }
    void arm_timer() {
        discard_timer();
        if (phase != Phase::running) return;
        const auto next = dispatch->loop.next_deadline();
        if (!next) return;
        const auto remaining = *next - runtime::Clock::now();
        if (remaining <= runtime::Clock::duration::zero()) {
            gate->signal();
            return;
        }
        CFRunLoopTimerContext context{};
        context.info = this;
        const double seconds = std::chrono::duration<double>(remaining).count();
        timer = CFRunLoopTimerCreate(kCFAllocatorDefault, CFAbsoluteTimeGetCurrent()+seconds,
            0, 0, 0, [](CFRunLoopTimerRef fired, void* info) {
                auto& host = *static_cast<Impl*>(info);
                ++host.statistics.timer_wakes;
                if (host.timer == fired) { host.timer = nullptr; CFRelease(fired); }
                host.drain();
            }, &context);
        if (!timer) throw std::runtime_error("Cannot create GUI deadline timer");
        CFRunLoopAddTimer(CFRunLoopGetMain(), timer, kCFRunLoopCommonModes);
    }
    void drain() noexcept {
        if (phase != Phase::running || draining) return;
        guard([&] {
            struct Reset final { bool& value; ~Reset() { value = false; } } reset{draining};
            draining = true;
            ++statistics.dispatches;
            if (!opened_delivered) {
                opened_delivered = true;
                if (callbacks.opened) callbacks.opened();
            }
            if (phase == Phase::running) dispatch->loop.poll(options.budget);
            if (phase == Phase::running && dispatch->loop.state()==runtime::LoopState::stopped) force_close();
            if (phase == Phase::running) canvas.flush(state.backing_scale, state.size.height);
            arm_timer();
        });
    }
    void open_native() {
        native.resize = [this] {
            if (phase != Phase::running) return;
            guard([&] {
                update_snapshot();
                canvas.dirty = true;
                gate->signal();
                if (opened_delivered && callbacks.resized) callbacks.resized(state.size);
            });
        };
        native.may_close = [this] { return may_close(); };
        native.did_close = [this] { guard([&] { did_close(); }); };
        native.request_close = [this] { guard([&] { if (phase == Phase::running) [window performClose:nil]; }); };
        delegate = [IIGUIHostDelegate new];
        delegate->callbacks = &native;
        application.delegate = delegate;
        [application setActivationPolicy:NSApplicationActivationPolicyRegular];

        previous_menu = application.mainMenu;
        NSMenu* menu = [NSMenu new];
        NSMenuItem* root = [NSMenuItem new];
        NSMenu* app_menu = [NSMenu new];
        NSString* app_name = [NSBundle.mainBundle objectForInfoDictionaryKey:@"CFBundleName"];
        if (!app_name.length) app_name = NSProcessInfo.processInfo.processName;
        NSMenuItem* quit = [[NSMenuItem alloc] initWithTitle:[@"Quit " stringByAppendingString:app_name]
            action:@selector(requestQuit:) keyEquivalent:@"q"];
        quit.target = delegate;
        [app_menu addItem:quit];
        root.submenu = app_menu;
        [menu addItem:root];
        application.mainMenu = menu;

        window = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, options.size.width, options.size.height)
            styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                      NSWindowStyleMaskResizable | NSWindowStyleMaskMiniaturizable
            backing:NSBackingStoreBuffered defer:NO];
        window.releasedWhenClosed = NO;
        window.title = native_text(state.title);
        window.contentMinSize = NSMakeSize(options.minimum_size.width, options.minimum_size.height);
        window.delegate = delegate;
        [window center];
        NSView* content = window.contentView;
        label = [NSTextField wrappingLabelWithString:native_text(state.text)];
        label.font = [NSFont monospacedSystemFontOfSize:17 weight:NSFontWeightRegular];
        label.textColor = NSColor.labelColor;
        label.selectable = YES;
        label.translatesAutoresizingMaskIntoConstraints = NO;
        [content addSubview:label];
        [NSLayoutConstraint activateConstraints:@[
            [label.leadingAnchor constraintEqualToAnchor:content.leadingAnchor constant:28],
            [label.trailingAnchor constraintEqualToAnchor:content.trailingAnchor constant:-28],
            [label.topAnchor constraintEqualToAnchor:content.topAnchor constant:28],
            [label.bottomAnchor constraintLessThanOrEqualToAnchor:content.bottomAnchor constant:-28]
        ]];

        CFRunLoopSourceContext context{};
        context.info = this;
        context.perform = [](void* info) { static_cast<Impl*>(info)->drain(); };
        source = CFRunLoopSourceCreate(kCFAllocatorDefault, 0, &context);
        if (!source) throw std::runtime_error("Cannot create native UI wake source");
        CFRunLoopAddSource(CFRunLoopGetMain(), source, kCFRunLoopCommonModes);
        gate->attach(source);
        dispatch->connect([wake=gate]() noexcept { wake->signal(); });
        phase = Phase::running;
        [window makeKeyAndOrderFront:nil];
        [application activateIgnoringOtherApps:YES];
        update_snapshot();
        gate->signal();
    }
    void cleanup() {
        dispatch->accepting.store(false, std::memory_order_release);
        dispatch->loop.request_stop();
        if (auto detached=dispatch->disconnect(); !detached)
            remember_failure(detached.error().message,detached.error().cause);
        gate->detach();
        discard_timer();
        canvas.clear();
        if (source) {
            CFRunLoopSourceInvalidate(source);
            CFRelease(source);
            source = nullptr;
        }
        if (window) {
            window.delegate = nil;
            [window orderOut:nil];
            [window close];
        }
        if (application.delegate == delegate) application.delegate = nil;
        application.mainMenu = previous_menu;
        if (delegate) delegate->callbacks = nullptr;
        callbacks = {};
        native = {};
        label = nil;
        window = nil;
        delegate = nil;
        previous_menu = nil;
        application = nil;
        state.visible = false;
        state.closed = true;
        phase = Phase::closed;
    }
};

Host::Host(HostOptions options) : impl_(std::make_unique<Impl>(std::move(options))) {}
Host::~Host() {
    if (!is_ui_thread() || impl_->phase == Impl::Phase::running || impl_->phase == Impl::Phase::closing)
        std::terminate();
    impl_->dispatch->accepting.store(false, std::memory_order_release);
    impl_->dispatch->loop.request_stop();
    impl_->gate->detach();
}
UiExecutor Host::executor() const noexcept { return UiExecutor(impl_->dispatch); }
bool Host::is_ui_thread() const noexcept { return impl_->on_owner(); }
core::Result<void> Host::run(HostCallbacks callbacks) {
    if (!is_ui_thread()) return core::fail(core::ErrorCode::wrong_executor, "Host::run requires the main thread");
    if (impl_->phase != Impl::Phase::created)
        return core::fail(impl_->phase == Impl::Phase::running ? core::ErrorCode::busy : core::ErrorCode::closed,
                          "Host::run is single use and cannot reenter");
    @autoreleasepool {
        NSApplication* application = NSApplication.sharedApplication;
        if (application.running || application.delegate)
            return core::fail(core::ErrorCode::busy, "iiGUI requires ownership of the standalone application host");
        impl_->application = application;
        impl_->callbacks = std::move(callbacks);
        @try {
            try {
                impl_->open_native();
                [application run];
                if (impl_->phase == Impl::Phase::running) {
                    impl_->remember_failure("Native application loop stopped before its window closed");
                    impl_->force_close();
                }
            } catch (...) {
                impl_->remember_failure("Cannot run native GUI host", std::current_exception());
                impl_->force_close();
            }
        } @catch (NSException* error) {
            impl_->remember_failure(native_error(error));
            impl_->force_close();
        }
        impl_->cleanup();
    }
    if (impl_->failure) return std::unexpected(*impl_->failure);
    return {};
}
core::Result<void> Host::set_title(std::string_view title) {
    if (auto ready = impl_->ready(); !ready) return ready;
    if (!detail::valid_title(title)) return core::fail(core::ErrorCode::invalid_argument, "Title must be UTF-8 without embedded NUL");
    std::string value(title);
    impl_->window.title = native_text(value);
    impl_->state.title = std::move(value);
    return {};
}
core::Result<void> Host::set_text(std::string_view text) {
    if (auto ready = impl_->ready(); !ready) return ready;
    if (!core::valid_utf8(text)) return core::fail(core::ErrorCode::invalid_argument, "Display text must be valid UTF-8");
    std::string value(text);
    if (value == impl_->state.text) return {};
    impl_->label.stringValue = native_text(value);
    impl_->state.text = std::move(value);
    return {};
}
core::Result<void> Host::set_content_size(Size size) {
    if (auto ready = impl_->ready(); !ready) return ready;
    if (!valid_size(size) || size.width < impl_->options.minimum_size.width ||
        size.height < impl_->options.minimum_size.height)
        return core::fail(core::ErrorCode::invalid_argument, "Content size must be finite and at least the window minimum");
    [impl_->window setContentSize:NSMakeSize(size.width, size.height)];
    impl_->update_snapshot();
    return {};
}
core::Result<void> Host::request_close() {
    if (auto ready = impl_->ready(); !ready) return ready;
    [impl_->window performClose:nil];
    return {};
}
core::Result<void> Host::set_canvas(CanvasFrame frame) {
    if (auto ready = impl_->ready(); !ready) return ready;
    auto result = impl_->canvas.set(std::move(frame), (__bridge void*)impl_->window.contentView,
                                   nullptr, impl_->state.backing_scale);
    if (result) impl_->gate->signal();
    return result;
}
core::Result<void> Host::clear_canvas() {
    if (auto ready = impl_->ready(); !ready) return ready;
    impl_->canvas.clear();
    return {};
}
core::Result<GraphicsInfo> Host::graphics_info() const {
    if (auto ready = impl_->ready(); !ready) return std::unexpected(ready.error());
    return impl_->canvas.info();
}
core::Result<PixelBuffer> Host::capture_canvas() {
    if (auto ready = impl_->ready(); !ready) return std::unexpected(ready.error());
    return impl_->canvas.capture(impl_->state.backing_scale, impl_->state.size.height);
}
core::Result<std::reference_wrapper<runtime::EventLoop>> Host::event_loop() {
    if (auto ready=impl_->ready(); !ready) return std::unexpected(ready.error());
    return std::ref(impl_->dispatch->loop);
}
core::Result<WindowSnapshot> Host::snapshot() const {
    if (!is_ui_thread()) return core::fail(core::ErrorCode::wrong_executor, "Snapshot requires the main thread");
    if (impl_->phase == Impl::Phase::created) return core::fail(core::ErrorCode::closed, "Window has not opened");
    impl_->update_snapshot();
    return impl_->state;
}
core::Result<HostStats> Host::stats() const {
    if (!is_ui_thread()) return core::fail(core::ErrorCode::wrong_executor, "Stats require the main thread");
    auto result = impl_->statistics;
    result.loop = impl_->dispatch->loop.stats();
    result.host = impl_->dispatch->host_stats();
    return result;
}
} // namespace ii::gui
