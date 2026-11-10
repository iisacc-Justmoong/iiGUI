#pragma once
#include <iiGUI/Graphics.h>

namespace ii::gui::detail {
// Internal surfaces may composite over a borrowed window. Public raw canvas
// submissions retain their existing opaque-background contract.
[[nodiscard]] IIGUI_PAINTER_EXPORT bool valid_surface_canvas(const CanvasFrame &) noexcept;
// Validates and retains the topological plan in one pass for external painters.
[[nodiscard]] IIGUI_PAINTER_EXPORT std::optional<CanvasScenePlan> valid_surface_scene_plan(const CanvasFrame &) noexcept;
}
