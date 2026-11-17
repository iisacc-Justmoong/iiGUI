#pragma once

#include "AccessibilityTree.h"
#include <cmath>
#include <limits>
#include <locale>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace ii::gui::detail {
// These peers expose existing iiGUI state; they never paint or own controls.
struct WebAccessibilityNode final {
    std::string id,role,name;
    bool enabled=true,checked=false,actionable=false,focused=false;
    std::optional<AccessibilityRange> range;
    Rect bounds{};
};

[[nodiscard]] inline std::vector<WebAccessibilityNode> web_accessibility_nodes(const std::vector<WidgetSnapshot>& snapshots){
    AccessibilityTree tree(snapshots);std::vector<WebAccessibilityNode> output;output.reserve(tree.size());
    for(std::size_t i=0;i<tree.size();++i){
        const auto& item=tree.node(i);const auto& snapshot=item.snapshot;std::string_view role;
        std::optional<AccessibilityRange> range;
        switch(item.role){
        case AccessibilityProjectionRole::text:role="text";break;
        case AccessibilityProjectionRole::button:role="button";break;
        case AccessibilityProjectionRole::link:role="link";break;
        case AccessibilityProjectionRole::check_box:
            role=snapshot.kind==WidgetKind::toggle_switch||
                (snapshot.kind==WidgetKind::action_surface&&snapshot.action_role==ActionRole::toggle_switch)?"switch":"checkbox";break;
        case AccessibilityProjectionRole::radio_button:role="radio";break;
        case AccessibilityProjectionRole::slider:case AccessibilityProjectionRole::progress:
            range=snapshot.accessibility_semantics&&snapshot.accessibility_semantics->range?snapshot.accessibility_semantics->range:
                std::optional<AccessibilityRange>{AccessibilityRange{snapshot.minimum,snapshot.maximum,snapshot.value}};
            if(!std::isfinite(range->minimum)||!std::isfinite(range->maximum)||!std::isfinite(range->value)||!std::isfinite(range->maximum-range->minimum)||
               range->minimum>=range->maximum||range->value<range->minimum||range->value>range->maximum)continue;
            role=item.role==AccessibilityProjectionRole::slider?"slider":"progressbar";
            break;
        default:continue;
        }
        const auto bounds=item.bounds;
        if(!std::isfinite(bounds.x)||!std::isfinite(bounds.y)||!std::isfinite(bounds.width)||!std::isfinite(bounds.height))continue;
        output.push_back({snapshot.id,std::string(role),item.name,snapshot.enabled,snapshot.checked,
                          item.actionable||(role=="slider"&&snapshot.enabled),snapshot.focused,range,bounds});
    }
    return output;
}

inline void web_json_string(std::string& output,std::string_view text){
    constexpr char hex[]="0123456789abcdef";output.push_back('"');
    for(unsigned char c:text){
        if(c=='"'||c=='\\'){output.push_back('\\');output.push_back(char(c));}
        else if(c<0x20){output.append("\\u00");output.push_back(hex[c>>4]);output.push_back(hex[c&15]);}
        else output.push_back(char(c));
    }
    output.push_back('"');
}
inline void web_json_number(std::string& output,double value){
    std::ostringstream stream;stream.imbue(std::locale::classic());stream.precision(std::numeric_limits<double>::max_digits10);stream<<value;
    if(!stream)throw std::runtime_error("Cannot encode Web accessibility range");output.append(stream.str());
}
[[nodiscard]] inline std::string web_accessibility_json(const std::vector<WebAccessibilityNode>& nodes){
    std::string result="[";bool first=true;
    for(const auto& node:nodes){
        if(!first)result.push_back(',');first=false;result.append("{\"id\":");web_json_string(result,node.id);
        result.append(",\"role\":");web_json_string(result,node.role);
        result.append(",\"name\":");web_json_string(result,node.name);
        result.append(",\"enabled\":");result.append(node.enabled?"true":"false");
        result.append(",\"checked\":");result.append(node.checked?"true":"false");
        result.append(",\"actionable\":");result.append(node.actionable?"true":"false");
        result.append(",\"focused\":");result.append(node.focused?"true":"false");
        result.append(",\"bounds\":[");web_json_number(result,node.bounds.x);result.push_back(',');web_json_number(result,node.bounds.y);
        result.push_back(',');web_json_number(result,node.bounds.width);result.push_back(',');web_json_number(result,node.bounds.height);result.push_back(']');
        if(node.range){result.append(",\"minimum\":");web_json_number(result,node.range->minimum);
            result.append(",\"maximum\":");web_json_number(result,node.range->maximum);
            result.append(",\"value\":");web_json_number(result,node.range->value);}
        result.push_back('}');
    }
    result.push_back(']');return result;
}
} // namespace ii::gui::detail
