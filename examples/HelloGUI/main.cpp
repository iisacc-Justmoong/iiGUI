#include <iiGUI/iiGUI.h>
#include <iiSDKCore/Runtime/Runtime.h>
#include <iiSDKCore/Runtime/Operation.h>
#include <iiSDKCore/Runtime/FrameLoop.h>
#include <array>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <numeric>
#include <sstream>
#include <stdexcept>
#include <thread>

using namespace std::chrono_literals;
namespace {
template<class T> void require(const T& result, const char* message) {
    if (!result) throw std::runtime_error(message);
}
}
int main(int argc, char** argv) {
    try {
        bool self_test = false;
        if (argc == 2 && std::string_view(argv[1]) == "--self-test") self_test = true;
        else if (argc != 1) throw std::invalid_argument("Usage: HelloGUI [--self-test]");
        ii::gui::Host host({.title="iiSDK · HelloGUI", .size={760, 540}, .minimum_size={480,400}});
        auto ui = host.executor();
        ii::runtime::Runtime workers({.workers=1, .capacity=16});
        ii::runtime::OperationOwner operations;
        ii::runtime::TaskScope view;
        std::unique_ptr<ii::runtime::FrameLoop> frames;
        int ticks = 0, background = 0, resize_events = 0;
        bool closed = false, resized = false, result_on_ui = false, gpu_verified = false;
        auto display = [&] {
            const auto window = host.snapshot();
            require(window, "Cannot inspect native window");
            std::ostringstream text;
            const auto graphics=host.graphics_info();
            text << "HelloGUI · Native C++23\n"
                 << "한글 · العربية · हिन्दी · 👋\n"
                 << "Core timer: " << ticks << "  |  "
                 << "Background result: " << (background ? std::to_string(background) : "working…")
                 << "\nContent size: " << int(window->size.width) << " × " << int(window->size.height)
                 << " · " << window->backing_scale << "x · Resize events: " << resize_events
                 << "\n" << (graphics && !graphics->adapter.empty() ? graphics->adapter : "Native GPU canvas")
                 << "\nResize the window. Close to exit.";
            require(host.set_text(text.str()), "Cannot update native label");
            const float width=float(window->size.width-56),height=float(window->size.height-218);
            ii::gui::CanvasFrame frame{.viewport={28,190,width,height},.background={0.012f,0.018f,0.035f,1}};
            const float card=(width-32)/3;
            frame.panels={
                {.bounds={8,8,card,height-16},.top={0.02f,0.2f,0.85f,1},.bottom={0.015f,0.65f,0.35f,1},.radius=18},
                {.bounds={card+16,8,card,height-16},.top={0.42f,0.05f,0.8f,1},.bottom={0.04f,0.12f,0.5f,1},.radius=18},
                {.bounds={2*card+24,8,card,height-16},.top={0.95f,0.3f,0.04f,1},.bottom={0.45f,0.04f,0.1f,1},.radius=18},
                {.bounds={24,height-36,(width-48)*std::min(1.0f,float(ticks%9+1)/9),12},
                    .top={1,1,1,0.6f},.bottom={1,1,1,0.6f},.radius=6}
            };
            require(host.set_canvas(std::move(frame)),"Cannot attach native GPU canvas");
        };
        auto result = host.run({
            .opened=[&] {
                auto events=host.event_loop(); require(events,"Cannot access hosted Core loop");
                frames=std::make_unique<ii::runtime::FrameLoop>(events->get(),ii::runtime::FrameLoopOptions{.capacity=4});
                require(frames->add([&](const auto&) { display(); },
                    {.phase=ii::runtime::FramePhase::prepare_present}),"Cannot register Core presentation phase");
                display();
                std::cout << "HELLOGUI_OPEN visible=" << std::boolalpha << host.snapshot()->visible << '\n' << std::flush;
                require(ui.every(250ms, [&] {
                    ++ticks;
                    frames->step();
                    if (self_test && ticks >= 3 && background == 42 && resized) {
                        const auto info=host.graphics_info();
                        const auto pixels=host.capture_canvas();
                        require(info && pixels && info->srgb_attachment && !info->adapter.empty() &&
                            info->scene_renders>0 && pixels->width>0,"Native GPU acceptance failed");
                        gpu_verified=true;
                        std::cout << "HELLOGUI_GPU adapter=" << info->adapter << " hardware=" << info->hardware
                                  << " renders=" << info->scene_renders << " bytes=" << info->cached_texture_bytes << '\n';
                        require(host.request_close(), "Cannot close self-test window");
                    }
                }), "Cannot schedule Core timer");
                auto computation=operations.submit<int>(workers,[&](ii::runtime::WorkContext& context)->ii::core::Result<int> {
                    std::this_thread::sleep_for(350ms);
                    if (auto ready=context.checkpoint(); !ready) return std::unexpected(ready.error());
                    constexpr std::array values{2, 4, 6, 8, 10, 12};
                    return std::accumulate(values.begin(), values.end(), 0);
                });
                require(computation.admission()==ii::runtime::SubmitError::none,"Cannot submit tracked background work");
                require(computation.then_on(host.executor(),view.sender(),[&](auto reply) {
                        if (!reply->value) return; // Closing may cancel work before a value exists.
                        background = *reply->value;
                        result_on_ui = host.is_ui_thread();
                        display();
                        std::cout << "HELLOGUI_BACKGROUND value=" << background << " ui_thread=" << result_on_ui << '\n' << std::flush;
                }),"Cannot register scoped UI completion");
                if (self_test) {
                    require(view.sender().at(ui,ii::runtime::Clock::now()+100ms, [&] {
                        require(host.set_content_size({840, 540}), "Cannot resize self-test window");
                    }), "Cannot schedule resize");
                    require(ui.after(5s, [] { throw std::runtime_error("HelloGUI self-test timed out"); }), "Cannot schedule watchdog");
                }
            },
            .resized=[&](ii::gui::Size size) {
                ++resize_events;
                resized = resized || (std::abs(size.width-840) < 1 && std::abs(size.height-540) < 1);
                display();
                std::cout << "HELLOGUI_RESIZE width=" << size.width << " height=" << size.height << '\n' << std::flush;
            },
            .closed=[&] { closed = true; std::cout << "HELLOGUI_CLOSED\n" << std::flush; }
        });
        require(operations.close(),"Cannot close tracked background work");
        require(view.close(),"Cannot close UI work scope");
        require(workers.join(), "Background runtime failed");
        frames.reset();
        if (!result) throw std::runtime_error(result.error().message);
        if (self_test && !(ticks >= 3 && background == 42 && resized && closed && result_on_ui && gpu_verified))
            throw std::runtime_error("HelloGUI acceptance milestones were not all observed");
        std::cout << "HELLOGUI_OK ticks=" << ticks << " background=" << background
                  << " resized=" << std::boolalpha << resized << " closed=" << closed << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "HelloGUI: " << error.what() << '\n'; return 1;
    }
}
