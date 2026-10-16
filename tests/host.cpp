#include <iiGUI/iiGUI.h>
#include <iiSDKCore/Runtime/Runtime.h>
#include <iiSDKCore/Runtime/Operation.h>
#include <iiSDKCore/Runtime/FrameLoop.h>
#include <iiSDKCore/Process/Process.h>
#include <atomic>
#include <array>
#include <chrono>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <thread>

using namespace std::chrono_literals;
using namespace ii;
namespace {
std::atomic<int> assertions{0};
void check(bool value, const char* message) {
    ++assertions;
    if (!value) throw std::runtime_error(message);
}
template<class T> void good(const T& value, const char* message) {
    if constexpr (requires { value.error().message; }) {
        if (!value) {
            if (value.error().cause) std::rethrow_exception(value.error().cause);
            throw std::runtime_error(std::string(message) + ": " + value.error().message);
        }
    }
    check(bool(value), message);
}
void contracts() {
    static_assert(runtime::TaskExecutor<gui::UiExecutor>);
    static_assert(runtime::TimedTaskExecutor<gui::UiExecutor>);
    gui::UiExecutor empty;
    check(empty.post([] {}).error == runtime::SubmitError::stopped, "empty endpoint is closed");
    auto invalid = [](gui::HostOptions options) {
        try { gui::Host host(options); return false; }
        catch (const std::invalid_argument&) { return true; }
    };
    gui::HostOptions options;
    options.size.width = std::numeric_limits<double>::quiet_NaN();
    check(invalid(options), "reject NaN size");
    options = {}; options.size.width = 1e30;
    check(invalid(options), "reject size outside native coordinate range");
    options = {}; options.minimum_size = {1000, 1000};
    check(invalid(options), "reject initial size below minimum");
    options = {}; options.title = "\xC0\x80";
    check(invalid(options), "reject invalid UTF-8 title");
    options = {}; options.title = std::string("abc\0def", 7);
    check(invalid(options), "reject embedded NUL in native title");
    options = {}; options.budget.max_callbacks = 0;
    check(invalid(options), "reject zero callback budget");
    options = {}; options.task_capacity = 0;
    check(invalid(options), "reject zero queue capacity");
    std::atomic<bool> wrong_constructor{false}, owned_constructor{false};
    std::thread foreign([&] {
        try { gui::Host host; owned_constructor = host.is_ui_thread(); }
        catch (const std::invalid_argument&) { wrong_constructor = true; }
    });
    foreign.join();
#if defined(__APPLE__)
    check(wrong_constructor, "AppKit construction requires the process main thread");
#else
    check(owned_constructor && !wrong_constructor, "native host belongs to its construction thread");
#endif
    gui::Host host;
    check(!host.set_text("before open"), "window does not exist before run");
    good(host.executor().post([] {}), "pre-run submission is admitted");
    check(host.executor().every(0ms, [] {}).error == runtime::SubmitError::invalid,
          "reject zero period");
}
void lifecycle() {
    gui::Host host({.title="iiGUI lifecycle test"});
    auto ui = host.executor();
    runtime::Runtime workers({.workers=1, .capacity=8});
    bool opened = false, closed = false, background = false, resized = false;
    int ticks = 0;
    good(ui.every(20ms, [&] {
        check(host.is_ui_thread(), "timer uses UI thread");
        ++ticks;
        good(host.set_text("타이머 " + std::to_string(ticks)), "native Unicode text updates");
        if (ticks >= 3 && background && resized) good(host.request_close(), "close after work");
    }), "periodic timer admission");
    good(ui.after(4s, [&] { throw std::runtime_error("lifecycle watchdog"); }), "watchdog");
    auto result = host.run({
        .opened=[&] {
            opened = true;
            good(host.snapshot(), "snapshot while open");
            check(host.snapshot()->visible, "native window is visible");
            check(!host.run(), "run cannot reenter");
            check(!host.set_text("\xC0\x80"), "invalid UTF-8 update rejected");
            check(!host.set_title(std::string("abc\0def", 7)), "embedded NUL title update rejected");
            good(host.set_title("iiGUI 타이머"), "native Unicode title update");
            check(host.snapshot()->title == "iiGUI 타이머", "Unicode title snapshot");
            check(!host.set_content_size({1e30, 520}), "oversized native resize rejected");
            good(host.set_content_size({820, 520}), "resize native content");
            good(workers.post([&, ui] {
                check(!host.is_ui_thread(), "worker is not UI thread");
                auto wrong = host.set_text("wrong thread");
                if (wrong || wrong.error().code != core::ErrorCode::wrong_executor)
                    throw std::runtime_error("wrong-thread mutation was not rejected");
                std::this_thread::sleep_for(90ms);
                if (!ui.post([&] { background = true; }))
                    throw std::runtime_error("UI result rejected");
            }), "worker submission");
        },
        .resized=[&](gui::Size size) {
            check(host.is_ui_thread(), "resize callback uses UI thread");
            if (std::abs(size.width-820) < 1 && std::abs(size.height-520) < 1) resized = true;
        },
        .closed=[&] {
            closed = true;
            check(ui.post([] {}).error == runtime::SubmitError::stopped, "closed host rejects work");
        }
    });
    good(result, "native lifecycle run");
    good(workers.join(), "background workers joined");
    check(opened && closed && background && resized && ticks >= 3, "all lifecycle milestones");
    check(host.snapshot()->closed && !host.snapshot()->visible, "final closed snapshot");
    check(!host.run(), "host run is single use");
    std::cout << "ticks=" << ticks << " background=42 resize=820x520 closed=true\n";
}
void wakeup(bool earlier) {
    gui::Host host({.title=earlier ? "iiGUI deadline test" : "iiGUI idle wake test"});
    auto ui = host.executor();
    runtime::Runtime workers({.workers=1});
    bool delivered = false;
    runtime::Clock::duration latency{};
    good(ui.after(4s, [] { throw std::runtime_error("idle wake watchdog"); }), "long host deadline");
    auto result = host.run({.opened=[&] {
        good(workers.post([&, ui] {
            std::this_thread::sleep_for(100ms);
            const auto sent = runtime::Clock::now();
            auto reply = [&, sent] {
                latency = runtime::Clock::now()-sent;
                delivered = true;
                good(host.set_text("Background result: 42"), "background updates visible text");
                good(host.request_close(), "close idle host");
            };
            auto accepted = earlier ? ui.after(35ms, std::move(reply)) : ui.post(std::move(reply));
            if (!accepted) throw std::runtime_error("worker submission failed");
        }), "wake worker admitted");
    }});
    good(result, "host wakes without frame timer or input");
    good(workers.join(), "wake worker joined");
    check(delivered && latency < 1s, "native host promptly wakes for worker result");
    const auto stats = host.stats();
    good(stats, "host dispatch statistics");
    check(stats->dispatches < 30, "no permanent polling timer");
    std::cout << "delivery_ms=" << std::chrono::duration<double,std::milli>(latency).count()
              << " dispatches=" << stats->dispatches << '\n';
}
void close_veto() {
    gui::Host host;
    auto ui = host.executor();
    int requests = 0;
    bool closed = false;
    auto cancelled = ui.after(1ms, [] { throw std::runtime_error("cancelled callback ran"); });
    good(cancelled, "cancellable timer");
    check(ui.cancel(cancelled.token), "cancel pre-run timer");
    auto result = host.run({
        .opened=[&] {
            good(host.request_close(), "first close request");
            check(host.snapshot()->visible, "veto preserves native window");
            good(ui.after(25ms, [&] { good(host.request_close(), "second close request"); }), "second close timer");
        },
        .close_requested=[&] { return ++requests >= 2; },
        .closed=[&] { closed = true; }
    });
    good(result, "close veto lifecycle");
    check(requests == 2 && closed, "close accepted once after veto");
}
void failure() {
    gui::Host host;
    bool closed = false;
    auto result = host.run({.opened=[] { throw std::runtime_error("callback failure"); },
                            .closed=[&] { closed = true; }});
    check(!result && result.error().code == core::ErrorCode::failed && result.error().cause,
          "callback failure is returned, not unwound through a native callback");
    check(closed && host.snapshot()->closed, "failure cleans up window");
}
void close_failure() {
    gui::Host host;
    bool closed = false;
    auto result = host.run({
        .opened=[&] { good(host.request_close(), "request close with throwing veto"); },
        .close_requested=[]() -> bool { throw std::runtime_error("close callback failure"); },
        .closed=[&] { closed = true; }
    });
    check(!result && result.error().code == core::ErrorCode::failed && result.error().cause,
          "close callback exception is returned");
    check(closed && host.snapshot()->closed, "throwing close callback releases window");
}
void wrong_thread() {
    gui::Host host;
    std::thread worker([&] {
        auto wrong = [](const auto& value) {
            return !value && value.error().code == core::ErrorCode::wrong_executor;
        };
        check(wrong(host.run()), "foreign run rejected");
        check(wrong(host.snapshot()), "foreign snapshot rejected");
        check(wrong(host.stats()), "foreign stats rejected");
        check(wrong(host.set_title("foreign")), "foreign title rejected");
        check(wrong(host.set_content_size({700, 500})), "foreign resize rejected");
        check(wrong(host.request_close()), "foreign close rejected");
        check(wrong(host.set_canvas({})), "foreign GPU mutation rejected");
        check(wrong(host.clear_canvas()), "foreign GPU teardown rejected");
        check(wrong(host.graphics_info()), "foreign GPU inspection rejected");
        check(wrong(host.capture_canvas()), "foreign GPU capture rejected");
        check(wrong(host.event_loop()), "foreign Core-loop borrow rejected");
    });
    worker.join();
}
void graphics_contracts() {
    gui::CanvasFrame frame{.viewport={0,0,192,128}};
    check(gui::valid_canvas(frame), "empty opaque canvas is valid");
    auto invalid = [&](auto change) { auto value = frame; change(value); check(!gui::valid_canvas(value), "invalid graphics contract rejected"); };
    invalid([](auto& f) { f.viewport.width = 0; });
    invalid([](auto& f) { f.viewport.height = 8193; });
    invalid([](auto& f) { f.viewport.x = std::numeric_limits<float>::infinity(); });
    invalid([](auto& f) { f.background.red = -0.1f; });
    invalid([](auto& f) { f.background.alpha = 0.5f; });
    invalid([](auto& f) { f.acceleration = static_cast<gui::Acceleration>(42); });
    frame.panels.push_back({.bounds={8,8,64,48},.top={0,0,0,1},.bottom={1,1,1,1},.radius=12});
    check(gui::valid_canvas(frame), "rounded gradient panel valid");
    invalid([](auto& f) { f.panels[0].radius = 25; });
    invalid([](auto& f) { f.panels[0].top.red = std::numeric_limits<float>::quiet_NaN(); });
    invalid([](auto& f) { f.panels.resize(4097, f.panels.front()); });
    gui::Host host;
    check(!host.set_canvas(frame) && !host.capture_canvas() && !host.graphics_info(), "GPU operations require an open host");
}
void graphics() {
    gui::Host host({.title="iiGUI native GPU quality test"});
    auto ui = host.executor();
    gui::CanvasFrame frame{.viewport={28,28,192,128},.background={0,0,0,1}};
    frame.panels = {
        {.bounds={16,16,112,96},.top={0.5f,0.5f,0.5f,1},.bottom={0.5f,0.5f,0.5f,1},.radius=20},
        {.bounds={144,16,32,96},.top={0,0,0,1},.bottom={1,1,1,1},.radius=0},
        {.bounds={48,48,32,32},.top={1,0,0,0.5f},.bottom={1,0,0,0.5f},.radius=0}
    };
    good(ui.after(5s, [] { throw std::runtime_error("GPU watchdog"); }), "graphics watchdog");
    auto result = host.run({.opened=[&] {
        check(host.graphics_info()->api == gui::GraphicsApi::none, "no implicit GPU allocation for text hosts");
        for (int i=0; i<100; ++i) { frame.panels[0].top.red = i==99 ? 0.5f : 0.25f; good(host.set_canvas(frame), "GPU update admission"); }
        check(host.graphics_info()->scene_renders == 0, "GPU rendering waits until the Core turn finishes");
        good(ui.after(60ms, [&] {
            auto captured = host.capture_canvas(); good(captured, "actual GPU readback");
            const auto info = host.graphics_info(); good(info, "native adapter details");
            check(info->api != gui::GraphicsApi::none && !info->adapter.empty() && info->srgb_attachment,
                  "actual device and sRGB attachment reported");
            check(info->scene_renders == 1 && info->frame_requests == 100 && info->presentations >= 1,
                  "one render coalesces one hundred admitted updates");
            const double scale = host.snapshot()->backing_scale;
            check(captured->width == std::ceil(192*scale) && captured->height == std::ceil(128*scale), "capture uses device pixels");
            check(info->cached_texture_bytes == std::uint64_t(captured->width)*captured->height*4, "reported attachment storage matches physical size");
            auto pixel = [&](double x,double y,int channel=0) {
                return captured->rgba[(std::size_t(y*scale)*captured->width+std::size_t(x*scale))*4+channel];
            };
            check(pixel(0,0)==0 && pixel(16,16)==0 && pixel(80,32,3)==255, "opaque clear and clipped rounded corners");
            check(pixel(90,64)>=185 && pixel(90,64)<=190, "linear 0.5 encodes to sRGB 188");
            check(pixel(64,64)>=222 && pixel(64,64)<=227 && pixel(64,64,1)>=134 && pixel(64,64,1)<=140,
                  "alpha blends in linear space before sRGB encoding");
            check(pixel(160,24)<pixel(160,64) && pixel(160,64)<pixel(160,104), "gradient top-to-bottom orientation");
            int edges=0;
            for (std::size_t y=std::size_t(16*scale); y<std::size_t(36*scale); ++y)
                for (std::size_t x=std::size_t(16*scale); x<std::size_t(36*scale); ++x) {
                    auto c=captured->rgba[(y*captured->width+x)*4]; if (c>8 && c<180) ++edges;
                }
            check(edges>2, "analytic antialiasing produces fractional corner coverage");
            for (int i=0;i<50;++i) good(host.set_canvas(frame), "identical update admitted");
            good(host.capture_canvas(), "capture identical scene");
            check(host.graphics_info()->scene_renders==1, "identical scenes reuse retained attachment");
            auto invalid=frame; invalid.viewport.width=-1;
            check(!host.set_canvas(invalid), "invalid replacement rejected");
            check(host.graphics_info()->scene_renders==1, "invalid update preserves GPU resources");
            frame.viewport.width=240;
            good(host.set_canvas(frame), "resize GPU canvas");
            auto resized=host.capture_canvas(); good(resized, "capture resized GPU attachment");
            check(resized->width==std::ceil(240*scale) && host.graphics_info()->scene_renders==2, "attachment reallocates only on physical resize");
            auto strict=frame; strict.acceleration=gui::Acceleration::require_hardware;
            auto accepted=host.set_canvas(strict);
            if (info->hardware) { good(accepted, "hardware requirement accepted by actual GPU"); good(host.capture_canvas(), "strict hardware renders"); }
            else check(!accepted && accepted.error().code==core::ErrorCode::unsupported, "software cannot satisfy required hardware");
            std::cout << "GPU adapter=" << info->adapter << " hardware=" << info->hardware
                      << " pixels=" << captured->width << 'x' << captured->height
                      << " srgb_mid=" << int(pixel(90,64)) << " aa_pixels=" << edges
                      << " renders_per_100_requests=" << info->scene_renders << '\n';
            good(host.clear_canvas(), "explicit GPU teardown");
            check(host.graphics_info()->api==gui::GraphicsApi::none && !host.capture_canvas(), "cleared canvas releases attachment");
            good(host.request_close(), "close after native GPU checks");
        }), "graphics acceptance timer");
    }});
    good(result, "native GPU quality and resource lifecycle");
}
void core_io(const char* fixture) {
    auto child=process::Process::start({.arguments={fixture}});good(child,"native I/O fixture");
    std::array<std::byte,1> ready{};
    good(child->output().wait(io::Interest::readable,10s),"I/O fixture startup readiness");
    auto startup=child->output().read(ready);
    check(startup && startup->bytes==1 && ready[0]==std::byte{'R'},"I/O fixture startup handshake");
    gui::Host host({.title="Core native I/O on UI thread"}); auto ui=host.executor();
    io::Buffer buffer(2); bool delivered=false; auto started=runtime::Clock::now();
    good(ui.after(4s,[] { throw std::runtime_error("Core I/O UI watchdog"); }),"I/O watchdog");
    good(host.run({.opened=[&] {
        started=runtime::Clock::now();
        good(ui.read(child->output(),{buffer,0,2,3s},[&](auto reply) {
            check(host.is_ui_thread(),"native readiness completion uses UI owner");
            check(reply && reply->transfer.bytes==2 && buffer.bytes()[0]==std::byte{'4'},"Core I/O reaches UI");
            delivered=true;
            good(host.set_text("Core native pipe: 42"),"I/O result updates native UI");
            good(host.request_close(),"close from I/O completion");
        }),"UI endpoint admits native channel input");
        const std::array release{std::byte{'g'}};
        auto sent=child->input().write(release);
        check(sent && *sent==1,"release fixture after native UI I/O admission");
    }}),"idle host wakes for I/O readiness");
    check(delivered && runtime::Clock::now()-started<1s,"I/O readiness precedes the long request timeout");
    auto stats=host.stats();good(stats,"Core host statistics");
    check(stats->host.io_waits>0 && !stats->host.connected,"readiness waiter joined before host cleanup");
    std::cout<<"CORE_GUI_IO_OK waits="<<stats->host.io_waits<<" signals="<<stats->host.wake_signals<<'\n';
}
void core_operation() {
    gui::Host host; auto ui=host.executor(); runtime::Runtime workers({.workers=1});
    runtime::TaskScope view; runtime::OperationOwner operations;
    bool delivered=false,scoped=false,removed_ran=false;
    auto removed=std::make_unique<runtime::TaskScope>();
    good(removed->sender().at(ui,runtime::Clock::now()+40ms,[&] { removed_ran=true; }),"scoped UI timer");
    good(removed->close(),"cancel scoped UI timer before opening");
    auto work=operations.submit<int>(workers,[](runtime::WorkContext&)->core::Result<int> {
        std::this_thread::sleep_for(80ms); return 42;
    });
    { auto temporary=host.executor();
      check(work.then_on(temporary,view.sender(),[&](auto reply) {
        check(host.is_ui_thread() && reply->value==42,"Core operation completion uses native UI owner");
        delivered=true; if (scoped) good(host.request_close(),"finish tracked completion");
      }),"tracked completion owns its temporary UI endpoint"); }
    good(view.sender().at(ui,runtime::Clock::now()+20ms,[&] {
        scoped=true; if (delivered) good(host.request_close(),"finish scoped timer");
    }),"TaskScope sender accepts structural timed UI execution");
    good(ui.after(4s,[] { throw std::runtime_error("Core operation UI watchdog"); }),"operation watchdog");
    good(host.run(),"Core Operation/TaskScope GUI lifecycle");
    good(operations.close(),"close operation owner"); good(view.close(),"close UI scope");good(workers.join(),"join tracked worker");
    check(delivered && scoped && !removed_ran,"tracked work and scoped cancellation reach UI safely");
}
void core_frame() {
    gui::Host host; auto ui=host.executor();
    std::unique_ptr<runtime::FrameLoop> frames;
    int updates=0,prepared=0; bool pinned=false,blocking=false;
    good(ui.after(4s,[] { throw std::runtime_error("Core frame UI watchdog"); }),"frame watchdog");
    good(host.run({.opened=[&] {
        auto events=host.event_loop();good(events,"borrow bound Core loop");
        std::thread foreign([&] { try { events->get().poll(); } catch(const std::logic_error&) { pinned=true; } }); foreign.join();
        try { events->get().run(); } catch(const std::logic_error&) { blocking=true; }
        frames=std::make_unique<runtime::FrameLoop>(events->get(),runtime::FrameLoopOptions{.capacity=4});
        good(frames->add([&](const auto&) { check(host.is_ui_thread(),"frame update uses UI thread");++updates; }),"Core frame update");
        good(frames->add([&](const auto&) {
            check(updates==prepared+1,"update precedes prepare_present"); ++prepared;
            gui::CanvasFrame frame{.viewport={28,180,320,180},.background={0,0,0,1}};
            frame.panels.push_back({.bounds={16,16,288,148},.top={0.02f,0.15f,0.8f,1},.bottom={0.01f,0.65f,0.25f,1},.radius=20});
            good(host.set_canvas(std::move(frame)),"frame stage prepares native GPU scene");
            if (prepared>=3) {
                good(host.capture_canvas(),"Core frame reaches actual GPU");
                good(host.request_close(),"close after Core frame stages");
            }
        },{.phase=runtime::FramePhase::prepare_present}),"Core frame presentation preparation");
        good(ui.every(20ms,[&] { frames->step(); }),"externally hosted frame cadence");
    }}),"Core FrameLoop and native GUI integration");
    check(pinned && blocking && updates==3 && prepared==3,"bound Core loop cannot lose its UI thread");
    frames.reset();
}
void core_stop() {
    gui::Host host; bool closed=false;
    good(host.run({.opened=[&] {
        auto events=host.event_loop();good(events,"Core loop for terminal stop");
        good(events->get().post([loop=std::ref(events->get())] { loop.get().request_stop(); }),"direct Core submission wakes UI");
    },.closed=[&] { closed=true; }}),"terminal Core stop closes native host normally");
    check(closed && host.snapshot()->closed,"Core stop maps to native cleanup");
}
void shutdown_race() {
    gui::UiExecutor retired;
    runtime::Runtime workers({.workers=2});
    std::atomic<bool> reject{false};
    {
        gui::Host host;
        retired = host.executor();
        auto result = host.run({.opened=[&] {
            good(workers.post([ui=retired, &reject] {
                std::this_thread::sleep_for(100ms);
                reject = ui.post([] {}).error == runtime::SubmitError::stopped;
            }), "late worker");
            good(host.request_close(), "close with worker active");
        }});
        good(result, "close without waiting on UI result");
    }
    good(workers.join(), "join after GUI teardown");
    check(reject && retired.post([] {}).error == runtime::SubmitError::stopped,
          "weak UI endpoint rejects after host destruction");
}
void producer_race() {
    gui::Host host({.task_capacity=32});
    auto ui = host.executor();
    runtime::Runtime workers({.workers=2});
    std::atomic<int> rejected{0};
    std::atomic<bool> closed{false}, after_close{false};
    int delivered = 0;
    auto result = host.run({
        .opened=[&] {
            good(ui.after(50ms, [&] { good(host.request_close(), "close during producers"); },
                          runtime::Priority::high), "close timer");
            for (int worker = 0; worker < 2; ++worker) {
                good(workers.post([&, ui] {
                    for (;;) {
                        const auto submitted = ui.post([&] {
                            if (closed) after_close = true;
                            ++delivered;
                        });
                        if (submitted.error == runtime::SubmitError::stopped) { ++rejected; break; }
                        if (submitted.error == runtime::SubmitError::full) std::this_thread::yield();
                    }
                }), "concurrent producer");
            }
        },
        .closed=[&] { closed = true; }
    });
    good(result, "shutdown racing producers");
    good(workers.join(), "producers joined");
    check(delivered > 0 && rejected == 2 && !after_close, "no UI delivery after close barrier");
    std::cout << "delivered=" << delivered << " rejected_producers=" << rejected << '\n';
}
void full_queue() {
    gui::Host host({.task_capacity=1});
    auto ui = host.executor();
    bool opened = false, delivered = false;
    good(ui.post([&] {
        check(opened, "open callback precedes queued work");
        delivered = true;
        good(host.request_close(), "close with capacity one");
    }), "one-slot queue accepts pre-run work");
    check(ui.post([] {}).error == runtime::SubmitError::full, "queue admission stays bounded");
    good(host.run({.opened=[&] { opened = true; }}), "full queue does not prevent native startup");
    check(delivered, "pre-run accepted work executes");
}
}
int main(int argc, char** argv) {
    try {
        const std::string name = argc > 1 ? argv[1] : "lifecycle";
        if (name == "contracts") contracts();
        else if (name == "lifecycle") lifecycle();
        else if (name == "wakeup") wakeup(false);
        else if (name == "earlier_deadline") wakeup(true);
        else if (name == "close_veto") close_veto();
        else if (name == "failure") failure();
        else if (name == "close_failure") close_failure();
        else if (name == "shutdown_race") shutdown_race();
        else if (name == "producer_race") producer_race();
        else if (name == "full_queue") full_queue();
        else if (name == "wrong_thread") wrong_thread();
        else if (name == "graphics_contracts") graphics_contracts();
        else if (name == "graphics") graphics();
        else if (name == "core_io") { if (argc<3) throw std::runtime_error("missing I/O fixture"); core_io(argv[2]); }
        else if (name == "core_operation") core_operation();
        else if (name == "core_frame") core_frame();
        else if (name == "core_stop") core_stop();
        else throw std::runtime_error("unknown test");
        std::cout << name << ": " << assertions.load() << " assertions passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "iiGUI: " << error.what() << '\n'; return 1;
    }
}
