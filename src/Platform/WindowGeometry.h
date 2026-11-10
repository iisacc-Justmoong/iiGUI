#pragma once

#include <optional>
#include <string>

namespace ii::gui {
struct Size final {
    double width=0, height=0;
    friend bool operator==(const Size&,const Size&)=default;
};
// A declaration update owns only present fields. Unchanged declarations never
// restore an old size after the host or user resizes the surface.
struct WindowGeometry final {
    std::optional<std::string> title;
    std::optional<double> width,height,minimum_width,minimum_height;
    friend bool operator==(const WindowGeometry&,const WindowGeometry&)=default;
};
} // namespace ii::gui
