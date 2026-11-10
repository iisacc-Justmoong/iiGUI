#include <iiGUI/ListItemModel.h>

#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>

namespace {
void require(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
bool inside(ii::gui::ListRowRect rect, double width, double height) {
    return rect.x >= 0 && rect.y >= 0 && rect.width >= 0 && rect.height >= 0 &&
        rect.x + rect.width <= width + 1e-9 && rect.y + rect.height <= height + 1e-9;
}
}
int main() {
    using namespace ii::gui;
    ListItemModel item; auto value = item.configuration();
    value.palette.text = {.8, .8, .8, 1};
    value.palette.disabled_text = {.2, .2, .2, 1};
    value.palette.hover = {.1, .2, .3, 1};
    value.palette.pressed = {.2, .3, .4, 1};
    value.palette.selected = {.3, .4, .5, 1};
    value.palette.focus = {.4, .5, .6, 1};
    require(item.intrinsic_size().width == 170 && item.intrinsic_size().height == 20,
            "Mini item lost its default dimensions");
    value.label = "Camera"; value.description = "A camera"; value.has_icon = true;
    require(item.set_configuration(value), "Valid list row was rejected");
    auto row = item.geometry(200, 36);
    require(row && row->icon.x == 4 && row->icon.width == 14 && row->label.x == 24 &&
            row->description.height == 14 && row->label.width == 172,
            "List row geometry did not preserve inherited icon and text spacing");
    auto reserved=value;reserved.has_icon=false;reserved.reserved_leading=24;
    require(item.set_configuration(reserved),"A custom leading extent was rejected");
    auto narrow=item.geometry(20,44),wide=item.geometry(200,44);
    require(narrow&&wide&&narrow->icon.width==12&&narrow->label.width==0&&wide->icon.width==24&&wide->label.x==34,
            "Custom leading extent did not reserve space without overlapping the text");
    reserved.reserved_leading=60;reserved.reserved_leading_height=20;
    require(item.set_configuration(reserved),"A rectangular leading extent was rejected");
    auto rectangular=item.geometry(200,44);
    require(rectangular&&rectangular->icon.width==60&&rectangular->icon.height==20&&rectangular->icon.y==12&&rectangular->label.x==70,
            "A rectangular leading view lost its independent dimensions or text reservation");
    auto invalid_reserved=reserved;invalid_reserved.reserved_leading=-1;
    require(!item.set_configuration(invalid_reserved)&&item.configuration()==reserved,"Invalid custom leading extent replaced the current row");
    invalid_reserved=reserved;invalid_reserved.reserved_leading_height=-1;
    require(!item.set_configuration(invalid_reserved)&&item.configuration()==reserved,"Invalid custom leading height replaced the current row");
    for(int width=0;width<=80;++width)for(int height=0;height<=40;++height){
        auto geometry=item.geometry(width,height);
        require(geometry&&inside(geometry->icon,width,height)&&inside(geometry->label,width,height)&&
                geometry->icon.x+geometry->icon.width<=geometry->label.x+1e-9,
                "Rectangular leading extent overlapped the text or escaped its row");
    }
    require(item.set_configuration(value),"The original row configuration was not restored");
    auto metrics=value;metrics.variant=ListRowVariant::navigation;metrics.value="Ready";
    metrics.value_width=20;metrics.value_gap=3;metrics.value_line_height=9;
    require(item.set_configuration(metrics),"Application row metrics were rejected");
    const auto custom=item.geometry(200,44);
    require(custom && custom->value.height==9 && custom->value.width==20 &&
            custom->label.x+custom->label.width+3==custom->value.x,
            "Application row metrics did not control value layout");
    metrics.value_gap=std::numeric_limits<double>::infinity();
    require(!item.set_configuration(metrics),"Invalid application row metrics were accepted");
    require(item.set_configuration(value),"The original row configuration was not restored after metric tests");
    require(item.visual({.hovered=true}).background == value.palette.hover &&
            item.visual({.pressed=true}).background == value.palette.pressed,
            "List row interaction visuals changed");
    value.selected = true; require(item.set_configuration(value) &&
        item.visual({.pressed=true}).background == value.palette.selected,
        "Selected list row did not retain its selection visual");
    value.enabled = false; require(item.set_configuration(value) &&
        item.visual({.focused=true}).text == value.palette.disabled_text && !item.visual({.focused=true}).focused,
        "Disabled list row used the active text or focus style");
    value.enabled = true; value.show_chevron = true; value.variant = ListRowVariant::navigation;
    require(item.set_configuration(value) && item.intrinsic_size().width == 280 && item.intrinsic_size().height == 44,
            "Navigation row lost its inherited dimensions");
    for (int width = 0; width <= 40; ++width) for (int height = 0; height <= 40; ++height) {
        auto geometry = item.geometry(width, height);
        require(geometry && inside(geometry->icon, width, height) && inside(geometry->label, width, height) &&
                inside(geometry->description, width, height) && inside(geometry->chevron, width, height),
                "Narrow list row escaped its bounds");
    }
    value.variant = ListRowVariant::toggle; require(item.set_configuration(value) && item.intrinsic_size().width == 280 && item.intrinsic_size().height == 44,
            "Toggle row lost its inherited dimensions");
    value.variant = ListRowVariant::checkable; require(item.set_configuration(value) && item.intrinsic_size().width == 280 && item.intrinsic_size().height == 44,
            "Checkable row lost its inherited dimensions");
    value.variant = ListRowVariant::action_group; require(item.set_configuration(value) && item.intrinsic_size().width == 400 && item.intrinsic_size().height == 44,
            "Action group row lost its inherited dimensions");
    value.variant = ListRowVariant::stepper; require(item.set_configuration(value) && item.intrinsic_size().width == 400 && item.intrinsic_size().height == 44,
            "Stepper row lost its inherited dimensions");
    value.variant = ListRowVariant::select; require(item.set_configuration(value) && item.intrinsic_size().width == 400 && item.intrinsic_size().height == 44,
            "Selectable row lost its inherited dimensions");
    value.variant = ListRowVariant::inline_edit; require(item.set_configuration(value) && item.intrinsic_size().width == 400 && item.intrinsic_size().height == 44,
            "Inline edit row lost its inherited dimensions");
    value.variant = ListRowVariant::detail; value.width = 0; value.height = 0;
    require(item.set_configuration(value) && item.intrinsic_size().width == 194 && item.intrinsic_size().height == 44,
            "Detail row lost its inherited dimensions");
    value.width = 315; value.height = 55;
    require(item.set_configuration(value) && item.intrinsic_size().width == 315 && item.intrinsic_size().height == 55,
            "Explicit row dimensions were ignored");
    const auto previous = item.configuration(); value.width = std::numeric_limits<double>::infinity();
    require(!item.set_configuration(value) && item.configuration() == previous, "Invalid metrics replaced committed row settings");
    value = previous; value.label = std::string("bad\xC0\x80", 5);
    require(!item.set_configuration(value), "Invalid UTF-8 row label was accepted");
    value = previous; value.variant = static_cast<ListRowVariant>(99);
    require(!item.set_configuration(value), "Unknown row variant was accepted");
    require(!item.geometry(-1, 20) && !item.geometry(20, std::numeric_limits<double>::quiet_NaN()),
            "Invalid host extent produced row geometry");
}
