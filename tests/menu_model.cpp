#include <iiGUI/MenuModel.h>
#include <limits>
#include <stdexcept>
#include <vector>
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(ii::gui::MenuResult<T> value){if(!value)throw std::runtime_error(value.error().message);return std::move(*value);}
void good(ii::gui::MenuResult<void> value){if(!value)throw std::runtime_error(value.error().message);}
}
int main(){
    ii::gui::MenuModel menu;
    std::vector<ii::gui::MenuEntry> entries{{.key="open",.label="Open"},{.key="separator",.kind=ii::gui::MenuEntryKind::divider},
        {.key="disabled",.label="Disabled",.enabled=false},{.key="nested",.label="More",.kind=ii::gui::MenuEntryKind::submenu},
        {.key="hidden",.label="Hidden",.visible=false}};
    good(menu.set_entries(entries));auto geometry=menu.layout();
    require(geometry&&geometry->rows.size()==5&&geometry->rows[0].height==18&&geometry->rows[1].height==3&&
            geometry->rows[4].height==0&&geometry->width==157,"Menu layout or hidden entry handling is incorrect");
    auto custom=entries;custom[0].height=37;good(menu.set_entries(custom));
    require(menu.layout()->rows[0].height==37,"Custom menu item height was not included in the menu layout");
    custom[0].height=std::numeric_limits<double>::quiet_NaN();require(!menu.set_entries(custom),"Invalid menu item height was accepted");good(menu.set_entries(entries));
    custom=entries;custom[1].height=0;good(menu.set_entries(custom));require(menu.layout()->rows[1].height==0,"Zero-height divider used the default height");good(menu.set_entries(entries));
    require(!menu.navigate(ii::gui::MenuNavigation::next),"Closed menu accepted navigation");auto first_epoch=take(menu.open());
    require(take(menu.navigate(ii::gui::MenuNavigation::next))==0&&take(menu.navigate(ii::gui::MenuNavigation::next))==3&&
            take(menu.navigate(ii::gui::MenuNavigation::next))==0&&take(menu.navigate(ii::gui::MenuNavigation::last))==3,
            "Menu navigation did not skip noninteractive entries or wrap");
    require(!menu.select(1)&&!menu.select(2)&&!menu.prepare_activation(4),"Unavailable menu entries were accepted");
    auto submenu=take(menu.prepare_activation(3));auto forged=submenu;forged.close_on_complete=true;
    require(!menu.complete_activation(forged)&&menu.is_open(),"A modified activation policy closed a submenu");
    require(!submenu.close_on_complete&&!take(menu.complete_activation(submenu))&&menu.is_open()&&!menu.complete_activation(submenu),
            "Submenu activation closed its parent or was committed twice");
    auto plan=take(menu.prepare_activation(0));good(menu.select(0));
    require(!menu.complete_activation(plan)&&menu.is_open(),"Stale menu activation closed a modified menu");
    plan=take(menu.prepare_activation(0));good(menu.close());auto second_epoch=take(menu.open());
    require(second_epoch>first_epoch&&!menu.complete_activation(plan)&&menu.is_open(),"Old activation closed a reopened menu");
    plan=take(menu.prepare_activation(0));require(take(menu.complete_activation(plan))&&!menu.is_open(),"Current action did not close the menu");
    good(menu.set_entries(entries));good(menu.select(3));auto reordered=entries;std::swap(reordered[0],reordered[3]);good(menu.set_entries(reordered));
    require(menu.selected_index()==0&&menu.entries()[0].key=="nested","Menu selection did not follow its stable entry key");
    auto invalid=reordered;invalid[1].key="nested";require(!menu.set_entries(invalid)&&menu.entries()==reordered,"Duplicate key changed the menu");
    invalid=reordered;invalid[1].key=std::string("bad\0id",6);require(!menu.set_entries(invalid),"NUL in a menu key was accepted");
    auto config=menu.configuration();config.opacity=std::numeric_limits<double>::quiet_NaN();require(!menu.set_configuration(config),"Invalid menu opacity was accepted");
    require(!menu.set_entries(std::vector<ii::gui::MenuEntry>(4097)),"Menu entry limit was not enforced");
    config=menu.configuration();config.auto_close_on_trigger=false;good(menu.set_configuration(config));
    reordered[0].close_on_trigger=true;good(menu.set_entries(reordered));(void)take(menu.open());
    require(take(menu.prepare_activation(0)).close_on_complete,"Item close policy did not override menu policy");
    auto all_disabled=reordered;for(auto& entry:all_disabled)entry.enabled=false;good(menu.set_entries(all_disabled));
    require(take(menu.navigate(ii::gui::MenuNavigation::next))==-1&&menu.selected_index()==-1,"All-disabled menu admitted selection");
    std::vector<ii::gui::MenuEntry> many;many.reserve(4096);
    for(int i=0;i<4096;++i)many.push_back({.key=std::to_string(i),.label="Item",.compact=false});
    good(menu.set_entries(std::move(many)));require(!menu.layout(),"An oversized menu layout was not rejected");
}
