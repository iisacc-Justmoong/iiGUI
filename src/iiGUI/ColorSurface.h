#pragma once
#include "NativeWidgets.h"

namespace ii::gui::detail {
// Per-field background cache; cursors are separate immutable foreground images.
class ColorSurface final {
public:
    ColorSurfaceOptions options;
    std::shared_ptr<ColorChangePort> receiver;
    std::function<void(WidgetColor)> edited;
    std::uint64_t rasterizations=0;
    int drag_part=-1;
    [[nodiscard]] bool begin(Rect,double x,double y);
    [[nodiscard]] core::Result<void> edit(Rect,double x,double y);
    [[nodiscard]] core::Result<void> key(WidgetKey,bool shift);
    void paint(CanvasFrame& frame,Rect r,double scale){paint(frame.images,r,scale);}
    void paint(CanvasLayer& layer,Rect r,double scale){paint(layer.images,r,scale);}
private:
    void paint(std::vector<ImageRun>&,Rect,double scale);
    std::shared_ptr<const RasterImage> domain;
    ColorField cached_field=ColorField::wheel;
    std::uint32_t cached_width=0,cached_height=0;
    double cached_dependency=-1;
};
} // namespace ii::gui::detail
