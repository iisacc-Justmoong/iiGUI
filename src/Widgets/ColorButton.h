#pragma once
#include <iiGUI/NativeWidgets.h>
namespace ii::gui::detail {
struct ColorButton final {
    ColorButtonOptions options;
    WidgetColor current;
    std::shared_ptr<ColorChangePort> receiver;
    std::uint64_t rasterizations=0;
    [[nodiscard]] std::shared_ptr<const RasterImage> paint(Size,bool hovered,bool pressed,bool focused,WidgetColor primary);
private:
    struct Key final {
        Size size;WidgetColor color,primary;bool ring,circular,hovered,pressed,focused;double border_opacity;
        friend bool operator==(const Key&,const Key&)=default;
    };
    std::optional<Key> key_;
    std::shared_ptr<const RasterImage> image_;
};
}
