#include <iiGUI/WidgetState.h>
#include <memory>

// Link the ordinary retained-widget path without using compatibility APIs.
int main() {
    ii::gui::require_text_weight_rules_abi_v1();
    auto state = std::make_shared<ii::gui::detail::WidgetState>(
        [](ii::runtime::Task task) { task(); }, ii::gui::default_widget_appearance());
    return state->appearance() ? 0 : 1;
}
