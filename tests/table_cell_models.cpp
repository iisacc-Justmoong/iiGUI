#include <iiGUI/TableCellItemModel.h>
#include <iiGUI/TableRowViewModel.h>
#include <iiGUI/NativeWidgets.h>
#include <iiSDKCore/Core/Text/Utf8.h>
#include <cmath>
#include <expected>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
namespace {
using namespace ii::gui;
void check(bool v,const char* m){if(!v)throw std::runtime_error(m);}
struct Palette{WidgetColor background{},divider{},header_text{},cell_text{},selection{},current{};};
struct CellConfig{std::string text="Text",value_type="string",accessibility_name;double width=234,height=24,padding=8,font_size=13,current_border_width=1;bool enabled=true,visible=true,inputable=false,selected=false,current=false,show_divider=true,clip_content=true;Palette palette;};
struct Rect{double x=0,y=0,width=0,height=0;};struct Geometry{Rect surface,text,divider;};struct Visual{bool selected=false,current=false;WidgetColor text,selection,border,divider;};
using Result=std::expected<void,std::string>;struct Validator{virtual ~Validator()=default;virtual Result validate(std::string_view,std::string_view)const=0;};
struct CellPolicy{using Configuration=CellConfig;using Color=WidgetColor;using Geometry=::Geometry;using Visual=::Visual;using Validator=::Validator;using Result=::Result;
 static bool valid_text(std::string_view v){return v.find('\0')==v.npos&&ii::core::valid_utf8(v);}static Result reject(std::string v){return std::unexpected(std::move(v));}};
using Cell=BasicTableCellItem<CellPolicy>;struct Value{std::string text="Text",value_type="text";};struct RowCell{std::string key;Value value;};
struct RowConfig{std::vector<RowCell> cells{{"a",{}},{"b",{}},{"c",{}}};double width=717,cell_width=234,cell_height=24,content_spacing=8,font_size=13;bool inputable=false,enabled=true,visible=true,show_divider=true;Palette palette;};
struct Slot{std::size_t index=0;double x=0,width=0;};struct Frame{std::vector<Slot> cells;double width=0,height=0,spacing=0,content_width=0,offset=0;};
struct RowPolicy{using Configuration=RowConfig;using CellConfiguration=CellConfig;using CellModel=Cell;using Frame=::Frame;static bool valid_text(std::string_view v){return CellPolicy::valid_text(v);}};
}
int main(){Cell c;auto cfg=c.configuration();cfg.value_type="int";check(c.set_configuration(cfg)&&c.accept("-14")&&!c.accept("1.5"),"Cell validation failed");
 auto g=c.geometry(234,24);check(g&&g->text.x==8&&g->text.width==218,"Cell geometry failed");BasicTableRowView<RowPolicy> r;auto frame=r.layout(300,300);check(frame&&frame->cells.size()==2&&frame->cells[0].index==1,"Visible table row layout failed");
 auto invalid=r.configuration();invalid.cells[1].key=invalid.cells[0].key;check(!r.set_configuration(invalid),"Duplicate keys accepted");}
