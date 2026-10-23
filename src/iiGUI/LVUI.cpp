#include <iiGUI/LVUI.h>
#include "NativeText.h"
#include <algorithm>
#include <optional>

namespace ii::gui {
namespace {
float linear(float encoded) {
    return encoded<=0.04045f ? encoded/12.92f : std::pow((encoded+0.055f)/1.055f,2.4f);
}
Color color(lvui::Color c) { return {linear(c.red),linear(c.green),linear(c.blue),c.alpha}; }
float resolve(lvui::Length length,float basis,float automatic) {
    if (length.unit==lvui::Unit::automatic) return automatic;
    return length.unit==lvui::Unit::percent ? basis*length.value/100 : length.value;
}
struct Typography final {
    Color color{0,0,0,1};
    float size=14;
    unsigned weight=400;
    TextAlignment alignment=TextAlignment::left;
};
struct Layout final {
    ViewLayout result;
    [[noreturn]] void overflow(const lvui::Node& node,std::string reason) {
        throw core::Error{core::ErrorCode::out_of_range,
            "LVUI <"+node.tag+">"+(node.id.empty()?"":" #"+node.id)+": "+reason};
    }
    float place(const lvui::Node& node,float x,float y,float width,float available_height,
                bool root,Typography inherited) {
        const auto& s=node.style;
        if (s.color) inherited.color=color(*s.color);
        if (s.font_size) inherited.size=*s.font_size;
        if (s.font_weight) inherited.weight=*s.font_weight;
        if (s.text_align) inherited.alignment=*s.text_align==lvui::Alignment::center?TextAlignment::center:
            *s.text_align==lvui::Alignment::right?TextAlignment::right:TextAlignment::left;
        const bool fixed_height=root || s.height.unit!=lvui::Unit::automatic;
        const float height=resolve(s.height,available_height,root?available_height:0);
        const auto p=s.padding;
        const float inner_width=width-p.left-p.right;
        const float inner_height=(fixed_height?height:available_height)-p.top-p.bottom;
        if (!std::isfinite(width) || width<0 || width>8192 || inner_width<0 || inner_height<0)
            overflow(node,"padding or size exceeds available space");
        const auto panel_index=result.frame.panels.size();
        if (s.background) result.frame.panels.push_back({});
        const auto element_index=result.elements.size();
        if (!node.id.empty()) result.elements.push_back({node.id,{}});
        float used=0;
        if (node.kind==lvui::Kind::text) {
            if (inner_width<=0) overflow(node,"text needs positive content width");
            TextRun run{{x+p.left,y+p.top,inner_width,8192},node.text,
                inherited.color,inherited.size,inherited.weight,inherited.alignment,{}};
            used=detail::measure_text(run);
            if (fixed_height) run.bounds.height=inner_height; else run.bounds.height=used;
            if (run.bounds.height>0 && !run.text.empty()) result.frame.texts.push_back(std::move(run));
        } else if (!node.children.empty()) {
            const float gaps=s.gap*float(node.children.size()-1);
            float taken=gaps; std::size_t automatic=0;
            if (s.direction==lvui::Direction::row) {
                for (const auto& child:node.children) {
                    if (child.style.width.unit==lvui::Unit::automatic) ++automatic;
                    else taken+=resolve(child.style.width,inner_width,0);
                }
                if (taken>inner_width+0.01f) overflow(node,"row widths and gaps exceed content width");
            }
            float advance=0;
            for (const auto& child:node.children) {
                const bool row=s.direction==lvui::Direction::row;
                const float child_width=resolve(child.style.width,inner_width,
                    row?(automatic?(inner_width-taken)/float(automatic):0):inner_width);
                if (child_width>inner_width+0.01f) overflow(child,"width exceeds parent content");
                const float child_height=place(child,x+p.left+(row?advance:0),y+p.top+(row?0:advance),
                    child_width,inner_height,false,inherited);
                if (row) { advance+=child_width+s.gap; used=std::max(used,child_height); }
                else { advance+=child_height+s.gap; used=advance-s.gap; }
            }
            if (fixed_height && used>inner_height+0.01f) overflow(node,"children exceed content height");
        }
        const float actual=fixed_height?height:used+p.top+p.bottom;
        if (!std::isfinite(actual) || actual<0 || actual>8192 || std::abs(x)>32767 || std::abs(y)>32767)
            overflow(node,"layout exceeds canvas geometry limits");
        const Rect bounds{x,y,width,actual};
        if (s.background) {
            auto c=color(*s.background);
            if (width>0 && actual>0) result.frame.panels[panel_index]={bounds,c,c,std::min({s.radius,width/2,actual/2})};
            else result.frame.panels.erase(result.frame.panels.begin()+std::ptrdiff_t(panel_index));
        }
        if (!node.id.empty()) result.elements[element_index].bounds=bounds;
        return actual;
    }
};
}
core::Result<ViewLayout> layout_view(const lvui::Document& document,Size viewport) {
    if (!std::isfinite(viewport.width) || !std::isfinite(viewport.height) ||
        viewport.width<=0 || viewport.height<=0 || viewport.width>8192 || viewport.height>8192)
        return core::fail(core::ErrorCode::invalid_argument,"LVUI viewport must be finite and within 0..8192 logical units");
    try {
        Layout layout;
        layout.result.frame.viewport={0,0,float(viewport.width),float(viewport.height)};
        const auto& root=document.root();
        const float width=resolve(root.style.width,float(viewport.width),float(viewport.width));
        const float height=layout.place(root,0,0,width,float(viewport.height),true,{});
        if (width>viewport.width+0.01 || height>viewport.height+0.01)
            return core::fail(core::ErrorCode::out_of_range,"LVUI root exceeds the viewport");
        if (!valid_canvas(layout.result.frame))
            return core::fail(core::ErrorCode::out_of_range,"LVUI result exceeds canvas text or geometry limits");
        return std::move(layout.result);
    } catch (const core::Error& error) { return std::unexpected(error); }
    catch (const std::exception& error) { return core::fail(core::ErrorCode::failed,error.what()); }
}
core::Result<void> Host::set_view(const lvui::Document& document,Renderer renderer) {
    const auto window=snapshot();
    if (!window) return std::unexpected(window.error());
    auto layout=layout_view(document,window->size);
    if (!layout) return std::unexpected(layout.error());
    layout->frame.renderer=renderer;
    return set_canvas(std::move(layout->frame));
}
} // namespace ii::gui
