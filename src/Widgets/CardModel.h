#pragma once
#include <iiGUI/ButtonModel.h>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
namespace ii::gui {
enum class CardType {file,file_preview,folder,project,device,model,member,link};enum class CardSize {small,medium,large};
enum class CardDetail {brief,detailed};enum class CardDisplayState {automatic,normal,hover,selected};
struct CardRow final {std::string label,value;friend bool operator==(const CardRow&,const CardRow&)=default;};
struct CardPalette final {WidgetColor surface{},hover{},selected{},border{},selected_border{},text{},secondary{},disabled_text{},progress{},progress_track{},status{},focus{};
 friend bool operator==(const CardPalette&,const CardPalette&)=default;};
struct CardConfiguration final {CardType type=CardType::file;CardSize size=CardSize::medium;CardDetail detail=CardDetail::brief;CardDisplayState display_state=CardDisplayState::automatic;
 std::string title,description,details,metadata,status_text,summary,footnote,domain,preview_title;std::string progress_text,menu_accessibility_name="More actions",icon_name;
 std::optional<std::string> action_text,progress_label;std::vector<CardRow> rows;std::optional<bool> selectable,show_menu,show_action;
 bool selected=false,enabled=true,visible=true,show_description=true,show_status=true;double width=0,height=0,progress=0,padding=18;
 std::optional<double> corner_radius;CardPalette palette;friend bool operator==(const CardConfiguration&,const CardConfiguration&)=default;};
struct CardExtent final {double width=0,height=0;};struct CardRect final {double x=0,y=0,width=0,height=0;};
struct CardRowGeometry final {std::size_t index=0;CardRect label,value;};
struct CardGeometry final {CardRect surface,preview,icon,title,description,details,metadata,status,summary,footnote,domain;
 CardRect progress_label,progress_text,progress_track,progress_fill,action,menu;std::vector<CardRowGeometry> rows;};
struct CardInteraction final {bool hovered=false,pressed=false,focused=false;};
struct CardVisual final {bool selected=false,hovered=false,enabled=true,focused=false;WidgetColor surface,border,text,secondary,focus;};
struct DefaultCardModelPolicy final {using Configuration=CardConfiguration;using Type=CardType;using Size=CardSize;using Detail=CardDetail;using DisplayState=CardDisplayState;
 using Extent=CardExtent;using Rect=CardRect;using Geometry=CardGeometry;using Interaction=CardInteraction;using Visual=CardVisual;using Color=WidgetColor;
 [[nodiscard]] static bool valid_text(std::string_view value)noexcept{return DefaultButtonPolicy::valid_text(value);}
 [[nodiscard]] static std::string_view action_label(CardType)noexcept{return {};}
 [[nodiscard]] static std::string_view progress_label(CardType)noexcept{return {};}};
// This model holds visual decisions and bounded content; apps supply images,
// actions and data independently of the widget host.
template<class Policy>class BasicCard final {
    using CardConfiguration=typename Policy::Configuration;using CardType=typename Policy::Type;using CardSize=typename Policy::Size;
    using CardDetail=typename Policy::Detail;using CardDisplayState=typename Policy::DisplayState;using CardExtent=typename Policy::Extent;
    using CardRect=typename Policy::Rect;using CardGeometry=typename Policy::Geometry;using CardInteraction=typename Policy::Interaction;
    using CardVisual=typename Policy::Visual;using StyleColor=typename Policy::Color;
public:
    [[nodiscard]] static bool validate(const CardConfiguration& c) noexcept {
        const auto number = [](double n,double max){ return std::isfinite(n) && n >= 0 && n <= max; };
        const auto color = [&](StyleColor v){ return number(v.red,1) && number(v.green,1) && number(v.blue,1) && number(v.alpha,1); };
        const auto type = static_cast<int>(c.type), size = static_cast<int>(c.size), detail = static_cast<int>(c.detail), state = static_cast<int>(c.display_state);
        if (type < 0 || type > 7 || size < 0 || size > 2 || detail < 0 || detail > 1 || state < 0 || state > 3 ||
            !number(c.width,8192) || !number(c.height,8192) || !number(c.progress,1) || !number(c.padding,8192) ||
            (c.corner_radius && !number(*c.corner_radius,8192)) || c.rows.size() > 128 || c.icon_name.size() > 256) return false;
        const auto& p = c.palette;
        if (!color(p.surface) || !color(p.hover) || !color(p.selected) || !color(p.border) || !color(p.selected_border) ||
            !color(p.text) || !color(p.secondary) || !color(p.disabled_text) || !color(p.progress) || !color(p.progress_track) ||
            !color(p.status) || !color(p.focus)) return false;
        std::size_t bytes = 0;
        const auto text = [&](std::string_view value) { if (!Policy::valid_text(value) || value.size() > 2*1024*1024 - bytes) return false;
            bytes += value.size(); return true; };
        if (!text(c.title) || !text(c.description) || !text(c.details) || !text(c.metadata) || !text(c.status_text) || !text(c.summary) ||
            !text(c.footnote) || !text(c.domain) || !text(c.preview_title) || !text(c.progress_text) || !text(c.menu_accessibility_name) ||
            !text(c.icon_name) || (c.action_text && !text(*c.action_text)) || (c.progress_label && !text(*c.progress_label))) return false;
        for (const auto& row : c.rows) if (!text(row.label) || !text(row.value)) return false;
        return true;
    }
    bool set_configuration(CardConfiguration value) {
        if (!validate(value)) return false; configuration_ = std::move(value); return true;
    }
    [[nodiscard]] const CardConfiguration& configuration() const noexcept { return configuration_; }
    [[nodiscard]] bool can_select() const noexcept { return configuration_.selectable.value_or(configuration_.type != CardType::file_preview); }
    [[nodiscard]] bool shows_menu() const noexcept {
        return configuration_.show_menu.value_or(configuration_.type != CardType::file && configuration_.type != CardType::file_preview);
    }
    [[nodiscard]] bool shows_action() const noexcept {
        return configuration_.show_action.value_or(configuration_.type != CardType::file && configuration_.type != CardType::file_preview);
    }
    [[nodiscard]] bool shows_progress() const noexcept { return configuration_.type == CardType::project || configuration_.type == CardType::device; }
    [[nodiscard]] std::string_view action_text() const noexcept {
        if (configuration_.action_text) return *configuration_.action_text;
        return Policy::action_label(configuration_.type);
    }
    [[nodiscard]] std::string_view progress_label() const noexcept {
        if (configuration_.progress_label) return *configuration_.progress_label;
        return Policy::progress_label(configuration_.type);
    }
    [[nodiscard]] CardExtent intrinsic_size() const noexcept {
        const auto& c = configuration_; double width = 256, height = (std::max)(280., 188. + 22. * double(c.rows.size()));
        if (c.type == CardType::file_preview) { width = 480; height = 320; }
        else if (c.type == CardType::file) {
            if (c.size == CardSize::small) { width = 140; height = 160; }
            else if (c.size == CardSize::large) { width = 360; height = 280; }
            else { width = 210; height = 240; }
        }
        return {c.width > 0 ? c.width : width, c.height > 0 ? c.height : (std::min)(height,8192.)};
    }
    [[nodiscard]] double resolved_corner_radius() const noexcept {
        const auto& c=configuration_;
        return c.corner_radius.value_or(c.type==CardType::file || c.type==CardType::file_preview ? 12. : 0.);
    }
    [[nodiscard]] CardVisual visual(CardInteraction input) const noexcept {
        const auto& c = configuration_; const bool preview = c.type == CardType::file_preview;
        const bool selected = !preview && (c.display_state == CardDisplayState::automatic ? c.selected : c.display_state == CardDisplayState::selected);
        const bool hovered = !preview && (c.display_state == CardDisplayState::automatic ? c.enabled && (input.hovered || input.focused) : c.display_state == CardDisplayState::hover);
        const auto& p = c.palette;
        return {selected,hovered,c.enabled,c.enabled && input.focused,
                c.type==CardType::file || c.type==CardType::file_preview ? (selected?p.selected:hovered?p.hover:p.surface) : StyleColor{0,0,0,0},
                selected?p.selected_border:p.border,c.enabled?p.text:p.disabled_text,c.enabled?p.secondary:p.disabled_text,p.focus};
    }
    [[nodiscard]] std::optional<bool> toggle_selection() noexcept {
        if (!configuration_.enabled || !configuration_.visible || !can_select()) return std::nullopt;
        configuration_.selected = !configuration_.selected; return configuration_.selected;
    }
    [[nodiscard]] std::optional<CardGeometry> geometry(double width,double height) const {
        if (!std::isfinite(width) || !std::isfinite(height) || width < 0 || height < 0 || width > 8192 || height > 8192) return std::nullopt;
        const auto& c = configuration_; CardGeometry g; g.surface = {0,0,width,height};
        const auto inset = (std::min)({c.padding,width/2,height/2}); const auto left=inset,right=width-inset,top=inset,bottom=height-inset;
        const auto available = (std::max)(0.,right-left);
        if (c.type == CardType::file || c.type == CardType::file_preview) {
            auto end=bottom;
            if (shows_menu()) {
                const auto menu_size=(std::min)({24.,available,(std::max)(0.,bottom-top)});
                g.menu={right-menu_size,top,menu_size,menu_size};
            }
            if (shows_action()) {
                const auto action_height=(std::min)(28.,(std::max)(0.,end-top));
                g.action={left,end-action_height,available,action_height};
                end=(std::max)(top,end-action_height-(action_height>0?8.:0.));
            }
            const auto& title=c.type==CardType::file_preview?c.preview_title:c.title;
            const auto title_height=title.empty()?0.:(std::min)(20.,(std::max)(0.,end-top));
            g.title={left,end-title_height,available,title_height};
            if(c.type==CardType::file_preview)g.preview=g.surface;
            else {
                const auto preview_bottom=(std::max)(top,end-title_height-(title_height>0?8.:0.));
                g.preview={left,top,available,preview_bottom-top};
                const auto icon=(std::min)({48.,g.preview.width,g.preview.height});
                g.icon={left+(available-icon)/2,top+(g.preview.height-icon)/2,icon,icon};
            }
            return g;
        }
        auto end = bottom;
        if (shows_action()) {
            const auto action_height = (std::min)(28.,(std::max)(0.,end-top));
            g.action = {left,end-action_height,available,action_height};
            end = (std::max)(top,end-action_height-(action_height>0?8.:0.));
        }
        double y = top;
        auto line = [&](double desired, double gap = 4.) {
            const auto h = (std::min)(desired,(std::max)(0.,end-y)); CardRect r{left,y,available,h};
            y = (std::min)(end,y+h+(h>0?gap:0.)); return r;
        };
        const auto header = line(40,8); const auto icon = (std::min)({40.,header.width,header.height});
        g.icon = {left,header.y,icon,icon};
        if (shows_menu()) {
            const auto menu_size = (std::min)({24.,header.width,header.height});
            g.menu = {right-menu_size,header.y,menu_size,menu_size};
        }
        if (!c.title.empty()) g.title = line(18,4);
        if (c.show_description && !c.description.empty()) g.description = line(28,4);
        if (c.detail == CardDetail::detailed && !c.details.empty()) g.details = line(16,4);
        if (!c.metadata.empty()) g.metadata = line(16,4);
        if (c.show_status && !c.status_text.empty()) g.status = line(16,4);
        if (!c.summary.empty()) g.summary = line(16,4);
        if (!c.domain.empty()) g.domain = line(16,4);
        if (!c.footnote.empty()) g.footnote = line(16,4);
        for (std::size_t index=0; index<c.rows.size(); ++index) {
            const auto row = line(18,2); if (row.height <= 0) break;
            const auto half = row.width / 2; g.rows.push_back({index,{row.x,row.y,half,row.height},{row.x+half,row.y,row.width-half,row.height}});
        }
        if (shows_progress()) {
            const auto labels = line(16,4); const auto half = labels.width/2;
            g.progress_label = {labels.x,labels.y,half,labels.height}; g.progress_text = {labels.x+half,labels.y,labels.width-half,labels.height};
            g.progress_track = line(4,0); g.progress_fill = g.progress_track; g.progress_fill.width *= c.progress;
        }
        return g;
    }
private:
    CardConfiguration configuration_;
};

using CardModel=BasicCard<DefaultCardModelPolicy>;
} // namespace ii::gui
