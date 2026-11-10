#include <iiGUI/TableHeaderPresentation.h>
#include <iiGUI/Image.h>
#include <memory>
#include <stdexcept>
struct Source{mutable unsigned version=0;bool churn=false;unsigned revision()const{return version;}};
struct Column{std::string key;};
struct ColumnAppearance{std::string column;std::optional<double> padding;std::optional<std::string> leading_icon,trailing_icon;};
struct Appearance{std::string default_leading_icon,default_trailing_icon;double icon_size=18,icon_spacing=7;std::vector<ColumnAppearance> column_appearance;};
struct Cache{
    std::shared_ptr<const ii::gui::RasterImage> image;
    unsigned revision=0;
    ii::core::Result<std::shared_ptr<const ii::gui::RasterImage>> resolve(std::string_view,std::shared_ptr<const Source> source,double){
        if(source&&source->churn)++source->version;
        if(!image||(source&&revision!=source->version)){auto value=ii::gui::rgba_image(1,1,{255,255,255,255});if(!value)return std::unexpected(value.error());image=*value;revision=source?source->version:0;}return image;
    }
};
struct Policy{
    using Column=::Column;using Appearance=::Appearance;using ColumnAppearance=::ColumnAppearance;using IconSource=Source;using IconCache=Cache;
    static constexpr std::size_t max_columns=1024;
    static bool valid_appearance(const Appearance& value){return value.icon_size>=0&&value.icon_size<=256;}
    static std::string display_text(std::string_view value,std::size_t budget){return std::string(value.substr(0,budget));}
};
void require(bool v,const char* m){if(!v)throw std::runtime_error(m);}
template<class T>T take(ii::core::Result<T> r){if(!r)throw std::runtime_error(r.error().message);return std::move(*r);}
int main(){
    using View=ii::gui::BasicTableHeaderPresentation<Policy>;
    std::vector<Column> columns{{"first"},{"second"}};Appearance appearance;appearance.default_leading_icon="icon";
    auto source=std::make_shared<Source>();auto initial=take(View::prepare(columns,appearance,4,source,1));
    ii::gui::WidgetDrawing drawing;ii::gui::WidgetPaintState state;state.size={100,24};
    auto result=initial->draw(drawing,0,"Label",0,50,24,state,{1,1,1,1},13,16,0,64);
    require(bool(result)&&drawing.images.size()==1&&drawing.texts.size()==1,"header did not draw icon and text");
    auto next=take(View::prepare(columns,appearance,4,source,1,initial));ii::gui::WidgetDrawing cached;
    require(bool(next->draw(cached,0,"Label",0,50,24,state,{1,1,1,1},13,16,0,64))&&
            cached.images[0].image==drawing.images[0].image,"header lost cached icon");
    // A binding revision requests reconsideration; unchanged providers retain their image.
    auto refreshed=take(View::prepare(columns,appearance,4,source,2,next));ii::gui::WidgetDrawing updated;
    require(bool(refreshed->draw(updated,0,"Label",0,50,24,state,{1,1,1,1},13,16,0,64))&&
            updated.images[0].image==cached.images[0].image,"unchanged provider discarded its cached image");
    ++source->version;auto replaced=take(View::prepare(columns,appearance,4,source,3,refreshed));ii::gui::WidgetDrawing changed;
    require(bool(replaced->draw(changed,0,"Label",0,50,24,state,{1,1,1,1},13,16,0,64))&&
            changed.images[0].image!=updated.images[0].image,"changed provider reused stale image");
    auto invalid=appearance;invalid.icon_size=-1;require(!View::prepare(columns,invalid,4,source,1,next),"invalid appearance accepted");
    source->churn=true;require(!View::prepare(columns,appearance,4,source,2,next),"changing source did not stop retries");
    require(!next->draw(cached,2,"Bad",0,50,24,state,{1,1,1,1},13,16,0,64),"stale index accepted");
}
