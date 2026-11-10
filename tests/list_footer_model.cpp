#include <iiGUI/ListFooterModel.h>

#include <limits>
#include <stdexcept>

namespace {
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
}
int main() {
    using namespace ii::gui;
    FooterOptions value;
    const auto defaults=[](FooterOptions& item){
        item.slots[0].icon_name="addFile";item.slots[1].icon_name="generaldelete";
        item.slots[2].icon_name="settings";item.slots[2].menu_indicator=true;
    };
    defaults(value); auto natural = ListFooterModel::geometry(value);
    require(natural && natural->width == 88 && natural->height == 26 && natural->slots[0].width == 22 &&
            natural->slots[1].x == 24 && natural->slots[2].width == 40 && value.slots[2].menu_indicator,
            "List footer defaults or menu geometry changed");
    value.slots[1].visible = false; auto hidden = ListFooterModel::geometry(value);
    require(hidden && hidden->width == 66 && hidden->slots[1].width == 0 && hidden->slots[2].x == 24,
            "A hidden footer slot still occupied layout space");
    value.width = 20; value.height = 10; auto narrow = ListFooterModel::geometry(value);
    require(narrow && narrow->slots[0].width > 0 && narrow->slots[2].x + narrow->slots[2].width <= 18.0001 &&
            narrow->slots[0].height == 6, "Narrow footer slots escaped the available bounds");
    value = {}; defaults(value); value.slots[0].text = "Open"; auto labeled = ListFooterModel::geometry(value, {30, 0, 0});
    require(labeled && labeled->width == 122, "Footer layout did not include its measured label");
    value = {}; defaults(value); value.slot_width=72; value.spacing=8; value.horizontal_padding=0; value.width=232;
    auto fixed=ListFooterModel::geometry(value,{30,10,10});
    require(fixed&&fixed->slots[0].width==72&&fixed->slots[1].x==80&&fixed->slots[2].x==160,
            "Fixed footer action widths did not match their layout");
    value = {}; defaults(value);value.menu_indicator_width=30;value.icon_text_spacing=6;value.slot_padding=6;
    value.slots[0].text="Open";auto customized=ListFooterModel::geometry(value,{30,0,0});
    require(customized && customized->slots[0].width==60 && customized->slots[2].width==54,
            "Injected footer spacing or indicator width was not used");
    value.menu_indicator_width=-1;require(!ListFooterModel::validate(value),"A negative menu indicator width was accepted");
    value.menu_indicator_width=18;value.menu_indicator_text=std::string("bad\xff",4);
    require(!ListFooterModel::validate(value),"Invalid menu indicator UTF-8 was accepted");
    value = {}; value.spacing = std::numeric_limits<double>::infinity(); require(!ListFooterModel::geometry(value), "Invalid footer spacing was accepted");
    value = {}; value.slots[0].icon_name = std::string("bad\xff", 4); require(!ListFooterModel::validate(value), "Invalid footer icon UTF-8 was accepted");
    value = {}; require(!ListFooterModel::geometry(value, {std::numeric_limits<double>::infinity(), 0, 0}), "Invalid footer text measurement was accepted");
}
