#include <iiGUI/TableHeaderModel.h>
#include <cmath>
#include <expected>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>
namespace {
void check(bool value,const char* text){if(!value)throw std::runtime_error(text);}
struct Color{double red=0,green=0,blue=0,alpha=1;};struct Palette{Color background,divider,header_text,cell_text,selection,current;};
enum class SortOrder{ascending,descending};struct Sort{std::string column;SortOrder order=SortOrder::ascending;};struct Column{std::string key;bool enabled=true;};struct Appearance{std::string column;};
struct Configuration{std::vector<Column> columns{{"a"},{"b"}};double width=200,row_height=24,horizontal_padding=8,separator_height=1,minimum_column_width=32,fallback_column_width=0;bool fit_flexible_columns=true;Palette palette;std::optional<Sort> sort;std::vector<Appearance> column_appearance;};
struct Snapshot{std::vector<Column> columns;};enum class ErrorCode{invalid};struct Error{ErrorCode code;std::string message;};template<class T>using Result=std::expected<T,Error>;
struct Index{std::shared_ptr<const Snapshot> snapshot;static Result<Index> create(std::shared_ptr<const Snapshot> s){if(!s||s->columns.size()>8)return std::unexpected(Error{ErrorCode::invalid,"bad columns"});for(std::size_t i=0;i<s->columns.size();++i)for(std::size_t j=0;j<i;++j)if(s->columns[i].key==s->columns[j].key)return std::unexpected(Error{ErrorCode::invalid,"duplicate"});return Index{s};}std::optional<std::size_t> column(const std::string& key)const{for(std::size_t i=0;i<snapshot->columns.size();++i)if(snapshot->columns[i].key==key)return i;return {};}};
struct Span{std::size_t index;double offset,extent;};struct Frame{double width=0;std::vector<Span> columns;};struct ViewportConfiguration{double width,fallback_column_width,minimum_column_width;bool fit_flexible_columns;};struct Request{double height,header_height,x;std::size_t overscan_rows,overscan_columns;};
struct Source{std::shared_ptr<const Snapshot> snapshot;ViewportConfiguration config;static Result<std::shared_ptr<const Source>> create(std::shared_ptr<const Snapshot> s,ViewportConfiguration c){return std::make_shared<const Source>(Source{s,c});}Result<Frame> frame(Request r)const{Frame f{config.width,{}};double x=-r.x;for(std::size_t i=0;i<snapshot->columns.size();++i){f.columns.push_back({i,x,50});x+=50;}return f;}};
struct Policy{using Configuration=::Configuration;using Color=::Color;using Snapshot=::Snapshot;using Index=::Index;using ViewportSource=Source;using ViewportConfiguration=::ViewportConfiguration;using ViewportRequest=Request;using ViewportFrame=Frame;using Error=::Error;using ErrorCode=::ErrorCode;using SortOrder=::SortOrder;template<class T>using Result=::Result<T>;static bool valid_presentation(const Configuration&){return true;}};
}
int main(){ii::gui::BasicTableHeader<Policy> model;auto c=model.configuration();auto f=model.layout(100,0);check(f&&model.hit(*f,20)==0,"header hit failed");c.columns[1].enabled=false;check(model.set_configuration(c)&&!model.hit(*model.layout(100,0),70),"disabled column accepted");auto original=model.configuration();c.columns[1].key="a";check(!model.set_configuration(c)&&model.configuration().columns[1].key==original.columns[1].key,"duplicate replaced header");c=original;c.sort=Sort{"missing"};check(!model.set_configuration(c),"unknown sort accepted");check(!model.layout(-1,0)&&!model.layout(100,-1),"invalid viewport accepted");}
