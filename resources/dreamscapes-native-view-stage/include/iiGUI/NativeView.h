#pragma once
#include <iiGUI/NativeWidgets.h>

namespace ii::gui {
// Build one native root per activation. Retain per-view state with the builder;
// never retain the builder itself. Painting, input and work remain C++ code.
class IIGUI_EXPORT NativeViewSource {
public:
    virtual ~NativeViewSource()=default;
    [[nodiscard]] virtual core::Result<void> build(WidgetBuilder&) const=0;
};
struct NativeViewOptions final {
    std::shared_ptr<NativeViewSource> source;
    double width=0,height=0;
    bool enabled=true,clip=true;
    friend bool operator==(const NativeViewOptions&,const NativeViewOptions&)=default;
};
// Weak endpoint for a retained native content generation. A failed candidate
// preserves the old generation. Equal source identity preserves its edits.
class IIGUI_EXPORT NativeView final {
public:
    NativeView()=default;
    [[nodiscard]] static core::Result<NativeView> create(WidgetBuilder&,Widget parent,std::string_view id,NativeViewOptions={});
    [[nodiscard]] Widget root()const noexcept{return root_;}
    [[nodiscard]] core::Result<Widget> content_item(std::string_view id={})const;
    [[nodiscard]] core::Result<NativeViewOptions> options()const;
    [[nodiscard]] core::Result<void> set_options(NativeViewOptions)const;
    [[nodiscard]] core::Result<void> set_source(std::shared_ptr<NativeViewSource>)const;
private:
    struct Impl;
    [[nodiscard]] core::Result<std::shared_ptr<Impl>> lock()const;
    Widget root_;
    std::weak_ptr<Impl> state_;
};
}
