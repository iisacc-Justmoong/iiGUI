#pragma once
#include <iiGUI/NativeListItemSelectorView.h>
#include <iiGUI/NativeComboBoxView.h>
#include <iiGUI/NativeMenuView.h>
#include <iiGUI/NativeMenuItemView.h>
namespace selector_test {
using namespace ii::gui;using ii::core::Result;
struct MenuInput{MenuConfiguration configuration;int selected_index=-1;friend bool operator==(const MenuInput&,const MenuInput&)=default;};
struct MenuCallbacks{std::function<void(int)> triggered;std::function<void()> opened,closed,canceled;};
struct Presentation{std::optional<bool> selected,compact,show_icon_slot;std::optional<double> width;friend bool operator==(const Presentation&,const Presentation&)=default;};
using Activation=std::function<void(std::function<void()>)>;using CloseAction=std::function<Result<void>()>;using ClosePlan=std::function<Result<CloseAction>()>;
struct PopupPort{std::function<Result<WidgetSnapshot>()> snapshot;std::function<Result<void>(Widget,MenuDirection,CloseAction)> open;std::function<Result<void>()> close;};
struct ItemPort{std::function<Result<void>(Presentation)> present;std::function<Result<void>(Activation)> activated;
 std::function<Result<void>(std::function<Result<void>()>)> changed;std::function<Result<MenuEntry>()> entry;
 std::function<Result<WidgetSnapshot>()> snapshot;std::function<Result<void>()> focus;std::function<Result<bool>()> open_submenu;
 std::function<Result<void>(ClosePlan)> parent_close;std::function<bool()> retired;std::function<Result<bool>()> submenu_open;
 std::function<Result<void>()> close_submenu;std::function<Result<Widget>()> widget;std::vector<Widget> layout_groups;};
using SectionPort=std::function<Result<void>(std::vector<ItemPort>)>;using RegistrationPort=std::function<Result<void>(ItemPort)>;
struct SectionAccess{std::function<Result<SectionPort>(Widget)> register_section;std::function<Result<MenuInput>()> options;};
struct MenuPolicy {using Model=MenuModel;using Input=MenuInput;using Callbacks=MenuCallbacks;using ItemPort=selector_test::ItemPort;
 using Presentation=selector_test::Presentation;using Activation=selector_test::Activation;using CloseAction=selector_test::CloseAction;using ClosePlan=selector_test::ClosePlan;
 using Entry=MenuEntry;using EntryKind=MenuEntryKind;using EntryState=MenuItemState;using Configuration=MenuConfiguration;using Layout=MenuLayout;
 using Color=WidgetColor;using Error=MenuError;using ErrorCode=MenuErrorCode;using Navigation=MenuNavigation;using Direction=MenuDirection;
 using PopupPort=selector_test::PopupPort;using SectionPort=selector_test::SectionPort;using SectionAccess=selector_test::SectionAccess;using RegistrationPort=selector_test::RegistrationPort;};
struct MenuItemInput{MenuItemConfiguration configuration;std::shared_ptr<const RasterImage> icon;std::shared_ptr<const void> icon_source;
 std::int64_t icon_revision=0;bool visible=true;std::optional<bool> close_on_trigger;friend bool operator==(const MenuItemInput&,const MenuItemInput&)=default;};
struct ItemCallbacks{std::function<void()> clicked,pressed,released,canceled;};
struct IconCache{Result<std::shared_ptr<const RasterImage>> resolve(std::string_view,std::shared_ptr<const void>,double){return ii::core::fail(ii::core::ErrorCode::not_found,"No icon");}};
struct ItemPolicy{using Model=MenuItemModel;using Input=MenuItemInput;using Callbacks=ItemCallbacks;using Presentation=selector_test::Presentation;
 using Activation=selector_test::Activation;using CloseAction=selector_test::CloseAction;using ClosePlan=selector_test::ClosePlan;using PopupPort=selector_test::PopupPort;
 using ItemPort=selector_test::ItemPort;using Entry=MenuEntry;using EntryKind=MenuEntryKind;using EntryState=MenuItemState;using Direction=MenuDirection;
 using Rect=MenuItemRect;using Color=WidgetColor;using IconCache=selector_test::IconCache;static std::string_view chevron_name(Direction){return "chevron";}};
struct Input{SelectorConfiguration configuration;std::optional<int> accent;double width=97,height=20;bool visible=true;std::string accessibility_name;
 friend bool operator==(const Input&,const Input&)=default;};
struct HostPolicy{using Model=ListItemSelectorModel;using Input=selector_test::Input;using NativeComboBox=ii::gui::NativeComboBox;
 using NativeMenu=BasicNativeMenu<MenuPolicy>;using NativeMenuItem=BasicNativeMenuItem<ItemPolicy>;using MenuItemInput=selector_test::MenuItemInput;
 static bool valid_accent(const std::optional<int>&){return true;}static bool valid_text(std::string_view text){return DefaultButtonPolicy::valid_text(text);}
 static ComboBoxConfiguration control_input(const Input& in,const Model& model){ComboBoxConfiguration c;c.text=model.display_text();c.accessibility_name=in.accessibility_name;
  c.width=in.width;c.height=in.height;c.enabled=model.configuration().enabled;c.arrow=StepperArrow::down;return c;}
 static selector_test::MenuInput menu_input(const Input& in,const Model& model){selector_test::MenuInput v;v.configuration.item_width=in.width;v.configuration.show_icon_slot=false;
  v.configuration.accessibility_name=in.accessibility_name;const auto& c=model.configuration();v.selected_index=!c.options.empty()&&c.options[std::size_t(c.selected_index)].enabled?c.selected_index:-1;return v;}
 static void accent_item(MenuItemInput&,const std::optional<int>&){};};
}
