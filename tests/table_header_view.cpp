#include <iiGUI/NativeTableHeaderView.h>
#include <iiGUI/WidgetState.h>
#include <expected>
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>
namespace {
using namespace ii::gui;
void check(bool v,const char* m){if(!v)throw std::runtime_error(m);}
template<class T>T take(ii::core::Result<T> r){if(!r)throw std::runtime_error(r.error().message);return std::move(*r);}
void good(ii::core::Result<void> r){if(!r)throw std::runtime_error(r.error().message);}
struct Color{double red=.5,green=.5,blue=.5,alpha=1;friend bool operator==(const Color&,const Color&)=default;};
struct Palette{Color background,divider,header_text,current;friend bool operator==(const Palette&,const Palette&)=default;};
enum class SortOrder{ascending,descending};struct Sort{std::string column;SortOrder order=SortOrder::ascending;friend bool operator==(const Sort&,const Sort&)=default;};
struct Column{std::string key,title;bool enabled=true;friend bool operator==(const Column&,const Column&)=default;};
struct Config{std::vector<Column> columns{{"a","A"},{"b","B"}};double width=200,row_height=24,separator_height=1,horizontal_padding=8;bool interactive=true,enabled=true,visible=true;Palette palette;std::optional<Sort> sort;friend bool operator==(const Config&,const Config&)=default;};
struct Text{std::string header_group="Header",unnamed_column="Column",sorted_ascending="ascending",sorted_descending="descending";friend bool operator==(const Text&,const Text&)=default;};
struct Input{Config configuration;double offset=0;Text text;std::shared_ptr<const void> icon_source;std::int64_t icon_revision=0;friend bool operator==(const Input&,const Input&)=default;};
struct Error{std::string message;};template<class T>using Expected=std::expected<T,Error>;
struct Span{std::size_t index;double offset,extent;};struct Source;struct Frame{std::shared_ptr<const Source> source;std::vector<Span> columns;double width=0,x=0;};
struct Request{double height=0,header_height=0,x=0;std::size_t overscan_rows=0,overscan_columns=0;std::string reveal_column;};
struct Source:std::enable_shared_from_this<Source>{Config config;double width;Expected<Frame> frame(Request r)const{Frame f{shared_from_this(),{},width,r.x};for(std::size_t i=0;i<config.columns.size();++i)f.columns.push_back({i,double(i)*100-r.x,100});return f;}};
struct Model{Config config;bool set_configuration(const Config& c){if(c.width<1)return false;config=c;return true;}Expected<Frame> layout(double width,double x)const{auto source=std::make_shared<Source>();source->config=config;source->width=width;return source->frame({.x=x});}std::optional<std::size_t> hit(const Frame& f,double x)const{for(const auto& s:f.columns)if(x>=s.offset&&x<s.offset+s.extent&&config.columns[s.index].enabled)return s.index;return {};}};
struct ViewSnapshot{bool header=false;std::string column_key;std::size_t column_index=0;std::string text;bool enabled=false,sorted=false,descending=false;double x=0,width=0,height=0;};
struct ViewItem{std::string key;ViewSnapshot value;Rect frame;};using Factory=std::function<ii::core::Result<void>(WidgetBuilder&,std::shared_ptr<const void>)>;
struct Views{Factory header,cell;std::map<std::string,Factory,std::less<>> headers,columns;};
struct Group{Group(Widget,Factory,std::map<std::string,Factory,std::less<>>){}bool has(const std::string&)const{return false;}ii::core::Result<void> update(const std::vector<ViewItem>&,Size){return {};}};
struct Presentation{using Snapshot=std::shared_ptr<const Presentation>;inline static std::vector<std::weak_ptr<int>> retained;std::shared_ptr<int> marker=std::make_shared<int>(1);static ii::core::Result<Snapshot> prepare(const std::vector<Column>&,int,double,std::shared_ptr<const void>,std::int64_t,Snapshot){auto p=std::make_shared<Presentation>();retained.push_back(p->marker);return p;}ii::core::Result<void> draw(WidgetDrawing&,std::size_t,const std::string&,double,double,double,const WidgetPaintState&,ii::gui::Color,double,double,double,std::size_t)const{return {};}};
struct Callbacks{std::function<void(std::size_t,const Column&)> column_clicked;};
struct Policy{using Input=::Input;using Model=::Model;using Frame=::Frame;using Presentation=::Presentation;using ViewGroup=::Group;using Views=::Views;using ViewItem=::ViewItem;using ViewSnapshot=::ViewSnapshot;using Callbacks=::Callbacks;using SortOrder=::SortOrder;static bool valid_text(const Text&){return true;}static int appearance(const Config&){return 0;}static std::string unnamed_column(const std::string& s,const std::string& n){return s+n;}static std::string display_text(std::string_view s,std::size_t){return std::string(s);}static bool pointer_canceled(WidgetInputKind k){return k==WidgetInputKind::focus_lost;}};
}
int main(){using View=ii::gui::BasicNativeTableHeader<Policy>;auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task t){t();});auto host=take(tree->content_host({},"host",{200,30}));std::optional<View> view;auto owner=std::make_shared<int>(1);std::weak_ptr<int> weak=owner;
 good(host.set_content([&](WidgetBuilder& b)->ii::core::Result<void>{Callbacks callbacks;callbacks.column_clicked=[held=owner](std::size_t,const Column&){};auto v=View::create(b,{},"header",{},std::move(callbacks));if(!v)return std::unexpected(v.error());view=*v;return {};}));owner.reset();(void)take(tree->drawing_snapshot({200,30}));check(take(view->viewport()).columns.size()==2,"header frame missing");auto next=take(view->options());next.configuration.width=100;good(view->set_options(next));next.configuration.width=0;check(!view->set_options(next),"invalid header was accepted");good(host.clear_content());for(const auto& p:Presentation::retained)check(p.expired(),"header presentation retained after retirement");check(weak.expired()&&!view->options(),"header callback retained or remained live");tree->close();}
