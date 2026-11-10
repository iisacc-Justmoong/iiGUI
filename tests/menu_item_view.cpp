#include <iiGUI/NativeMenuItemView.h>
#include <iiGUI/WidgetState.h>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <vector>

namespace {
using namespace ii::gui;
using ii::core::Result;
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(Result<T> value){if(!value)throw std::runtime_error(value.error().message);return std::move(*value);}
void good(Result<void> value){if(!value)throw std::runtime_error(value.error().message);}
struct Input final {
    MenuItemConfiguration configuration;
    std::shared_ptr<const RasterImage> icon;
    std::shared_ptr<const void> icon_source;
    std::int64_t icon_revision=0;
    bool visible=true;
    std::optional<bool> close_on_trigger;
    friend bool operator==(const Input&,const Input&)=default;
};
struct Callbacks final {std::function<void()> clicked,pressed,released,canceled;};
struct Presentation final {std::optional<bool> selected,compact,show_icon_slot;std::optional<double> width;};
using Activation=std::function<void(std::function<void()>)>;
using CloseAction=std::function<Result<void>()>;
using ClosePlan=std::function<Result<CloseAction>()>;
enum class EntryKind {action,submenu};
struct Entry final {
    std::string key,label,shortcut,icon_name,accessibility_name;
    EntryKind kind=EntryKind::action;
    MenuItemState state=MenuItemState::normal;
    bool enabled=true,visible=true,compact=true;
    std::optional<double> height;
    std::optional<bool> close_on_trigger;
};
struct PopupPort final {
    std::function<Result<WidgetSnapshot>()> snapshot;
    std::function<Result<void>(Widget,MenuDirection,CloseAction)> open;
    std::function<Result<void>()> close;
};
struct ItemPort final {
    std::function<Result<void>(Presentation)> present;
    std::function<Result<void>(Activation)> activated;
    std::function<Result<void>(std::function<Result<void>()>)> changed;
    std::function<Result<Entry>()> entry;
    std::function<Result<WidgetSnapshot>()> snapshot;
    std::function<Result<void>()> focus;
    std::function<Result<bool>()> open_submenu;
    std::function<Result<void>(ClosePlan)> parent_close;
    std::function<bool()> retired;
    std::function<Result<bool>()> submenu_open;
    std::function<Result<void>()> close_submenu;
    std::function<Result<Widget>()> widget;
    std::vector<Widget> layout_groups;
};
struct IconCache final {
    Result<std::shared_ptr<const RasterImage>> resolve(std::string_view,std::shared_ptr<const void>,double){
        return ii::core::fail(ii::core::ErrorCode::not_found,"No test icon");
    }
};
struct Policy final {
    using Model=MenuItemModel;using Input=::Input;using Callbacks=::Callbacks;using Presentation=::Presentation;
    using Activation=::Activation;using CloseAction=::CloseAction;using ClosePlan=::ClosePlan;
    using PopupPort=::PopupPort;using ItemPort=::ItemPort;using Entry=::Entry;using EntryKind=::EntryKind;
    using EntryState=MenuItemState;using Direction=MenuDirection;using Rect=MenuItemRect;using Color=WidgetColor;using IconCache=::IconCache;
    static std::string_view chevron_name(Direction){return "chevron";}
};
}
int main(){
    using Item=ii::gui::BasicNativeMenuItem<Policy>;
    auto tree=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{320,100}));std::optional<Item> item;
    auto payload=std::make_shared<int>(7);std::weak_ptr<int> weak=payload;int clicks=0;
    good(host.set_content([&](auto& builder)->Result<void>{
        Input input;input.configuration.label="Action";input.configuration.icon_name.clear();
        input.configuration.show_icon_slot=false;input.configuration.show_chevron=false;
        auto created=Item::create(builder,{},"action",input,Callbacks{.clicked=[payload,&clicks]{++clicks;}});
        if(!created)return std::unexpected(created.error());item=*created;return {};
    }));
    payload.reset();require(!weak.expired(),"Live item released its callback");
    good(item->root().activate());require(clicks==1,"Action callback was not delivered");
    auto port=item->weak_group_port();auto entry=take(port.entry());
    require(entry.label=="Action"&&entry.kind==EntryKind::action,"Menu entry changed");
    auto invalid=take(item->options());invalid.configuration.icon_size=0;
    require(!item->set_options(invalid)&&take(item->options()).configuration.label=="Action","Invalid input replaced item");
    good(host.clear_content());require(weak.expired(),"Retired item retained callbacks");
    require(port.retired()&&!item->set_options(invalid),"Retired item accepted input");tree->close();
}
