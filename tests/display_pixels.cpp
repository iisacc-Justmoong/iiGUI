#include <iiGUI/DisplayPixelsPolicy.h>
#include <iiGUI/Host.h>
#include <cstdint>
#include <limits>
#include <stdexcept>

using namespace ii::gui;
void check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
int main() {
    using detail::checked_display_pixels;
    using detail::oriented_display_pixels;
    check(checked_display_pixels(3840,2160)==DisplayPixels{3840,2160},"4K pixels changed");
    check(oriented_display_pixels(2064,2752,2752,2064)==DisplayPixels{2752,2064},"landscape orientation lost pixels");
    check(oriented_display_pixels(2064,2752,2064,2752)==DisplayPixels{2064,2752},"portrait orientation lost pixels");
    // A narrow portrait-shaped Split View can sit on a landscape display: the
    // display dimensions, rather than the window dimensions, define rotation.
    check(oriented_display_pixels(2048,2732,1366,1024)==DisplayPixels{2732,2048},"split view changed display orientation");
    check(oriented_display_pixels(1000,1000,1000,1000)==DisplayPixels{1000,1000},"square orientation changed");
    check(!checked_display_pixels(0,1000) && !checked_display_pixels(1000,0),"zero display accepted");
    check(!checked_display_pixels(std::uint64_t(std::numeric_limits<int>::max())+1,1),"overflow display accepted");
    check(!oriented_display_pixels(1920,1080,0,1080),"unavailable orientation accepted");
    Host host;
    check(!host.display_pixels(),"unopened host reported a display");
}
