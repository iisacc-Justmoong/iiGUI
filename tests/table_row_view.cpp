#include <iiGUI/NativeTableRowView.h>
#include <iiGUI/WidgetState.h>
#include <memory>
#include <optional>
#include <stdexcept>
#include <vector>
namespace {
using namespace ii::gui;void check(bool v,const char* m){if(!v)throw std::runtime_error(m);}
template<class T>T take(ii::core::Result<T> r){if(!r)throw std::runtime_error(r.error().message);return std::move(*r);}void good(ii::core::Result<void> r){if(!r)throw std::runtime_error(r.error().message);}
struct Value{std::string text="text",value_type="text";bool enabled=true,editable=true;friend bool operator==(const Value&,const Value&)=default;};
struct RowCell{std::string key;Value value;std::shared_ptr<const int> validator;bool selected=false,current=false;friend bool operator==(const RowCell&,const RowCell&)=default;};
struct Config{std::vector<RowCell> cells{{"a",{}},{"b",{}},{"c",{}}};double width=180,cell_width=100,cell_height=24,content_spacing=8,font_size=13;bool inputable=true,enabled=true,visible=true,show_divider=true;int palette=0;friend bool operator==(const Config&,const Config&)=default;};
struct Input{Config configuration;double offset=0;friend bool operator==(const Input&,const Input&)=default;};
struct Callbacks{std::function<void(std::size_t,std::string)> edited,submitted;std::function<void(std::size_t,std::string,std::string)> rejected;};
struct Slot{std::size_t index;double x,width;};struct Frame{std::vector<Slot> cells;};
struct Model{Config c;bool set_configuration(Config v){c=std::move(v);return true;}std::optional<Frame> layout(double width,double offset)const{Frame f;for(std::size_t i=0;i<c.cells.size();++i){double x=i*c.cell_width-offset;if(x+c.cell_width>0&&x<width)f.cells.push_back({i,x,c.cell_width});}return f;}};
struct CellConfig{std::string text,value_type;double width,height,padding,font_size;int palette;bool enabled,inputable,selected,current,show_divider;};
struct CellInput{CellConfig configuration;std::shared_ptr<const int> validator;};
struct CellCallbacks{std::function<void(std::string)> edited,submitted;std::function<void(std::string,std::string)> rejected;};
struct Cell{Widget widget;static ii::core::Result<Cell> create(WidgetBuilder& b,Widget parent,std::string_view id,CellInput,CellCallbacks cb){auto root=b.action_surface(parent,id,{},[cb]{if(cb.edited)cb.edited("edit");});if(!root)return std::unexpected(root.error());return Cell{*root};}
 Widget root()const{return widget;}ii::core::Result<void> set_options(CellInput){return {};}};
struct Policy{using Model=::Model;using Cell=::Cell;using Input=::Input;using CellInput=::CellInput;using Callbacks=::Callbacks;};
}
int main(){using View=ii::gui::BasicNativeTableRow<Policy>;auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task t){t();});auto host=take(tree->content_host({},"host",{180,24}));std::optional<View> row;auto owner=std::make_shared<int>(1);std::weak_ptr<int> weak=owner;std::size_t index=99;
 good(host.set_content([&](WidgetBuilder& b)->ii::core::Result<void>{Callbacks cb;cb.edited=[&,held=owner](std::size_t i,std::string){index=i;};auto v=View::create(b,{},"row",{},std::move(cb));if(!v)return std::unexpected(v.error());row=*v;return {};}));owner.reset();
 (void)take(tree->drawing_snapshot({180,24}));auto cell=take(row->cell("b"));good(cell.root().activate());check(index==1,"Row event index lost");
 auto next=take(row->options());std::swap(next.configuration.cells[0],next.configuration.cells[1]);good(row->set_options(next));(void)take(tree->drawing_snapshot({180,24}));
 index=99;cell=take(row->cell("b"));good(cell.root().activate());check(index==0,"Row index did not follow reordered key");
 good(host.clear_content());check(weak.expired()&&!row->options(),"Retired row retained callbacks");tree->close();}
