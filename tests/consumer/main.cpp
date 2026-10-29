#include <iiGUI/iiGUI.h>
#include <LVUI/NativeDocument.h>
#include <iiSDKCore/Runtime/Runtime.h>
#include <iiSDKCore/Runtime/Operation.h>
#include <iiSDKCore/Runtime/FrameLoop.h>
#include <chrono>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <thread>

static_assert(__cplusplus >= 202302L, "Consumer must inherit C++23 from the installed SDK");
using namespace std::chrono_literals;
int main(int argc,char** argv) {
    try {
        const bool vulkan=argc==2 && std::string_view(argv[1])=="--vulkan";
        if (argc>1 && !vulkan) throw std::runtime_error("Usage: installed consumer [--vulkan]");
        const auto renderer=vulkan?ii::gui::Renderer::vulkan:ii::gui::Renderer::automatic;
        ii::gui::Host host({.title="iiGUI installed consumer"});
        auto ui = host.executor();
        ii::runtime::Runtime workers({.workers=1});
        ii::runtime::OperationOwner operations;
        ii::runtime::TaskScope view;
        std::unique_ptr<ii::runtime::FrameLoop> frames;
        int ticks = 0, value = 0;
        bool resized = false, closed = false, gpu = false, scoped = false, lvui_rendered = false;
        int frame_updates=0;
        auto result = host.run({
            .opened=[&] {
                auto events=host.event_loop();if (!events) throw std::runtime_error("Installed Core host access failed");
                frames=std::make_unique<ii::runtime::FrameLoop>(events->get(),ii::runtime::FrameLoopOptions{.capacity=2});
                if (!frames->add([&](const auto&) { ++frame_updates; })) throw std::runtime_error("Installed frame registration failed");
                if (!view.sender().at(ui,ii::runtime::Clock::now()+10ms,[&] { scoped=true; }))
                    throw std::runtime_error("Installed structural timed scope failed");
                if (!host.set_text("Installed iiGUI consumer\nC++23 · no source-tree headers") ||
                    !host.set_content_size({760, 480})) throw std::runtime_error("Native setup failed");
                auto document=lvui::parse("<view style='padding:20px; background:#123456'><p style='color:white'>Installed LVUI 한글</p></view>");
                if (!document || !host.set_view(*document,renderer)) throw std::runtime_error("Installed LVUI package failed");
                auto view_pixels=host.capture_canvas();
                if (!view_pixels) throw std::runtime_error("Installed LVUI rendering failed");
                std::size_t glyph_pixels=0;
                for (std::size_t i=0;i<view_pixels->rgba.size();i+=4)
                    if (view_pixels->rgba[i]>200 && view_pixels->rgba[i+1]>200 && view_pixels->rgba[i+2]>200) ++glyph_pixels;
                lvui_rendered=glyph_pixels>20;
                ii::gui::CanvasFrame frame{.viewport={28,150,320,180},.background={0,0,0,1}};
                frame.renderer=renderer;
                frame.panels.push_back({.bounds={16,16,288,148},.top={0.02f,0.1f,0.9f,1},
                    .bottom={0.01f,0.65f,0.2f,1},.radius=20});
                if (!ii::gui::valid_canvas(frame) || !host.set_canvas(std::move(frame)))
                    throw std::runtime_error("Installed GPU API setup failed");
                if (!ui.every(20ms, [&] {
                    frames->step();
                    if (++ticks >= 3 && value == 42 && resized && scoped && frame_updates>=3) {
                        const auto pixels=host.capture_canvas(); const auto info=host.graphics_info();
                        gpu=pixels && info && info->srgb_attachment && info->scene_renders>0 && !info->adapter.empty();
                        if (vulkan) gpu=gpu && info->api==ii::gui::GraphicsApi::vulkan && info->presentations>0;
                        if (!gpu) throw std::runtime_error("Installed native GPU failed");
                        (void)host.request_close();
                    }
                })) throw std::runtime_error("Timer admission failed");
                if (!ui.after(4s, [] { throw std::runtime_error("Consumer watchdog"); }))
                    throw std::runtime_error("Watchdog admission failed");
                auto work=operations.submit<int>(workers,[](ii::runtime::WorkContext&)->ii::core::Result<int> {
                    std::this_thread::sleep_for(60ms);
                    return 6*7;
                });
                if (work.admission()!=ii::runtime::SubmitError::none || !work.then_on(host.executor(),view.sender(),[&](auto result) {
                    if (result->value) value=*result->value;
                })) throw std::runtime_error("Installed owned completion failed");
            },
            .resized=[&](ii::gui::Size size) { resized = std::abs(size.width-760) < 1 && std::abs(size.height-480) < 1; },
            .closed=[&] { closed = true; }
        });
        const auto stopped=operations.close(); const auto released=view.close();
        frames.reset();
        if (!workers.join() || !stopped || !released || !result || ticks < 3 || value != 42 || !resized || !closed || !gpu || !scoped || !lvui_rendered)
            throw std::runtime_error("Installed consumer acceptance failed");
        std::cout << "INSTALLED_GUI_OK ticks=" << ticks << " background=" << value
                  << " resize=" << std::boolalpha << resized << " closed=" << closed << " gpu=" << gpu
                  << " core_frames=" << frame_updates << " scoped=" << scoped << " lvui=" << lvui_rendered << " vulkan=" << vulkan << '\n';
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
