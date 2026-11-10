#include <iiGUI/HierarchyToolbarModel.h>
#include <iiSDKCore/Core/Text/Utf8.h>
#include <expected>
#include <memory>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>
namespace {void check(bool v,const char* m){if(!v)throw std::runtime_error(m);}using Id=std::variant<std::int64_t,std::string>;
enum class ErrorCode{invalid,not_found,wrong_thread,exhausted};struct Error{ErrorCode code;std::string message;};template<class T>using Result=std::expected<T,Error>;struct Command{};
struct Item{std::optional<Id> id;std::string icon_name,text,accessibility_name;bool enabled=true,visible=true,selected=false;std::shared_ptr<const Command> command;friend bool operator==(const Item&,const Item&)=default;};
struct Selection{Id id;std::size_t index=0;friend bool operator==(const Selection&,const Selection&)=default;};struct Snapshot{std::vector<Item> items;std::vector<Id> ids;std::optional<Selection> active;std::uint64_t revision=0;};
struct Activation{Selection selection;bool changed=false;std::shared_ptr<const Command> command;};enum class Direction{previous,next,first,last};
struct Policy{using Item=::Item;using Id=::Id;using Snapshot=::Snapshot;using Selection=::Selection;using Activation=::Activation;using Direction=::Direction;using Error=::Error;using ErrorCode=::ErrorCode;template<class T>using Result=::Result<T>;static bool valid_text(std::string_view v){return v.find('\0')==v.npos&&ii::core::valid_utf8(v);}};
}
int main(){ii::gui::BasicHierarchyToolbar<Policy> t;std::vector<Item> items(2);items[0].id=std::string("a");items[1].id=std::string("b");items[0].text="A";items[1].text="B";check(t.set_items(items)&&t.set_active(Id{std::string("b")}),"Toolbar selection failed");
 std::swap(items[0],items[1]);check(t.set_items(items)&&t.snapshot()->get()->active->index==0,"Toolbar index was not refreshed on reorder");check(t.set_active(Id{std::string("a")})&&t.snapshot()->get()->active->index==1,"Toolbar indexed lookup returned wrong item");}
