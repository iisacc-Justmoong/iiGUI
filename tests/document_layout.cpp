#include <iiGUI/DocumentLayout.h>
#include <concepts>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>
namespace example {
struct Color {float red=0,green=0,blue=0,alpha=1;};enum class Unit {automatic,pixels,percent};
struct Length {Unit unit=Unit::automatic;float value=0;};enum class Direction {column,row};enum class Alignment {left,center,right};
struct Insets {float top=0,right=0,bottom=0,left=0;};struct Style {Length width,height;Insets padding;float gap=0,radius=0;Direction direction=Direction::column;
 std::optional<Color> background,color;std::optional<float> font_size;std::optional<unsigned> font_weight;std::optional<Alignment> text_align;};
enum class Kind {container,text};struct Node {Kind kind=Kind::container;std::string tag,id,text;Style style;std::vector<Node> children;};
struct Document {Node root_;const Node& root()const{return root_;}};struct Policy {using Document=example::Document;using Node=example::Node;using Color=example::Color;using Length=example::Length;using Unit=example::Unit;using Direction=example::Direction;using Alignment=example::Alignment;using Kind=example::Kind;};
}
namespace {void check(bool v,const char* m){if(!v)throw std::runtime_error(m);}}
int main(){using namespace example;Document d;d.root_.tag="view";d.root_.style.padding={10,10,10,10};
 d.root_.style.direction=Direction::row;d.root_.style.gap=10;Node a;a.id="a";a.style.width={Unit::pixels,40};Node b;b.id="b";d.root_.children={a,b};
 auto layout=ii::gui::layout_document<Policy>(d,{200,100});check(layout&&layout->elements.size()==2,"Document layout failed");
 check(layout->elements[0].bounds.x==10&&layout->elements[0].bounds.width==40&&layout->elements[1].bounds.x==60&&layout->elements[1].bounds.width==130,"Document row geometry changed");
 check(!ii::gui::layout_document<Policy>(d,{0,100}),"Invalid viewport was accepted");
 static_assert(requires(ii::gui::Host& h, const Document& value){{h.set_view(ii::gui::DocumentView<Policy>{value})}->std::same_as<ii::core::Result<void>>;});}
