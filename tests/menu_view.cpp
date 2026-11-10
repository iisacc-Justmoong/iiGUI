#include <iiGUI/NativeMenuView.h>
#include <iiGUI/WidgetState.h>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <vector>

namespace {
using namespace ii::gui;using ii::core::Result;
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(Result<T> value){if(!value)throw std::runtime_error(value.error().message);return std::move(*value);}
void good(Result<void> value){if(!value)throw std::runtime_error(value.error().message);}
struct Input final {MenuConfiguration configuration;int selected_index=-1;friend bool operator==(const Input&,const Input&)=default;};
struct Callbacks final {std::function<void(int)> triggered;std::function<void()> opened,closed,canceled;};
struct Presentation final {std::optional<bool> selected,compact,show_icon_slot;std::optional<double> width;
    friend bool operator==(const Presentation&,const Presentation&)=default;};
using Activation=std::function<void(std::function<void()>)>;
using CloseAction=std::function<Result<void>()>;using ClosePlan=std::function<Result<CloseAction>()>;
struct PopupPort final {std::function<Result<WidgetSnapshot>()> snapshot;
    std::function<Result<void>(Widget,MenuDirection,CloseAction)> open;std::function<Result<void>()> close;};
struct ItemPort final {
    std::function<Result<void>(Presentation)> present;std::function<Result<void>(Activation)> activated;
    std::function<Result<void>(std::function<Result<void>()>)> changed;std::function<Result<MenuEntry>()> entry;
    std::function<Result<WidgetSnapshot>()> snapshot;std::function<Result<void>()> focus;
    std::function<Result<bool>()> open_submenu;std::function<Result<void>(ClosePlan)> parent_close;
    std::function<bool()> retired;std::function<Result<bool>()> submenu_open;std::function<Result<void>()> close_submenu;
    std::function<Result<Widget>()> widget;std::vector<Widget> layout_groups;
};
using SectionPort=std::function<Result<void>(std::vector<ItemPort>)>;
using RegistrationPort=std::function<Result<void>(ItemPort)>;
struct SectionAccess final {std::function<Result<SectionPort>(Widget)> register_section;std::function<Result<Input>()> options;};
struct Policy final {
    using Model=MenuModel;using Input=::Input;using Callbacks=::Callbacks;using ItemPort=::ItemPort;using Presentation=::Presentation;
    using Activation=::Activation;using CloseAction=::CloseAction;using ClosePlan=::ClosePlan;using Entry=MenuEntry;
    using EntryKind=MenuEntryKind;using EntryState=MenuItemState;using Configuration=MenuConfiguration;using Layout=MenuLayout;
    using Color=WidgetColor;using Error=MenuError;using ErrorCode=MenuErrorCode;using Navigation=MenuNavigation;
    using Direction=MenuDirection;using PopupPort=::PopupPort;using SectionPort=::SectionPort;using SectionAccess=::SectionAccess;
    using RegistrationPort=::RegistrationPort;
};
}
int main(){
    using Menu=ii::gui::BasicNativeMenu<Policy>;
    auto tree=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{320,100}));std::optional<Menu> menu;
    auto payload=std::make_shared<int>(7);std::weak_ptr<int> weak=payload;int opened=0;
    good(host.set_content([&](auto& builder)->Result<void>{
        auto created=Menu::create(builder,{},"menu",{},Callbacks{.opened=[payload,&opened]{++opened;}});
        if(!created)return std::unexpected(created.error());menu=*created;return menu->finalize();
    }));
    payload.reset();require(!weak.expired(),"Live menu lost its callback");
    good(menu->root().open_at(0,0));require(opened==1,"Menu open was not delivered");
    auto bad=take(menu->options());bad.configuration.item_width=0;
    require(!menu->set_options(bad),"Invalid menu input was accepted");
    good(host.clear_content());require(weak.expired(),"Retired menu retained callbacks");
    require(!menu->set_options(bad),"Retired menu accepted input");tree->close();
}
