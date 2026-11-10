#pragma once
#include <iiGUI/Image.h>
#include <iiGUI/NativeWidgets.h>
#include <iiGUI/Text.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <exception>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
namespace ii::gui {
// A retained header presentation resolves resources once and draws clipped cells.
template<class Policy>
class BasicTableHeaderPresentation final {
    using TableColumn=typename Policy::Column;using TableHeaderPresentation=typename Policy::Appearance;
    using TableHeaderColumnAppearance=typename Policy::ColumnAppearance;using IconSource=typename Policy::IconSource;
    using IconCache=typename Policy::IconCache;
public:
    struct Column final {
        double padding=0;
        std::string leading,trailing;
        std::shared_ptr<const ii::gui::RasterImage> leading_image,trailing_image;
    };
    using Snapshot=std::shared_ptr<const BasicTableHeaderPresentation>;

    [[nodiscard]] static ii::core::Result<Snapshot> prepare(
            std::span<const TableColumn> columns,const TableHeaderPresentation& input,double default_padding,
            std::shared_ptr<const IconSource> source,std::int64_t icon_revision,Snapshot previous={}){
        try{
            if(!Policy::valid_appearance(input)||!std::isfinite(default_padding)||default_padding<0||default_padding>8192||
               icon_revision<0||icon_revision>9007199254740991LL||columns.size()>Policy::max_columns)
                return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid table header presentation");
            auto result=std::make_shared<BasicTableHeaderPresentation>();result->appearance_=input;
            // Providers may synchronously replace the caller's authored options. Keep
            // all keys and settings in this candidate before invoking one.
            const auto& appearance=result->appearance_;
            std::unordered_map<std::string_view,const TableHeaderColumnAppearance*> overrides;overrides.reserve(appearance.column_appearance.size());
            std::unordered_set<std::string> names;
            for(const auto& item:appearance.column_appearance){
                overrides.emplace(item.column,&item);
                // Pending keys are retained across column removal; validate their explicitly authored assets now.
                if(appearance.icon_size>0){if(item.leading_icon&&!item.leading_icon->empty())names.insert(*item.leading_icon);
                    if(item.trailing_icon&&!item.trailing_icon->empty())names.insert(*item.trailing_icon);}
            }
            result->columns_.reserve(columns.size());
            for(const auto& column:columns){
                Column style{.padding=default_padding,.leading=appearance.default_leading_icon,.trailing=appearance.default_trailing_icon};
                if(const auto item=overrides.find(column.key);item!=overrides.end()){
                    const auto& value=*item->second;if(value.padding)style.padding=*value.padding;
                    if(value.leading_icon)style.leading=*value.leading_icon;if(value.trailing_icon)style.trailing=*value.trailing_icon;
                }
                if(appearance.icon_size>0){if(!style.leading.empty())names.insert(style.leading);if(!style.trailing.empty())names.insert(style.trailing);}
                result->columns_.push_back(std::move(style));
            }
            const auto pixels=std::uint64_t(std::ceil(appearance.icon_size*2));
            constexpr std::uint64_t raster_budget=64ull*1024*1024;
            if(pixels&&names.size()>raster_budget/(pixels*pixels*4))
                return ii::core::fail(ii::core::ErrorCode::out_of_range,"Table header icon raster budget exceeded");
            if(names.empty())return Snapshot(std::move(result));
            std::unordered_map<std::string,std::shared_ptr<const ii::gui::RasterImage>> images;images.reserve(names.size());
            for(unsigned attempt=0;attempt<4;++attempt){
                const auto revision=source?source->revision():0;result->icon_caches_.clear();images.clear();
                std::optional<ii::core::Error> failure;
                for(const auto& name:names){
                    if(previous)if(const auto old=previous->icon_caches_.find(name);old!=previous->icon_caches_.end())result->icon_caches_.emplace(name,old->second);
                    auto [cache,inserted]=result->icon_caches_.try_emplace(name);(void)inserted;
                    auto image=cache->second.resolve(name,source,appearance.icon_size);
                    if(!image){failure=image.error();break;}images.emplace(name,std::move(*image));
                }
                if(source&&source->revision()!=revision)continue;
                if(failure)return std::unexpected(*failure);
                for(auto& column:result->columns_){
                    if(!column.leading.empty()&&appearance.icon_size>0)column.leading_image=images.at(column.leading);
                    if(!column.trailing.empty()&&appearance.icon_size>0)column.trailing_image=images.at(column.trailing);
                }
                return Snapshot(std::move(result));
            }
            return ii::core::fail(ii::core::ErrorCode::busy,"Table header icon source changed repeatedly");
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Table header presentation preparation failed",std::current_exception());}
    }

    // Draw one clipped cell without reading a widget or invoking an icon provider.
    [[nodiscard]] ii::core::Result<void> draw(ii::gui::WidgetDrawing& drawing,std::size_t index,std::string_view text,
            double offset,double extent,double row_height,const ii::gui::WidgetPaintState& view,ii::gui::Color ink,
            double font_size,double line_height,double indicator,std::size_t budget)const{
        try{
            if(index>=columns_.size())return ii::core::fail(ii::core::ErrorCode::out_of_range,"Table header presentation index is stale");
            const auto& style=columns_[index];const auto left=(std::max)(0.,offset),right=(std::min)(view.size.width,offset+extent);
            if(right<=left)return {};const auto pad=(std::min)(style.padding,extent/2);
            const auto available=(std::max)(0.,extent-2*pad-indicator),content_x=offset+pad;
            const auto leading=style.leading_image?(std::min)(appearance_.icon_size,available):0.;
            const auto leading_gap=leading>0?(std::min)(appearance_.icon_spacing,(std::max)(0.,available-leading)):0.;
            const auto trailing=style.trailing_image?(std::min)(appearance_.icon_size,(std::max)(0.,available-leading-leading_gap)):0.;
            const auto trailing_gap=trailing>0?(std::min)(appearance_.icon_spacing,(std::max)(0.,available-leading-leading_gap-trailing)):0.;
            const auto text_available=(std::max)(0.,available-leading-leading_gap-trailing-trailing_gap);
            const auto title=Policy::display_text(text,budget);const auto text_x=content_x+leading+leading_gap;
            auto text_width=text_available;
            if(trailing>0&&!title.empty()&&text_available>0){
                auto measured=ii::gui::measure_text_layout({.text=title,.color=ink,.font_size=float(font_size),.font_weight=500,
                    .font_family=view.font_family,.line_height=float(line_height)});
                if(!measured)return std::unexpected(measured.error());text_width=(std::min)(text_available,measured->width);
            }else if(title.empty())text_width=0;
            const auto icon=[&](const std::shared_ptr<const ii::gui::RasterImage>& image,double x,double slot){
                if(!image||slot<=0||appearance_.icon_size<=0)return;
                const auto y=(row_height-appearance_.icon_size)/2;
                const auto x0=(std::max)(left,x),x1=(std::min)(right,x+slot);
                const auto y0=(std::max)(0.,y),y1=(std::min)({view.size.height,row_height,y+appearance_.icon_size});
                if(x1<=x0||y1<=y0)return;
                drawing.images.push_back({.bounds={float(x0),float(y0),float(x1-x0),float(y1-y0)},.image=image,
                    .source={float((x0-x)/appearance_.icon_size),float((y0-y)/appearance_.icon_size),
                             float((x1-x0)/appearance_.icon_size),float((y1-y0)/appearance_.icon_size)}});
            };
            icon(style.leading_image,content_x,leading);icon(style.trailing_image,text_x+text_width+trailing_gap,trailing);
            if(!title.empty()&&text_width>0)drawing.texts.push_back({.bounds={float(text_x),0,float(text_width),float((std::min)(row_height,view.size.height))},
                .text=title,.color=ink,.font_size=float(font_size),.font_weight=500,.alignment=ii::gui::TextAlignment::left,
                .font_family=view.font_family,.line_height=float(line_height),.elide_right=true,
                .clip=ii::gui::Rect{float(left),0,float(right-left),float((std::min)(row_height,view.size.height))}});
            return {};
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Table header content drawing failed",std::current_exception());}
    }
private:
    TableHeaderPresentation appearance_;
    std::vector<Column> columns_;
    std::unordered_map<std::string,IconCache> icon_caches_;
};

} // namespace ii::gui
