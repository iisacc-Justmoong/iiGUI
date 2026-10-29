#import <AppKit/AppKit.h>
#import <CoreFoundation/CoreFoundation.h>
#import <QuartzCore/QuartzCore.h>
#include <iiGUI/Host.h>
#include <iiSDKCore/Core/Text/Utf8.h>
#include "DispatchState.h"
#include "HostCommon.h"
#include "DesktopCommon.h"
#include "CanvasState.h"
#include "WidgetState.h"
#include <cmath>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <thread>

@interface IIGUIAppCommand : NSObject {
@public std::function<void()> invoke;
}
- (void)trigger:(id)sender;
@end
@implementation IIGUIAppCommand
- (void)trigger:(id)sender { (void)sender;if(invoke)invoke(); }
@end

namespace ii::gui::detail {
struct NativeCallbacks final {
    std::function<void()> resize;
    std::function<bool()> may_close;
    std::function<void()> did_close;
    std::function<void()> request_close;
    std::function<void(double,double)> frame;
    std::function<void(std::vector<std::string>)> files;
};
}

@interface IIGUIHostDelegate : NSObject <NSWindowDelegate, NSApplicationDelegate> {
@public
    ii::gui::detail::NativeCallbacks* callbacks;
}
- (void)requestQuit:(id)sender;
- (void)displayFrame:(CADisplayLink*)sender;
@end

@implementation IIGUIHostDelegate
- (void)application:(NSApplication*)application openFiles:(NSArray<NSString*>*)filenames {
    if(!callbacks||!callbacks->files||filenames.count>256){[application replyToOpenOrPrint:NSApplicationDelegateReplyFailure];return;}
    std::vector<std::string> paths;
    for(NSString* path in filenames){if(path.length>32768||!path.UTF8String){[application replyToOpenOrPrint:NSApplicationDelegateReplyFailure];return;}paths.emplace_back(path.UTF8String);}
    callbacks->files(std::move(paths));[application replyToOpenOrPrint:NSApplicationDelegateReplySuccess];
}
- (void)displayFrame:(CADisplayLink*)sender {
    if(callbacks && callbacks->frame) callbacks->frame(sender.targetTimestamp,sender.targetTimestamp-sender.timestamp);
}
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
    if(options.frame_loop) { if(@available(macOS 14.0,*)) {} else throw std::invalid_argument("Display-synchronized frames require macOS 14+"); }
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
        dispatch(std::make_shared<detail::DispatchState>(options.task_capacity, options.event_loop)),
        gate(std::make_shared<WakeGate>()) {

        state.size = options.size;
        state.title = options.title;
        state.text.clear();
    }

    HostOptions options;
    std::shared_ptr<detail::DispatchState> dispatch;
    std::shared_ptr<PresentationStream> presentations=std::make_shared<PresentationStream>([this](runtime::Task task){return dispatch->loop.post(std::move(task));});
    std::unique_ptr<runtime::FrameLoop> motion_frames;
    std::shared_ptr<MotionPreferences> motion_preferences=std::make_shared<MotionPreferences>();
    std::size_t motion_clients=0;
    std::shared_ptr<WakeGate> gate;
    const std::thread::id owner = std::this_thread::get_id();
    Phase phase = Phase::created;
    HostCallbacks callbacks;
    detail::NativeCallbacks native;
    WindowSnapshot state;
    HostStats statistics;
    detail::CanvasState canvas;
    std::shared_ptr<detail::WidgetState> widgets;
    bool mounting = false;
    std::optional<core::Error> failure;
    bool draining = false;
    bool opened_delivered = false;
    std::vector<std::string> pending_files;
    CFRunLoopSourceRef source = nullptr;
    CFRunLoopTimerRef timer = nullptr;
    NSApplication* application = nil;
    NSWindow* window = nil;
    std::shared_ptr<detail::WidgetState> legacy;
    Widget legacy_text;
    IIGUIHostDelegate* delegate = nil;
    NSMenu* previous_menu = nil;
    NSMutableArray<IIGUIAppCommand*>* menu_commands=nil;
    NSSavePanel* file_panel=nil;
    bool secondary=false;
    Impl* parent=nullptr;
    std::vector<Impl*> children;
    CADisplayLink* display_link = nil;
    std::optional<runtime::TickToken> present_tick;
    std::optional<double> display_origin;
    runtime::Clock::time_point frame_origin{};
    double previous_target=0;

    runtime::FrameLoop* frames() const { return options.frame_loop?options.frame_loop:motion_frames.get(); }
    detail::WidgetFrameHooks frame_hooks() {
        return {
            .frames=[this]()->core::Result<runtime::FrameLoop*> {
                if(@available(macOS 14.0,*)) {} else return core::fail(core::ErrorCode::unsupported,"Native widget motion requires macOS 14+");
                if(!frames())motion_frames=std::make_unique<runtime::FrameLoop>(dispatch->loop);
                if(auto result=reserve_present();!result)return std::unexpected(result.error());
                return frames();
            },
            .activity=[this](bool active) {
                if(active)++motion_clients;else if(motion_clients)--motion_clients;
                if(!draining)guard([&]{refresh_frames();});
            },
            .preferences=motion_preferences,
            .renderer=options.renderer,
            .events=[this]()->core::Result<runtime::EventLoop*>{return &dispatch->loop;},.presentations=presentations
        };
    }
    void refresh_frames() {
        if(phase!=Phase::running || !opened_delivered)return;
        if(options.frame_loop || motion_clients)start_frames();
        else if(display_link)stop_frames();
    }

    core::Result<void> reserve_present() {
        if(present_tick)return {};
        auto present=frames()->add([this](const auto&) {
            if(phase!=Phase::running)return;
            canvas.flush(state.backing_scale,state.size.height);
            if(widgets)widgets->present();
            if(legacy)legacy->present();
        },{.phase=runtime::FramePhase::present,.clock=runtime::TickClock::unscaled,.enabled=false});
        if(!present)return std::unexpected(present.error());
        present_tick=*present;return {};
    }
    void stop_frames(bool retire=false) {
        [display_link invalidate];display_link=nil;
        if(present_tick) {
            if(retire){(void)frames()->remove(*present_tick);present_tick.reset();}
            else (void)frames()->set_enabled(*present_tick,false);
        }
        if(widgets)widgets->set_frame_paced(false);
        if(legacy)legacy->set_frame_paced(false);
        statistics.display_synced=false;
    }
    void start_frames() {
        if(!frames() || display_link) return;
        if(@available(macOS 14.0,*)) {
            if(auto result=reserve_present();!result)throw std::runtime_error("Frame loop requires a presentation callback slot: "+result.error().message);
            if(auto result=frames()->set_enabled(*present_tick,true);!result)throw std::runtime_error(result.error().message);
            if(!options.frame_loop){display_origin.reset();frames()->reset_clock();}
            display_link=[window.contentView displayLinkWithTarget:delegate selector:@selector(displayFrame:)];
            if(!display_link) throw std::runtime_error("Cannot create native display frame source");
            [display_link addToRunLoop:NSRunLoop.mainRunLoop forMode:NSRunLoopCommonModes];
            if(widgets)widgets->set_frame_paced(true);
            if(legacy)legacy->set_frame_paced(true);
            statistics.display_synced=true;
        }
    }
    void display_frame(double target,double interval) noexcept {
        if(phase!=Phase::running || !opened_delivered || draining || !frames()) return;
        guard([&] {
            struct Reset final { bool& v;~Reset(){v=false;} } reset{draining};draining=true;
            if(!std::isfinite(target) || !std::isfinite(interval) || interval<=0) throw std::runtime_error("Invalid native display timestamp");
            if(!display_origin || target<previous_target) {
                const bool rebase=display_origin.has_value();display_origin=target;frame_origin=runtime::Clock::now();
                if(rebase) frames()->reset_clock(frame_origin);
            }
            previous_target=target;
            const auto now=frame_origin+std::chrono::duration_cast<runtime::Clock::duration>(std::chrono::duration<double>(target-*display_origin));
            statistics.display_interval=std::chrono::duration_cast<runtime::Clock::duration>(std::chrono::duration<double>(interval));
            if(!secondary)dispatch->loop.poll(options.budget);
            if(phase!=Phase::running) return;
            ++statistics.display_callbacks;
            (void)frames()->step(now);
            if(phase==Phase::running && dispatch->loop.state()==runtime::LoopState::stopped) { force_close();return; }
            if(phase==Phase::running) {
                refresh_frames();
                arm_timer();
            }
        });
    }

    bool on_owner() const noexcept { return owner == std::this_thread::get_id(); }
    void clear_legacy() { if(legacy) { legacy->close(); legacy.reset(); legacy_text={}; } }
    core::Result<void> show_legacy_text() {
        if(!legacy) {
            auto replacement=std::make_shared<detail::WidgetState>([this](runtime::Task task) { guard([&] { if(phase==Phase::running) task(); }); },WidgetAppearance{},frame_hooks());
            auto root=replacement->add(WidgetKind::vertical_stack,{},"iiGUI.legacy.root",{.padding=28},{},{},{});
            if(!root) return std::unexpected(root.error());
            auto text=replacement->add(WidgetKind::label,*root,"iiGUI.legacy.text",{},{.text=state.text,.font_size=17},{},{});
            if(!text) return std::unexpected(text.error());
            if(auto r=replacement->attach((__bridge void*)window.contentView);!r) return r;
            replacement->set_frame_paced(display_link!=nil || options.frame_loop!=nullptr);
            legacy_text=*text; legacy=std::move(replacement);
        }
        return legacy_text.set_text(state.text);
    }
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
        if(secondary)return;
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
            dispatch->retire();
            if(!secondary)dispatch->loop.request_stop();
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
        auto descendants=children;for(auto* child:descendants)child->force_close();children.clear();
        if(file_panel){[file_panel cancel:nil];file_panel=nil;}
        stop_frames(true);
        dispatch->retire();
        if(!secondary)dispatch->loop.request_stop();
        canvas.clear();
        clear_legacy();
        if (widgets) { widgets->close(); widgets.reset(); }
        [window orderOut:nil];
        update_snapshot();
        state.visible = false;
        state.closed = true;presentations->close();
        phase = Phase::closed;
        if(secondary){
            auto callback=std::move(callbacks.closed);auto endpoint=parent?parent->dispatch:std::shared_ptr<detail::DispatchState>{};cleanup();
            if(callback&&endpoint&&endpoint->accepting.load()){auto sender=endpoint->loop.sender();(void)endpoint->track(endpoint->tasks.post(sender,std::move(callback)));}
            return;
        }
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
        if (phase != Phase::running || secondary) return;
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
            if(phase==Phase::running&&!pending_files.empty()){
                auto paths=std::exchange(pending_files,{});
                if(callbacks.files_opened)callbacks.files_opened(std::move(paths));
            }
            if (phase == Phase::running&&!secondary) dispatch->loop.poll(options.budget);
            if (phase == Phase::running && dispatch->loop.state()==runtime::LoopState::stopped) force_close();
            if (phase == Phase::running) {
                refresh_frames();
                if(!display_link) canvas.flush(state.backing_scale, state.size.height);
            }
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
        native.frame = [this](double target,double interval) { display_frame(target,interval); };
        native.files = [this](std::vector<std::string> paths){guard([&]{if(phase==Phase::running&&pending_files.size()+paths.size()<=256){for(auto& path:paths)pending_files.push_back(std::move(path));gate->signal();}});};
        delegate = [IIGUIHostDelegate new];
        delegate->callbacks = &native;
        if(!secondary){
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
        }

        window = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, options.size.width, options.size.height)
            styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                      NSWindowStyleMaskResizable | NSWindowStyleMaskMiniaturizable
            backing:NSBackingStoreBuffered defer:NO];
        window.releasedWhenClosed = NO;
        window.title = native_text(state.title);
        window.contentMinSize = NSMakeSize(options.minimum_size.width, options.minimum_size.height);
        window.delegate = delegate;
        [window center];

        CFRunLoopSourceContext context{};
        context.info = this;
        context.perform = [](void* info) { static_cast<Impl*>(info)->drain(); };
        source = CFRunLoopSourceCreate(kCFAllocatorDefault, 0, &context);
        if (!source) throw std::runtime_error("Cannot create native UI wake source");
        CFRunLoopAddSource(CFRunLoopGetMain(), source, kCFRunLoopCommonModes);
        gate->attach(source);
        if(!secondary)dispatch->connect([wake=gate]() noexcept { wake->signal(); });
        phase = Phase::running;
        [window makeKeyAndOrderFront:nil];
        [application activateIgnoringOtherApps:YES];
        update_snapshot();
        gate->signal();
    }
    void cleanup() {
        auto descendants=children;for(auto* child:descendants)child->force_close();children.clear();
        if(file_panel){[file_panel cancel:nil];file_panel=nil;}
        stop_frames(true);
        dispatch->retire();
        if(!secondary)dispatch->loop.request_stop();
        if (auto detached=dispatch->disconnect(); !detached)
            remember_failure(detached.error().message,detached.error().cause);
        gate->detach();
        discard_timer();
        canvas.clear();
        if (widgets) { widgets->close(); widgets.reset(); }
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
        if(!secondary)application.mainMenu = previous_menu;
        menu_commands=nil;
        if(parent){std::erase(parent->children,this);parent=nullptr;}
        if (delegate) delegate->callbacks = nullptr;
        callbacks = {};
        native = {};
        clear_legacy();
        window = nil;
        delegate = nil;
        previous_menu = nil;
        application = nil;
        state.visible = false;
        state.closed = true;presentations->close();
        phase = Phase::closed;
    }
};

core::Result<std::shared_ptr<PresentationSource>> Host::presentation_source() const {
    if(!is_ui_thread())return core::fail(core::ErrorCode::wrong_executor,"Presentation source requires host owner");
    return std::static_pointer_cast<PresentationSource>(impl_->presentations);
}
Host::Host(HostOptions options) : impl_(std::make_unique<Impl>(std::move(options))) {}
Host::~Host() {
    if (!is_ui_thread() || impl_->phase == Impl::Phase::running || impl_->phase == Impl::Phase::closing)
        std::terminate();
    impl_->presentations->close();impl_->dispatch->retire();
    if (!impl_->secondary&&(impl_->phase != Impl::Phase::created || impl_->dispatch->owned_loop)) impl_->dispatch->loop.request_stop();
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
core::Result<void> Host::show(Host& parent,HostCallbacks callbacks){
    if(!is_ui_thread()||!parent.is_ui_thread())return core::fail(core::ErrorCode::wrong_executor,"Secondary windows require the owning main thread");
    if(this==&parent||impl_->phase!=Impl::Phase::created)return core::fail(core::ErrorCode::busy,"Secondary windows are single use");
    if(auto ready=parent.impl_->ready();!ready)return ready;
    if(impl_->options.event_loop&&impl_->options.event_loop!=&parent.impl_->dispatch->loop)return core::fail(core::ErrorCode::invalid_argument,"Secondary windows must share their parent's EventLoop");
    if(impl_->options.frame_loop)return core::fail(core::ErrorCode::invalid_argument,"A secondary window manages its own display cadence");
    impl_->dispatch->retire();impl_->dispatch=std::make_shared<detail::DispatchState>(impl_->options.task_capacity,&parent.impl_->dispatch->loop);
    impl_->options.event_loop=&parent.impl_->dispatch->loop;impl_->secondary=true;impl_->parent=parent.impl_.get();
    impl_->application=parent.impl_->application;impl_->callbacks=std::move(callbacks);parent.impl_->children.push_back(impl_.get());
    try{impl_->open_native();}catch(...){impl_->cleanup();return core::fail(core::ErrorCode::failed,"Cannot open secondary window",std::current_exception());}
    return {};
}
core::Result<void> Host::choose_file(FileDialogOptions options,FileDialogCompletion completion){
    if(auto ready=impl_->ready();!ready)return ready;
    if(!completion||!detail::valid_dialog(options))return core::fail(core::ErrorCode::invalid_argument,"Invalid file dialog options");
    if(impl_->file_panel&&impl_->file_panel.sheetParent)return core::fail(core::ErrorCode::busy,"A file dialog is already open");
    NSSavePanel* panel=options.mode==FileDialogMode::save?NSSavePanel.savePanel:NSOpenPanel.openPanel;
    if(options.mode!=FileDialogMode::save){auto open=(NSOpenPanel*)panel;open.canChooseFiles=options.mode==FileDialogMode::open;open.canChooseDirectories=options.mode==FileDialogMode::directory;open.allowsMultipleSelection=options.multiple;}
    panel.title=native_text(options.title);panel.nameFieldStringValue=native_text(options.filename);panel.canCreateDirectories=YES;
    if(!options.initial_directory.empty())panel.directoryURL=[NSURL fileURLWithPath:native_text(options.initial_directory)];
    NSMutableArray<NSString*>* types=[NSMutableArray new];for(const auto& filter:options.filters)for(const auto& ext:filter.extensions)[types addObject:native_text(ext)];
    if(types.count)panel.allowedFileTypes=types;
    impl_->file_panel=panel;auto endpoint=std::weak_ptr(impl_->dispatch);auto owner=impl_.get();auto ui=executor();
    [panel beginSheetModalForWindow:impl_->window completionHandler:^(NSModalResponse response){
        auto active=endpoint.lock();if(!active||!active->accepting.load())return;
        owner->file_panel=nil;FileDialogSelection result;result.canceled=response!=NSModalResponseOK;
        if(!result.canceled){if([panel isKindOfClass:NSOpenPanel.class])for(NSURL* url in ((NSOpenPanel*)panel).URLs)result.paths.emplace_back(url.path.UTF8String);else if(panel.URL)result.paths.emplace_back(panel.URL.path.UTF8String);}
        (void)ui.post([completion,result=std::move(result)]()mutable{completion(std::move(result));});
    }];return {};
}
core::Result<void> Host::window_action(WindowAction action){
    if(auto ready=impl_->ready();!ready)return ready;
    switch(action){
    case WindowAction::restore:[impl_->window deminiaturize:nil];if(impl_->window.zoomed)[impl_->window zoom:nil];[impl_->window makeKeyAndOrderFront:nil];break;
    case WindowAction::minimize:[impl_->window miniaturize:nil];break;
    case WindowAction::maximize:if(!impl_->window.zoomed)[impl_->window zoom:nil];break;
    case WindowAction::toggle_fullscreen:[impl_->window toggleFullScreen:nil];break;
    case WindowAction::raise:[impl_->window makeKeyAndOrderFront:nil];break;
    default:return core::fail(core::ErrorCode::invalid_argument,"Invalid window action");}
    return {};
}
core::Result<void> Host::set_app_menu(std::vector<AppMenuItem> items){
    if(auto ready=impl_->ready();!ready)return ready;
    if(impl_->secondary)return core::fail(core::ErrorCode::invalid_argument,"The primary Host owns the application menu");
    if(!detail::valid_menu(items))return core::fail(core::ErrorCode::invalid_argument,"Invalid application menu");
    auto ui=executor();NSMutableArray<IIGUIAppCommand*>* commands=[NSMutableArray new];
    const auto build=[&](auto&& self,const std::vector<AppMenuItem>& rows)->NSMenu*{
        NSMenu* menu=[NSMenu new];menu.autoenablesItems=NO;
        for(const auto& row:rows){if(row.separator){[menu addItem:NSMenuItem.separatorItem];continue;}
            auto item=[[NSMenuItem alloc] initWithTitle:native_text(row.title) action:nil keyEquivalent:row.shortcut?native_text(row.shortcut->key):@""];
            item.identifier=native_text(row.id);item.enabled=row.enabled;item.state=row.checked?NSControlStateValueOn:NSControlStateValueOff;
            if(row.shortcut){auto key=*row.shortcut;item.keyEquivalentModifierMask=(key.meta?NSEventModifierFlagCommand:0)|(key.control?NSEventModifierFlagControl:0)|(key.alt?NSEventModifierFlagOption:0)|(key.shift?NSEventModifierFlagShift:0);}
            if(!row.children.empty())item.submenu=self(self,row.children);
            else if(row.triggered){auto target=[IIGUIAppCommand new];auto callback=row.triggered;target->invoke=[ui,callback]{(void)ui.post(callback);};item.target=target;item.action=@selector(trigger:);[commands addObject:target];}
            [menu addItem:item];
        }return menu;
    };
    NSMenu* menu=build(build,items);auto app=impl_->application.mainMenu.itemArray.firstObject;
    if(app){[impl_->application.mainMenu removeItem:app];[menu insertItem:app atIndex:0];}
    impl_->application.mainMenu=menu;impl_->menu_commands=commands;return {};
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
    if (impl_->widgets) return core::fail(core::ErrorCode::busy, "Use the mounted widget's text property");
    if (!detail::valid_title(text) || text.size()>65536) return core::fail(core::ErrorCode::invalid_argument, "Display text must be valid UTF-8 without NUL, up to 64 KiB");
    std::string value(text);
    if (value == impl_->state.text && impl_->legacy) return {};
    auto previous=std::exchange(impl_->state.text,std::move(value));
    if (auto result=impl_->show_legacy_text(); !result) { impl_->state.text=std::move(previous); return result; }
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
core::Result<void> Host::set_window_geometry(const WindowGeometry& update) {
    if(auto ready=impl_->ready();!ready)return ready;
    impl_->update_snapshot();
    const WindowGeometryValue previous{impl_->state.title,impl_->state.size,impl_->options.minimum_size};
    auto next=resolve_window_geometry(previous,update);if(!next)return std::unexpected(next.error());
    @try {
        impl_->options.minimum_size=next->minimum_size;
        impl_->window.contentMinSize=NSMakeSize(next->minimum_size.width,next->minimum_size.height);
        if(next->title!=previous.title)impl_->window.title=native_text(next->title);
        if(next->size!=previous.size)[impl_->window setContentSize:NSMakeSize(next->size.width,next->size.height)];
        impl_->state.title=next->title;impl_->update_snapshot();return {};
    } @catch(NSException* error) {
        impl_->options.minimum_size=previous.minimum_size;
        impl_->window.contentMinSize=NSMakeSize(previous.minimum_size.width,previous.minimum_size.height);
        impl_->window.title=native_text(previous.title);
        [impl_->window setContentSize:NSMakeSize(previous.size.width,previous.size.height)];
        return core::fail(core::ErrorCode::failed,native_error(error));
    }
}
core::Result<void> Host::request_close() {
    if (auto ready = impl_->ready(); !ready) return ready;
    [impl_->window performClose:nil];
    return {};
}
core::Result<void> Host::set_canvas(CanvasFrame frame) {
    if(frame.renderer==Renderer::automatic)frame.renderer=impl_->options.renderer;
    if (auto ready = impl_->ready(); !ready) return ready;
    if (impl_->mounting) return core::fail(core::ErrorCode::busy, "A widget factory is active");
    auto result = impl_->canvas.set(std::move(frame), (__bridge void*)impl_->window.contentView,
                                   nullptr, impl_->state.backing_scale,impl_->presentations);
    if (result) {
        if (impl_->widgets) { impl_->widgets->close(); impl_->widgets.reset(); }
        impl_->clear_legacy();
        impl_->gate->signal();
    }
    return result;
}
core::Result<WidgetMount> Host::mount_widgets(WidgetFactory factory) {
    return mount_widgets(std::move(factory), WidgetAppearance{});
}
core::Result<WidgetMount> Host::mount_widgets(WidgetFactory factory, const WidgetAppearance& appearance) {
    if (auto ready = impl_->ready(); !ready) return std::unexpected(ready.error());
    if (!factory) return core::fail(core::ErrorCode::invalid_argument, "Native widget factory is empty");
    if (impl_->mounting) return core::fail(core::ErrorCode::busy, "A widget factory is already active");
    struct Reset final { bool& value; ~Reset() { value = false; } } reset{impl_->mounting};
    impl_->mounting = true;
    @try {
            auto hooks=impl_->frame_hooks();hooks.window_geometry=[this](const WindowGeometry& value){return set_window_geometry(value);};
        try {
            auto replacement = std::make_shared<detail::WidgetState>([host=impl_.get()](runtime::Task work) {
                host->guard([&] { if (host->phase == Impl::Phase::running) work(); });
            }, appearance,std::move(hooks));
            if (auto built = replacement->construct(std::move(factory)); !built) return std::unexpected(built.error());
            if (auto ready = impl_->ready(); !ready) return std::unexpected(ready.error());
            if (auto attached = replacement->attach((__bridge void*)impl_->window.contentView); !attached)
                return std::unexpected(attached.error());
            if(auto committed=replacement->commit_window_properties();!committed)return std::unexpected(committed.error());
            impl_->canvas.clear();
            if (impl_->widgets) impl_->widgets->close();
            impl_->widgets = std::move(replacement);
            impl_->clear_legacy();
            impl_->widgets->set_frame_paced(impl_->display_link!=nil || impl_->options.frame_loop!=nullptr);
            auto published=impl_->widgets;published->commit_publication();
            if(auto alive=published->ready();!alive)return std::unexpected(alive.error());
            return WidgetMount(published);
        } catch (...) {
            return core::fail(core::ErrorCode::failed, "Native widget factory failed", std::current_exception());
        }
    } @catch (NSException* error) {
        return core::fail(core::ErrorCode::failed, native_error(error));
    }
}
core::Result<void> Host::unmount_widgets() {
    if (auto ready = impl_->ready(); !ready) return ready;
    if (impl_->mounting) return core::fail(core::ErrorCode::busy, "A widget factory is active");
    if (impl_->widgets) { impl_->widgets->close(); impl_->widgets.reset(); }
    return {};
}
core::Result<void> Host::unmount_widgets(const WidgetMount& expected) {
    if (auto ready=impl_->ready(); !ready) return ready;
    auto owner=expected.state_.lock();
    if (!owner) return core::fail(core::ErrorCode::closed,"Native widget generation is closed");
    if (owner!=impl_->widgets) return core::fail(core::ErrorCode::invalid_argument,"Native widget belongs to another host or generation");
    return unmount_widgets();
}
core::Result<void> Host::clear_canvas() {
    if (auto ready = impl_->ready(); !ready) return ready;
    impl_->canvas.clear();
    return {};
}
core::Result<GraphicsInfo> Host::graphics_info() const {
    if (auto ready = impl_->ready(); !ready) return std::unexpected(ready.error());
    if (impl_->widgets) return impl_->widgets->graphics_info();
    if (impl_->legacy) return impl_->legacy->graphics_info();
    return impl_->canvas.info();
}
core::Result<PixelBuffer> Host::capture_canvas() {
    if (auto ready = impl_->ready(); !ready) return std::unexpected(ready.error());
    if (impl_->widgets) return impl_->widgets->capture();
    if (impl_->legacy) return impl_->legacy->capture();
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
