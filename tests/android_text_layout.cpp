#include "../src/Text/AndroidTextLayout.h"
#include <cmath>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {
using namespace ii::gui;
using namespace ii::gui::detail;
void check(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
bool equal(float a, float b) { return std::abs(a - b) < .001f; }
}

int main() {
    // A right-to-left paragraph can have a large native left offset. Alignment
    // must use the shaped line's physical extent, not its logical direction.
    std::vector<AndroidTextLine> lines{{80, 20, 0, 20}, {5, 35, 20, 44}};
    Rect bounds{10, 20, 100, 80};
    auto natural = android_text_metrics(lines, 44, 0);
    check(equal(natural.width, 35) && equal(natural.height, 44), "Native line metrics changed");
    auto fixed = android_text_metrics(lines, 44, 30);
    check(equal(fixed.width, 35) && equal(fixed.height, 60), "Fixed line-height measurement changed");

    auto left = android_text_placement(bounds, TextAlignment::left, lines[0], 0, 0);
    auto center = android_text_placement(bounds, TextAlignment::center, lines[0], 0, 0);
    auto right = android_text_placement(bounds, TextAlignment::right, lines[0], 0, 0);
    check(equal(left.x, -70) && equal(center.x, -30) && equal(right.x, 10),
          "Physical alignment failed for an RTL-shaped line");
    check(equal(left.y, 20) && equal(left.clip_top, 20) && equal(left.clip_bottom, 40),
          "Natural line coordinates lost their original layout origin");
    auto second = android_text_placement(bounds, TextAlignment::left, lines[1], 1, 30);
    check(equal(second.x, 5) && equal(second.y, 33) && equal(second.clip_top, 50) && equal(second.clip_bottom, 80),
          "Fixed line height did not center and clip the second native line");

    for (auto invalid : {AndroidTextLine{0, -1, 0, 1}, AndroidTextLine{0, 1, 4, 3},
                         AndroidTextLine{std::numeric_limits<float>::quiet_NaN(), 1, 0, 1}}) {
        bool rejected = false;
        try { (void)android_text_metrics(std::vector<AndroidTextLine>{invalid}, 10, 0); }
        catch (const std::invalid_argument&) { rejected = true; }
        check(rejected, "Invalid native layout geometry was admitted");
    }
}
