#pragma once
#include <iiGUI/NativeWidgets.h>
#include <iiGUI/NativeListItemView.h>
#include <iiGUI/NativeTextLayout.h>
#include <algorithm>
#include <any>
#include <array>
#include <cmath>
#include <concepts>
#include <cstdint>
#include <exception>
#include <functional>
#include <iomanip>
#include <limits>
#include <locale>
#include <map>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>
namespace ii::gui {
template<class NativeInputFieldT, class NativeComboBoxT, class NativeMenuItemT, class NativeMenuT, class ListItemSelectorT, class NativeListItemSelectorT, class ListItemSegmentsT, class NativeListItemSegmentsT, class NativeToolbarButtonT, class NativeStepperT, class NativeListFooterT, class ListItemModelT, class ListItemDrawingT, class NativeListItemT, class CompositeCheckBoxT, class CompositeToggleSwitchT, class MenuItemPortT, class MenuSectionPortT, class IconCacheT, class ToolbarButtonInputT, class ListItemDetailActionsT, class ListItemDetailControlsT, class ListItemTaskT, class ListItemPreviewRowT, class ListItemFormT, class ListItemQuantityT, class ListItemMenuT>
struct ListItemCompositeComponents final {
    using NativeInputField=NativeInputFieldT;
    using NativeComboBox=NativeComboBoxT;
    using NativeMenuItem=NativeMenuItemT;
    using NativeMenu=NativeMenuT;
    using ListItemSelector=ListItemSelectorT;
    using NativeListItemSelector=NativeListItemSelectorT;
    using ListItemSegments=ListItemSegmentsT;
    using NativeListItemSegments=NativeListItemSegmentsT;
    using NativeToolbarButton=NativeToolbarButtonT;
    using NativeStepper=NativeStepperT;
    using NativeListFooter=NativeListFooterT;
    using ListItemModel=ListItemModelT;
    using ListItemDrawing=ListItemDrawingT;
    using NativeListItem=NativeListItemT;
    using CompositeCheckBox=CompositeCheckBoxT;
    using CompositeToggleSwitch=CompositeToggleSwitchT;
    using MenuItemPort=MenuItemPortT;
    using MenuSectionPort=MenuSectionPortT;
    using IconCache=IconCacheT;
    using ToolbarButtonInput=ToolbarButtonInputT;
    using ListItemDetailActions=ListItemDetailActionsT;
    using ListItemDetailControls=ListItemDetailControlsT;
    using ListItemTask=ListItemTaskT;
    using ListItemPreviewRow=ListItemPreviewRowT;
    using ListItemForm=ListItemFormT;
    using ListItemQuantity=ListItemQuantityT;
    using ListItemMenu=ListItemMenuT;
};
// Compound behavior is assembled from injected controls and data contracts.
template<class Policy>
class BasicNativeListItemComposite final {
    using ListItemCompositeInput=typename Policy::ListItemCompositeInput;
    using NativeListItemCompositeCallbacks=typename Policy::NativeListItemCompositeCallbacks;
    using NativeListItemCompositeViews=typename Policy::NativeListItemCompositeViews;
    using ListItemContext=typename Policy::ListItemContext;
    using ListItemContextSnapshot=typename Policy::ListItemContextSnapshot;
    using ListItemContextCommands=typename Policy::ListItemContextCommands;
    using ListItemInput=typename Policy::ListItemInput;
    using NativeInputField=typename Policy::NativeInputField;
    using NativeComboBox=typename Policy::NativeComboBox;
    using NativeMenuItem=typename Policy::NativeMenuItem;
    using NativeMenu=typename Policy::NativeMenu;
    using ListItemSelector=typename Policy::ListItemSelector;
    using NativeListItemSelector=typename Policy::NativeListItemSelector;
    using ListItemSegments=typename Policy::ListItemSegments;
    using NativeListItemSegments=typename Policy::NativeListItemSegments;
    using NativeToolbarButton=typename Policy::NativeToolbarButton;
    using NativeStepper=typename Policy::NativeStepper;
    using NativeListFooter=typename Policy::NativeListFooter;
    using ListItemModel=typename Policy::ListItemModel;
    using ListItemDrawing=typename Policy::ListItemDrawing;
    using NativeListItem=typename Policy::NativeListItem;
    using CompositeCheckBox=typename Policy::CompositeCheckBox;
    using CompositeToggleSwitch=typename Policy::CompositeToggleSwitch;
    using MenuItemPort=typename Policy::MenuItemPort;
    using MenuSectionPort=typename Policy::MenuSectionPort;
    using IconCache=typename Policy::IconCache;
    using ToolbarButtonInput=typename Policy::ToolbarButtonInput;
    using ListItemVariant=typename Policy::ListItemVariant;
    using ListItemDetailActions=typename Policy::ListItemDetailActions;
    using ListItemDetailActionsConfiguration=typename Policy::ListItemDetailActionsConfiguration;
    using ListItemDetailControls=typename Policy::ListItemDetailControls;
    using ListItemDetailControlsConfiguration=typename Policy::ListItemDetailControlsConfiguration;
    using ListItemDetailControlVariant=typename Policy::ListItemDetailControlVariant;
    using ListItemTask=typename Policy::ListItemTask;
    using ListItemTaskConfiguration=typename Policy::ListItemTaskConfiguration;
    using ListItemPreviewRow=typename Policy::ListItemPreviewRow;
    using ListItemPreviewRowConfiguration=typename Policy::ListItemPreviewRowConfiguration;
    using ListItemPreviewVariant=typename Policy::ListItemPreviewVariant;
    using ListItemForm=typename Policy::ListItemForm;
    using ListItemFormConfiguration=typename Policy::ListItemFormConfiguration;
    using ListItemQuantity=typename Policy::ListItemQuantity;
    using ListItemQuantityConfiguration=typename Policy::ListItemQuantityConfiguration;
    using ListItemSize=typename Policy::ListItemSize;
    using ListItemRect=typename Policy::ListItemRect;
    using ListItemActionValues=typename Policy::ListItemActionValues;
    using ListItemActionPayload=typename Policy::ListItemActionPayload;
    using ListItemOption=typename Policy::ListItemOption;
    using ListItemMenuEntry=typename Policy::ListItemMenuEntry;
    using ListItemMenuSelection=typename Policy::ListItemMenuSelection;
    using ListItemMenu=typename Policy::ListItemMenu;
    using ListItemValue=typename Policy::ListItemValue;
    using ButtonTone=typename Policy::ButtonTone;
    using StepperArrow=typename Policy::StepperArrow;
    using StepperConfiguration=typename Policy::StepperConfiguration;
    using InputFieldConfiguration=typename Policy::InputFieldConfiguration;
    using ToggleSwitchConfiguration=typename Policy::ToggleSwitchConfiguration;using CheckBoxConfiguration=typename Policy::CheckBoxConfiguration;
    using WindowAccentPalette=typename Policy::WindowAccentPalette;using ListFooterSlot=typename Policy::ListFooterSlot;
    using ListItemInputSubmission=typename Policy::ListItemInputSubmission;using ListItemContextViewFactory=typename Policy::ListItemContextViewFactory;
    using ListItemSelectorInput=typename Policy::ListItemSelectorInput;
    using ListItemSegmentsInput=typename Policy::ListItemSegmentsInput;
    using ListFooterInput=typename Policy::ListFooterInput;
    using MenuInput=typename Policy::MenuInput;
    using MenuItemInput=typename Policy::MenuItemInput;
    using MenuItemPresentation=typename Policy::MenuItemPresentation;
    using MenuEntryState=typename Policy::MenuEntryState;
    using MenuItemPalette=typename Policy::MenuItemPalette;
    using IconSource=typename Policy::IconSource;
    struct ActionMenu final {
        std::vector<NativeMenuItem> items;
        std::vector<ListItemOption> options;
        std::optional<std::vector<ListItemMenuEntry>> authored;
        std::vector<ListItemMenuEntry> entries;
        std::vector<ii::gui::NativeDivider> dividers;
        std::vector<NativeMenu> submenus;
        std::vector<MenuItemPort> ports;
        std::map<std::vector<std::size_t>,ii::gui::Widget> item_widgets,submenu_widgets;
        std::shared_ptr<const IconSource> icon_source;
        std::int64_t icon_revision=0;
    };
    struct State final {
        ListItemCompositeInput input;
        std::optional<NativeListItem> row;
        std::optional<CompositeToggleSwitch> toggle;
        std::optional<CompositeCheckBox> checkbox;
        std::optional<NativeListFooter> actions;
        std::optional<NativeToolbarButton> header_more;
        std::optional<NativeStepper> stepper;
        std::optional<NativeListItemSelector> selector;
        std::optional<NativeListItemSelector> unit_selector;
        std::optional<NativeListItemSegments> segments;
        std::optional<NativeInputField> input_field;
        std::optional<NativeInputField> input_field2;
        IconCache bookmarks;
        IconCache preview_icons;
        std::shared_ptr<const ii::gui::RasterImage> bookmark;
        std::shared_ptr<const ii::gui::RasterImage> preview;
        std::optional<InputFieldConfiguration> applied_input,applied_input2;
        ListItemQuantity quantity;
        ii::gui::Widget quantity_label;
        std::optional<ii::gui::Widget> footer;
        std::optional<ii::gui::Widget> preview_container,custom_preview,leading_container,custom_leading,trailing_container,custom_trailing;
        std::shared_ptr<ListItemContext> context;
        std::shared_ptr<const ListItemContextCommands> context_commands;
        double footer_height=0;
        ii::gui::Size leading_natural,preview_natural,trailing_natural;
        ii::gui::Widget menu_host;
        std::optional<NativeMenu> popup;
        MenuSectionPort menu_section;
        std::shared_ptr<ActionMenu> action_menu;
        ii::gui::Widget root;
        NativeListItemCompositeCallbacks callbacks;
        ii::gui::Size extent;
        std::optional<ii::core::Error> failure;
        const std::thread::id owner=std::this_thread::get_id();
        std::uint64_t revision=0;
        bool checked=false,live=false,has_custom_leading=false,has_custom_trailing=false;
        int selector_index=0,unit_index=0,segment_index=0;
        std::string input_text,input_text2;
        ~State(){if(context)Policy::ContextPort::close(*context);}
    };
    explicit BasicNativeListItemComposite(std::shared_ptr<State> state):state_(std::move(state)){}
    [[nodiscard]] static bool detail_controls(const ListItemCompositeInput& input)noexcept{
        return input.row.configuration.variant==ListItemVariant::detail_quantity||input.row.configuration.variant==ListItemVariant::detail_settings;
    }
    [[nodiscard]] static bool preview_row(const ListItemCompositeInput& input)noexcept{
        return input.row.configuration.variant==ListItemVariant::resource||input.row.configuration.variant==ListItemVariant::media;
    }
    [[nodiscard]] static bool leading_row(const ListItemCompositeInput& input)noexcept{
        const auto type=input.row.configuration.variant;
        return type==ListItemVariant::navigation||type==ListItemVariant::toggle||type==ListItemVariant::action||
            type==ListItemVariant::action_group||type==ListItemVariant::stepper||type==ListItemVariant::select||
            type==ListItemVariant::inline_edit||type==ListItemVariant::detail_actions||type==ListItemVariant::detail_settings;
    }
    [[nodiscard]] static bool trailing_row(const ListItemCompositeInput& input)noexcept{
        const auto type=input.row.configuration.variant;
        return type==ListItemVariant::navigation||type==ListItemVariant::toggle||type==ListItemVariant::checkable||
            type==ListItemVariant::action||type==ListItemVariant::action_group||type==ListItemVariant::stepper||
            type==ListItemVariant::select||type==ListItemVariant::inline_edit;
    }
    [[nodiscard]] static bool trailing_override(const State& state,const ListItemCompositeInput& input)noexcept{
        return state.has_custom_trailing&&trailing_row(input);
    }
    [[nodiscard]] static bool trailing_override(const State& state)noexcept{return trailing_override(state,state.input);}
    [[nodiscard]] static double leading_width(const State& state)noexcept{return state.input.leading_size.value_or(state.leading_natural.width);}
    [[nodiscard]] static double leading_height(const State& state)noexcept{return state.input.leading_size.value_or(state.leading_natural.height);}
    [[nodiscard]] static double trailing_width(const State& state)noexcept{return state.input.trailing_width.value_or(state.trailing_natural.width);}
    [[nodiscard]] static double trailing_height(const State& state)noexcept{return state.input.trailing_height.value_or(state.trailing_natural.height);}
    [[nodiscard]] static bool reverse_actions(const ListItemCompositeInput& input)noexcept{
        return input.row.configuration.variant==ListItemVariant::detail_settings||input.row.configuration.variant==ListItemVariant::task||input.row.configuration.variant==ListItemVariant::form||preview_row(input);
    }
    [[nodiscard]] static bool has_toggle(const ListItemCompositeInput& input)noexcept{
        const auto variant=input.row.configuration.variant;
        return (variant==ListItemVariant::toggle||variant==ListItemVariant::detail_quantity||variant==ListItemVariant::detail_settings||variant==ListItemVariant::form)&&input.show_toggle;
    }
    [[nodiscard]] static bool has_checkbox(const ListItemCompositeInput& input)noexcept{
        return (input.row.configuration.variant==ListItemVariant::checkable||input.row.configuration.variant==ListItemVariant::task)&&input.show_selection;
    }
    [[nodiscard]] static bool has_stepper(const ListItemCompositeInput& input)noexcept{
        const auto variant=input.row.configuration.variant;
        return (variant==ListItemVariant::stepper||variant==ListItemVariant::detail_quantity||variant==ListItemVariant::task)&&input.show_stepper;
    }
    [[nodiscard]] static bool has_quantity(const ListItemCompositeInput& input)noexcept{
        return input.row.configuration.variant==ListItemVariant::stepper||input.row.configuration.variant==ListItemVariant::detail_quantity||input.row.configuration.variant==ListItemVariant::task;
    }
    [[nodiscard]] static bool has_selector(const ListItemCompositeInput& input)noexcept{
        const auto variant=input.row.configuration.variant;
        return (variant==ListItemVariant::select||variant==ListItemVariant::detail_quantity||variant==ListItemVariant::detail_settings||variant==ListItemVariant::task||variant==ListItemVariant::form)&&input.show_selector;
    }
    [[nodiscard]] static bool has_unit_selector(const ListItemCompositeInput& input)noexcept{
        return input.row.configuration.variant==ListItemVariant::detail_quantity&&input.show_unit_selector;
    }
    [[nodiscard]] static bool has_segments(const ListItemCompositeInput& input)noexcept{
        return (input.row.configuration.variant==ListItemVariant::detail_settings||input.row.configuration.variant==ListItemVariant::media)&&input.show_segments;
    }
    [[nodiscard]] static bool has_input(const ListItemCompositeInput& input)noexcept{
        return (input.row.configuration.variant==ListItemVariant::inline_edit||input.row.configuration.variant==ListItemVariant::form)&&input.show_input;
    }
    [[nodiscard]] static bool has_input2(const ListItemCompositeInput& input)noexcept{
        return input.row.configuration.variant==ListItemVariant::form&&input.show_input2;
    }
    [[nodiscard]] static bool has_menu_entries(const ListItemCompositeInput& input)noexcept{
        return input.menu_entries?!input.menu_entries->empty():!input.more_items.empty();
    }
    [[nodiscard]] static bool has_action(const ListItemCompositeInput& input,std::size_t slot)noexcept{
        const auto type=input.row.configuration.variant;
        if(slot==0)return (type==ListItemVariant::action||type==ListItemVariant::action_group||type==ListItemVariant::inline_edit||type==ListItemVariant::detail_actions||type==ListItemVariant::detail_quantity||type==ListItemVariant::detail_settings||type==ListItemVariant::task||type==ListItemVariant::form||preview_row(input))&&input.show_primary_action&&input.primary_action.visible;
        if(slot==1)return (type==ListItemVariant::action_group||type==ListItemVariant::detail_actions||type==ListItemVariant::detail_settings||type==ListItemVariant::task||type==ListItemVariant::form||preview_row(input))&&input.show_secondary_action&&input.secondary_action.visible;
        return (type==ListItemVariant::action_group||type==ListItemVariant::detail_actions||type==ListItemVariant::resource)&&input.show_more_menu&&input.more_menu.visible;
    }
    [[nodiscard]] static ListItemDetailActionsConfiguration detail_configuration(const ListItemCompositeInput& input)noexcept{
        ListItemDetailActionsConfiguration c;c.width=input.row.configuration.width;c.height=input.row.configuration.height;
        c.horizontal_padding=input.detail_horizontal_padding;c.vertical_padding=input.detail_vertical_padding;
        c.icon_size=input.detail_icon_size;c.section_spacing=input.detail_section_spacing;c.content_spacing=input.slot_spacing;c.action_width=input.action_width;
        c.show_description=input.row.configuration.show_description;c.show_leading_icon=input.show_leading_icon;c.show_bookmark=input.show_bookmark;
        c.show_metadata=input.show_metadata;c.show_actions={has_action(input,0),has_action(input,1),has_action(input,2)};return c;
    }
    [[nodiscard]] static ListItemDetailControlsConfiguration controls_configuration(const ListItemCompositeInput& input)noexcept{
        ListItemDetailControlsConfiguration c;c.variant=input.row.configuration.variant==ListItemVariant::detail_settings?
            ListItemDetailControlVariant::settings:ListItemDetailControlVariant::quantity;
        c.width=input.row.configuration.width;c.height=input.row.configuration.height;
        c.horizontal_padding=input.detail_horizontal_padding;c.vertical_padding=input.detail_vertical_padding;
        c.icon_size=input.detail_icon_size;c.section_spacing=input.detail_section_spacing;c.content_spacing=input.slot_spacing;
        c.selector_width=input.selector_width;c.unit_selector_width=input.unit_selector_width;c.quantity_width=input.quantity_width;
        c.quantity_spacing=input.quantity_spacing;c.action_width=input.action_width;
        c.segment_width=8+double(input.segments.size())*input.segment_width+double(input.segments.empty()?0:input.segments.size()-1)*2;
        c.segment_height=input.segments.empty()?7:29;
        c.show_description=input.row.configuration.show_description;c.show_leading_icon=input.show_leading_icon;
        c.show_toggle=has_toggle(input);c.show_selector=has_selector(input);c.show_unit_selector=has_unit_selector(input);
        c.show_stepper=has_stepper(input);c.show_segments=has_segments(input);
        c.show_primary_action=has_action(input,0);c.show_secondary_action=has_action(input,1);return c;
    }
    [[nodiscard]] static ListItemTaskConfiguration task_configuration(const ListItemCompositeInput& input)noexcept{
        ListItemTaskConfiguration c;c.width=input.row.configuration.width;c.height=input.row.configuration.height;
        c.horizontal_padding=input.detail_horizontal_padding;c.vertical_padding=input.detail_vertical_padding;
        c.section_spacing=input.detail_section_spacing;c.content_spacing=input.slot_spacing;c.selector_width=input.selector_width;
        c.quantity_width=input.quantity_width;c.quantity_spacing=input.quantity_spacing;c.action_width=input.action_width;
        c.show_description=input.row.configuration.show_description;c.show_metadata=input.show_metadata;
        c.show_selection=has_checkbox(input);c.show_selector=has_selector(input);c.show_stepper=has_stepper(input);
        c.show_primary_action=has_action(input,0);c.show_secondary_action=has_action(input,1);return c;
    }
    [[nodiscard]] static ListItemPreviewRowConfiguration preview_configuration(const ListItemCompositeInput& input)noexcept{
        ListItemPreviewRowConfiguration c;c.variant=input.row.configuration.variant==ListItemVariant::media?
            ListItemPreviewVariant::media:ListItemPreviewVariant::resource;
        c.width=input.row.configuration.width;c.height=input.row.configuration.height;
        c.horizontal_padding=input.detail_horizontal_padding;c.vertical_padding=input.detail_vertical_padding;
        c.section_spacing=input.detail_section_spacing;c.content_spacing=input.slot_spacing;c.preview_size=input.preview_size.value_or(48.);c.action_width=input.action_width;
        c.segment_width=8+double(input.segments.size())*input.segment_width+double(input.segments.empty()?0:input.segments.size()-1)*2;
        c.segment_height=input.segments.empty()?7:29;
        c.show_preview=input.show_preview;c.show_description=input.row.configuration.show_description;c.show_metadata=input.show_metadata;
        c.show_more=has_action(input,2);c.show_segments=has_segments(input);
        c.show_primary_action=has_action(input,0);c.show_secondary_action=has_action(input,1);return c;
    }
    [[nodiscard]] static ListItemFormConfiguration form_configuration(const ListItemCompositeInput& input)noexcept{
        ListItemFormConfiguration c;c.width=input.row.configuration.width;c.height=input.row.configuration.height;
        c.horizontal_padding=input.detail_horizontal_padding;c.vertical_padding=input.detail_vertical_padding;
        c.section_spacing=input.detail_section_spacing;c.content_spacing=input.slot_spacing;c.selector_width=input.selector_width;
        c.input_width=input.input_width;c.input_height=input.input_height;c.action_width=input.action_width;
        c.show_description=input.row.configuration.show_description;c.show_selector=has_selector(input);c.show_input1=has_input(input);
        c.show_input2=has_input2(input);c.show_toggle=has_toggle(input);
        c.show_primary_action=has_action(input,0);c.show_secondary_action=has_action(input,1);return c;
    }
    [[nodiscard]] static ListItemDetailActionsConfiguration detail_configuration(const State& state)noexcept{
        auto c=detail_configuration(state.input);
        if(state.has_custom_leading&&state.input.show_leading_icon&&state.input.row.configuration.variant==ListItemVariant::detail_actions){
            c.leading_width=leading_width(state);
            c.leading_height=(std::min)(leading_height(state),ListItemDetailActions::maximum_leading_height(c));
        }
        return c;
    }
    [[nodiscard]] static ListItemDetailControlsConfiguration controls_configuration(const State& state)noexcept{
        auto c=controls_configuration(state.input);
        if(state.has_custom_leading&&state.input.show_leading_icon&&state.input.row.configuration.variant==ListItemVariant::detail_settings){
            c.leading_width=leading_width(state);
            c.leading_height=(std::min)(leading_height(state),ListItemDetailControls::maximum_leading_height(c));
        }
        return c;
    }
    [[nodiscard]] static ListItemPreviewRowConfiguration preview_configuration(const State& state)noexcept{
        auto c=preview_configuration(state.input);
        if(state.custom_preview&&!state.input.preview_size){
            c.preview_width=state.preview_natural.width;
            c.preview_height=(std::min)(state.preview_natural.height,ListItemPreviewRow::maximum_preview_height(c));
        }
        return c;
    }
    [[nodiscard]] static ListItemSize intrinsic(const ListItemCompositeInput& input)noexcept{
        if(input.row.configuration.variant==ListItemVariant::detail_actions)return *ListItemDetailActions::intrinsic_size(detail_configuration(input));
        if(detail_controls(input))return *ListItemDetailControls::intrinsic_size(controls_configuration(input));
        if(input.row.configuration.variant==ListItemVariant::task)return *ListItemTask::intrinsic_size(task_configuration(input));
        if(preview_row(input))return *ListItemPreviewRow::intrinsic_size(preview_configuration(input));
        if(input.row.configuration.variant==ListItemVariant::form)return *ListItemForm::intrinsic_size(form_configuration(input));
        ListItemModel model;(void)model.set_configuration(input.row.configuration);return model.intrinsic_size();
    }
    [[nodiscard]] static bool footer_allowed(const State& state)noexcept{
        const auto type=state.input.row.configuration.variant;
        return state.footer.has_value()&&type!=ListItemVariant::mini&&type!=ListItemVariant::detail;
    }
    [[nodiscard]] static bool has_footer(const State& state)noexcept{return footer_allowed(state)&&state.footer_height>0;}
    [[nodiscard]] static ListItemSize intrinsic(const State& state)noexcept{
        auto size=intrinsic(state.input);
        const auto variant=state.input.row.configuration.variant;
        if(variant==ListItemVariant::detail_actions)size=*ListItemDetailActions::intrinsic_size(detail_configuration(state));
        else if(detail_controls(state.input))size=*ListItemDetailControls::intrinsic_size(controls_configuration(state));
        else if(preview_row(state.input))size=*ListItemPreviewRow::intrinsic_size(preview_configuration(state));
        else if(state.has_custom_leading&&leading_row(state.input)&&state.input.show_leading_icon&&state.input.row.configuration.height==0)
            size.height=(std::max)(size.height,leading_height(state));
        if(trailing_override(state)&&state.input.row.configuration.height==0)size.height=(std::max)(size.height,trailing_height(state));
        if(has_footer(state)&&state.input.row.configuration.height==0)
            size.height=(std::min)(8192.,size.height+state.input.detail_section_spacing+state.footer_height);
        return size;
    }
    [[nodiscard]] static double body_height(const State& state)noexcept{
        if(!has_footer(state))return state.extent.height;
        const auto footer=(std::min)(state.footer_height,state.extent.height);
        return (std::max)(0.,state.extent.height-footer-(std::min)(state.input.detail_section_spacing,state.extent.height-footer));
    }
    [[nodiscard]] static double actions_width(const ListItemCompositeInput& input)noexcept{
        const auto count=unsigned(has_action(input,0))+unsigned(has_action(input,1))+
            unsigned(input.row.configuration.variant!=ListItemVariant::resource&&has_action(input,2));
        return (std::min)(8192.,double(count)*input.action_width+double(count?count-1:0)*input.slot_spacing);
    }
    [[nodiscard]] static double inline_width(const ListItemCompositeInput& input)noexcept{
        const auto action=actions_width(input);return (has_input(input)?input.input_width:0)+action+(has_input(input)&&action>0?input.slot_spacing:0);
    }
    [[nodiscard]] static bool default_checked(const ListItemCompositeInput& input)noexcept{
        return input.row.configuration.variant==ListItemVariant::toggle||input.row.configuration.variant==ListItemVariant::form||detail_controls(input);
    }
    [[nodiscard]] static ListItemActionValues action_values_of(const State& state){
        return {state.input_text,state.input_text2,state.checked,state.quantity.configuration().value,
                state.selector_index,state.unit_index,state.segment_index};
    }
    [[nodiscard]] static ListItemContextSnapshot context_snapshot(const State& state){
        const auto& c=state.input.row.configuration;
        return {.label=c.label,.description=c.description,.inputText1=state.input_text,.inputText2=state.input_text2,
            .type=c.variant,.enabled=c.enabled,.visible=c.visible,.checked=state.checked,.quantity=state.quantity.configuration().value,
            .selectorIndex=state.selector_index,.unitIndex=state.unit_index,.segmentIndex=state.segment_index,
            .options=state.input,.values=action_values_of(state),.commands=state.context_commands};
    }
    static void publish_context(const std::shared_ptr<State>& state)noexcept{
        if(!state->live||!state->context)return;
        try{Policy::ContextPort::publish(*state->context,context_snapshot(*state));}
        catch(...){try{state->failure=ii::core::Error{ii::core::ErrorCode::failed,"List item context observer failed",std::current_exception()};}catch(...) {}}
    }
    [[nodiscard]] static bool valid(const ListItemCompositeInput& input)noexcept{
        return (!input.accent||Policy::valid_window_accent_palette(*input.accent))&&ListItemModel::validate(input.row.configuration)&&input.row.icon_revision>=0&&ListItemSelector::validate_options(input.more_items)&&
            (!input.menu_entries||(input.more_items.empty()&&ListItemMenu::validate_options(*input.menu_entries)))&&
            ListItemDetailActions::intrinsic_size(detail_configuration(input)).has_value()&&
            (!detail_controls(input)||ListItemDetailControls::intrinsic_size(controls_configuration(input)).has_value())&&
            (input.row.configuration.variant!=ListItemVariant::task||ListItemTask::intrinsic_size(task_configuration(input)).has_value())&&
            (!preview_row(input)||ListItemPreviewRow::intrinsic_size(preview_configuration(input)).has_value())&&
            (input.row.configuration.variant!=ListItemVariant::form||ListItemForm::intrinsic_size(form_configuration(input)).has_value())&&
            (!input.show_leading_icon||input.detail_icon_size>=1)&&(!input.show_bookmark||input.detail_icon_size>=1)&&
            Policy::valid_accessibility_text(input.date_text)&&Policy::valid_accessibility_text(input.metadata1)&&Policy::valid_accessibility_text(input.metadata2)&&
            Policy::valid_accessibility_text(input.status_text)&&input.bookmark_icon_name.size()<=256&&Policy::valid_accessibility_text(input.bookmark_icon_name)&&
            Policy::valid_accessibility_text(input.quantity_label)&&Policy::valid_accessibility_text(input.option_label)&&
            Policy::valid_accessibility_text(input.mode_label)&&Policy::valid_accessibility_text(input.scope_label)&&
            Policy::valid_accessibility_text(input.duration_text)&&Policy::valid_accessibility_text(input.media_detail)&&
            input.preview_icon_name.size()<=256&&Policy::valid_accessibility_text(input.preview_icon_name)&&
            (!input.preview_size||(std::isfinite(*input.preview_size)&&*input.preview_size>=0&&*input.preview_size<=8192))&&
            (!preview_row(input)||!input.show_preview||input.preview||input.preview_icon_name.empty()||input.detail_icon_size>=1)&&
            (!input.show_bookmark||!input.bookmark_icon_name.empty())&&
            !(input.row.icon&&!input.row.configuration.icon_name.empty())&&std::isfinite(input.slot_padding)&&
            input.slot_padding>=0&&input.slot_padding<=8192&&std::isfinite(input.slot_spacing)&&input.slot_spacing>=0&&input.slot_spacing<=8192&&
            std::isfinite(input.action_width)&&input.action_width>=1&&input.action_width<=8192&&
            (!input.leading_size||(std::isfinite(*input.leading_size)&&*input.leading_size>=0&&*input.leading_size<=8192))&&
            (!input.trailing_width||(std::isfinite(*input.trailing_width)&&*input.trailing_width>=0&&*input.trailing_width<=8192))&&
            (!input.trailing_height||(std::isfinite(*input.trailing_height)&&*input.trailing_height>=0&&*input.trailing_height<=8192))&&
            std::isfinite(input.minimum_quantity)&&
            (std::isfinite(input.maximum_quantity)||input.maximum_quantity==std::numeric_limits<double>::infinity())&&
            input.minimum_quantity<=input.maximum_quantity&&std::isfinite(input.step_size)&&input.step_size>0&&
            (input.stepper_arrow==StepperArrow::up_down||input.stepper_arrow==StepperArrow::up||input.stepper_arrow==StepperArrow::down)&&
            (input.stepper_tone==ButtonTone::primary||input.stepper_tone==ButtonTone::normal||input.stepper_tone==ButtonTone::borderless||
             input.stepper_tone==ButtonTone::destructive||input.stepper_tone==ButtonTone::disabled)&&
            std::isfinite(input.quantity_width)&&input.quantity_width>=0&&input.quantity_width<=8192&&
            std::isfinite(input.quantity_spacing)&&input.quantity_spacing>=0&&input.quantity_spacing<=8192&&
            std::isfinite(input.selector_width)&&input.selector_width>=19&&input.selector_width<=8176&&ListItemSelector::validate(input.selector)&&
            std::isfinite(input.unit_selector_width)&&input.unit_selector_width>=19&&input.unit_selector_width<=8176&&ListItemSelector::validate(input.unit_selector)&&
            std::isfinite(input.segment_width)&&input.segment_width>=1&&input.segment_width<=8192&&ListItemSegments::validate_options(input.segments)&&
            8+double(input.segments.size())*input.segment_width+double(input.segments.empty()?0:input.segments.size()-1)*2<=8192&&
            std::isfinite(input.input_width)&&input.input_width>=1&&input.input_width<=8192&&std::isfinite(input.input_height)&&input.input_height>=1&&input.input_height<=8192&&
            Policy::valid_accessibility_text(input.input_placeholder)&&Policy::valid_accessibility_text(input.input_placeholder2)&&
            Policy::valid_accessibility_text(input.field_label1)&&Policy::valid_accessibility_text(input.field_label2)&&
            (!input.input_text||Policy::valid_accessibility_text(*input.input_text))&&(!input.input_text2||Policy::valid_accessibility_text(*input.input_text2))&&
            (!input.selector_index||(*input.selector_index>=0&&(input.selector.options.empty()?*input.selector_index==0:
                std::size_t(*input.selector_index)<input.selector.options.size())))&&
            (!input.unit_index||(*input.unit_index>=0&&(input.unit_selector.options.empty()?*input.unit_index==0:
                std::size_t(*input.unit_index)<input.unit_selector.options.size())))&&
            (!input.segment_index||(*input.segment_index>=0&&(input.segments.empty()?*input.segment_index==0:
                std::size_t(*input.segment_index)<input.segments.size())))&&
            (!input.quantity||(std::isfinite(*input.quantity)&&*input.quantity>=input.minimum_quantity&&*input.quantity<=input.maximum_quantity))&&
            Policy::valid_accessibility_text(input.primary_action.text)&&Policy::valid_accessibility_text(input.primary_action.accessibility_name)&&
            Policy::valid_accessibility_text(input.secondary_action.text)&&Policy::valid_accessibility_text(input.secondary_action.accessibility_name)&&
            Policy::valid_accessibility_text(input.more_menu.text)&&Policy::valid_accessibility_text(input.more_menu.accessibility_name)&&
            input.primary_action.icon_name.size()<=256&&Policy::valid_accessibility_text(input.primary_action.icon_name)&&
            input.secondary_action.icon_name.size()<=256&&Policy::valid_accessibility_text(input.secondary_action.icon_name)&&
            input.more_menu.icon_name.size()<=256&&Policy::valid_accessibility_text(input.more_menu.icon_name);
    }
    [[nodiscard]] static ListItemInput row_input(const State& state){
        auto value=state.input.row;
        if(value.configuration.variant==ListItemVariant::detail_actions||detail_controls(state.input)||value.configuration.variant==ListItemVariant::task||value.configuration.variant==ListItemVariant::form||preview_row(state.input)){
            value.custom_content=true;value.configuration.width=state.extent.width;value.configuration.height=body_height(state);
            value.configuration.icon_size=state.input.detail_icon_size;
            std::string accessible;
            const auto append=[&](const std::string& text){if(text.empty())return;if(!accessible.empty())accessible+=" · ";accessible+=Policy::native_display_text(text);};
            if(value.configuration.show_description)append(value.configuration.description);
            if(value.configuration.variant==ListItemVariant::detail_actions){
                if(state.input.show_metadata){append(state.input.date_text);append(state.input.metadata1);append(state.input.metadata2);}
                append(state.input.status_text);
            }else if(value.configuration.variant==ListItemVariant::detail_quantity){
                append(state.input.quantity_label);std::ostringstream number;number.imbue(std::locale::classic());number<<std::setprecision(12)<<state.quantity.configuration().value;append(number.str());
                if(has_unit_selector(state.input)&&state.unit_index>=0&&std::size_t(state.unit_index)<state.input.unit_selector.options.size())
                    append(state.input.unit_selector.options[std::size_t(state.unit_index)].text);
                append(state.input.option_label);
            }else if(value.configuration.variant==ListItemVariant::task){
                if(state.input.show_metadata){append(state.input.date_text);append(state.input.metadata1);append(state.input.metadata2);}
                if(has_selector(state.input)&&state.selector_index>=0&&std::size_t(state.selector_index)<state.input.selector.options.size())
                    append(state.input.selector.options[std::size_t(state.selector_index)].text);
                append(state.input.quantity_label);std::ostringstream number;number.imbue(std::locale::classic());number<<std::setprecision(12)<<state.quantity.configuration().value;append(number.str());
            }else if(preview_row(state.input)){
                if(value.configuration.variant==ListItemVariant::resource&&state.input.show_metadata){append(state.input.date_text);append(state.input.metadata1);append(state.input.metadata2);}
                if(value.configuration.variant==ListItemVariant::media){append(state.input.duration_text);append(state.input.media_detail);
                    if(has_segments(state.input)&&state.segment_index>=0&&std::size_t(state.segment_index)<state.input.segments.size())
                        append(state.input.segments[std::size_t(state.segment_index)].text);}
                append(state.input.status_text);
            }else if(value.configuration.variant==ListItemVariant::form){
                if(has_selector(state.input)&&state.selector_index>=0&&std::size_t(state.selector_index)<state.input.selector.options.size())
                    append(state.input.selector.options[std::size_t(state.selector_index)].text);
                append(state.input.field_label1);append(state.input.field_label2);append(state.input.status_text);
            }else{
                append(state.input.mode_label);if(has_selector(state.input)&&state.selector_index>=0&&std::size_t(state.selector_index)<state.input.selector.options.size())
                    append(state.input.selector.options[std::size_t(state.selector_index)].text);
                append(state.input.scope_label);if(has_segments(state.input)&&state.segment_index>=0&&std::size_t(state.segment_index)<state.input.segments.size())
                    append(state.input.segments[std::size_t(state.segment_index)].text);
                append(state.input.status_text);
            }
            value.accessibility_description=Policy::native_display_text(accessible);
            if((state.has_custom_leading&&leading_row(state.input))||!state.input.show_leading_icon||value.configuration.variant==ListItemVariant::detail_quantity||value.configuration.variant==ListItemVariant::task||value.configuration.variant==ListItemVariant::form||preview_row(state.input)){value.icon.reset();value.configuration.icon_name.clear();}
            else if(!value.icon&&value.configuration.icon_name.empty())value.configuration.icon_name="nodesfolder";
            return value;
        }
        value.custom_content=false;
        if(state.has_custom_leading&&(leading_row(state.input)||value.configuration.variant==ListItemVariant::checkable)){
            value.icon.reset();value.configuration.icon_name.clear();value.configuration.has_icon=false;
            const bool shown=leading_row(state.input)&&state.input.show_leading_icon;
            value.configuration.reserved_leading=shown?leading_width(state):0;
            value.configuration.reserved_leading_height=shown?std::optional<double>{leading_height(state)}:std::nullopt;
        }
        const auto custom_trailing=trailing_override(state);
        if(custom_trailing){
            value.configuration.show_chevron=false;
            value.configuration.show_value=false;
            if(value.configuration.variant==ListItemVariant::checkable){value.icon.reset();value.configuration.icon_name.clear();value.configuration.has_icon=false;
                value.configuration.reserved_leading=has_checkbox(state.input)?18.:0.;value.configuration.reserved_leading_height.reset();}
        }
        const auto reserve=custom_trailing?trailing_width(state):has_toggle(state.input)?38.:has_checkbox(state.input)?18.:
            has_quantity(state.input)?state.input.quantity_width+(has_stepper(state.input)?18.+state.input.quantity_spacing:0.):
            has_selector(state.input)?state.input.selector_width:
            state.input.row.configuration.variant==ListItemVariant::inline_edit?inline_width(state.input):actions_width(state.input);
        if(reserve>0)value.configuration.right_padding=(std::min)(8192.,value.configuration.right_padding+reserve+state.input.slot_padding+state.input.slot_spacing);
        return value;
    }
    [[nodiscard]] static ToggleSwitchConfiguration toggle_input(const State& state){
        ToggleSwitchConfiguration value;value.checked=state.checked;value.enabled=state.input.row.configuration.enabled&&state.input.toggle_enabled&&!trailing_override(state);
        if(const auto& a=state.input.accent){value.palette.on=a->primary;value.palette.on_hover=a->hover;value.palette.on_pressed=a->pressed;
            value.palette.off_pressed=a->subtle;value.palette.focus=a->primary;}
        value.accessibility_name=Policy::native_display_text(state.input.row.configuration.accessibility_name.empty()?state.input.row.configuration.label:state.input.row.configuration.accessibility_name);
        return value;
    }
    [[nodiscard]] static CheckBoxConfiguration checkbox_input(const State& state){
        CheckBoxConfiguration value;value.checked=state.checked;value.enabled=state.input.row.configuration.enabled&&state.input.selection_enabled;
        if(const auto& a=state.input.accent){value.palette.checked=a->primary;value.palette.focus=a->primary;}
        value.accessibility_name=Policy::native_display_text(state.input.row.configuration.accessibility_name.empty()?state.input.row.configuration.label:state.input.row.configuration.accessibility_name);
        return value;
    }
    [[nodiscard]] static ListItemQuantityConfiguration quantity_input(const ListItemCompositeInput& input,double value)noexcept{
        return {.value=value,.minimum=input.minimum_quantity,.maximum=input.maximum_quantity,.step=input.step_size,
                .enabled=input.row.configuration.enabled&&input.stepper_enabled&&input.stepper_tone!=ButtonTone::disabled};
    }
    [[nodiscard]] static StepperConfiguration stepper_input(const State& state){
        StepperConfiguration value;value.enabled=state.input.row.configuration.enabled&&state.input.stepper_enabled&&!trailing_override(state);
        value.arrow=state.input.stepper_arrow;value.tone=state.input.stepper_tone;
        if(const auto& a=state.input.accent){value.palette.primary=a->primary;value.palette.primary_hover=a->hover;
            value.palette.primary_pressed=a->pressed;value.palette.normal_pressed=a->subtle;}
        value.accessibility_name=Policy::native_display_text((state.input.row.configuration.variant==ListItemVariant::detail_quantity||state.input.row.configuration.variant==ListItemVariant::task)?state.input.quantity_label:
            state.input.row.configuration.accessibility_name.empty()?state.input.row.configuration.label:state.input.row.configuration.accessibility_name);
        return value;
    }
    [[nodiscard]] static ii::gui::LabelOptions quantity_label_input(const State& state){
        std::ostringstream stream;stream.imbue(std::locale::classic());stream<<std::setprecision(12)<<state.quantity.configuration().value;
        const auto& palette=state.input.row.configuration.palette;
        const auto ink=state.input.row.configuration.enabled&&state.input.stepper_enabled?palette.text:palette.disabled_text;
        return {.text=stream.str(),.font_size=13,.selectable=false,.role=ii::gui::TextRole::body,
                .color=ii::gui::WidgetColor{ink.red,ink.green,ink.blue,ink.alpha},.alignment=ii::gui::TextAlignment::right,
                .line_height=16,.font_weight=500u,.elide_right=true};
    }
    [[nodiscard]] static ListItemSelectorInput selector_input(const State& state){
        ListItemSelectorInput value;value.configuration=state.input.selector;value.configuration.selected_index=state.selector_index;
        value.accent=state.input.accent;
        value.configuration.enabled=value.configuration.enabled&&state.input.selector_enabled&&state.input.row.configuration.enabled;
        value.width=state.input.selector_width;value.visible=has_selector(state.input)&&state.input.row.configuration.visible&&!trailing_override(state);
        value.accessibility_name=Policy::native_display_text(state.input.row.configuration.accessibility_name.empty()?state.input.row.configuration.label:state.input.row.configuration.accessibility_name);
        return value;
    }
    [[nodiscard]] static ListItemSelectorInput unit_selector_input(const State& state){
        ListItemSelectorInput value;value.configuration=state.input.unit_selector;value.configuration.selected_index=state.unit_index;
        value.accent=state.input.accent;
        value.configuration.enabled=value.configuration.enabled&&state.input.unit_selector_enabled&&state.input.row.configuration.enabled;
        value.width=state.input.unit_selector_width;value.visible=has_unit_selector(state.input)&&state.input.row.configuration.visible;
        value.accessibility_name=Policy::native_display_text(state.input.unit_selector.placeholder);return value;
    }
    [[nodiscard]] static ListItemSegmentsInput segments_input(const State& state){
        ListItemSegmentsInput value;value.configuration.options=state.input.segments;value.configuration.selected_index=state.segment_index;
        value.accent=state.input.accent;
        value.configuration.segment_width=state.input.segment_width;value.configuration.enabled=state.input.segments_enabled&&state.input.row.configuration.enabled;
        value.visible=has_segments(state.input)&&state.input.row.configuration.visible;return value;
    }
    [[nodiscard]] static InputFieldConfiguration input_field_input(const State& state,bool second=false){
        InputFieldConfiguration value;value.text=second?state.input_text2:state.input_text;value.placeholder=second?state.input.input_placeholder2:state.input.input_placeholder;
        if(const auto& a=state.input.accent){value.palette.focus=a->primary;value.palette.selection=a->primary;value.palette.selection.alpha*=.38;}
        value.width=state.input.input_width;value.height=state.input.input_height;
        value.enabled=state.input.row.configuration.enabled&&(second?state.input.input2_enabled:state.input.input_enabled)&&!trailing_override(state);
        value.read_only=second?state.input.input2_read_only:state.input.input_read_only;
        const auto& name=state.input.row.configuration.variant==ListItemVariant::form?(second?state.input.field_label2:state.input.field_label1):
            state.input.row.configuration.accessibility_name.empty()?state.input.row.configuration.label:state.input.row.configuration.accessibility_name;
        value.accessibility_name=Policy::native_display_text(name);
        return value;
    }
    [[nodiscard]] static ListFooterInput actions_input(const State& state){
        ListFooterInput value;auto& c=value.configuration;const bool reverse=reverse_actions(state.input);
        if(const auto& a=state.input.accent){c.palette.primary=a->primary;c.palette.pressed=a->subtle;c.palette.focus=a->primary;}
        c.slots=reverse?std::array<ListFooterSlot,3>{state.input.secondary_action,state.input.primary_action,state.input.more_menu}:
            std::array<ListFooterSlot,3>{state.input.primary_action,state.input.secondary_action,state.input.more_menu};
        for(std::size_t i=0;i<c.slots.size();++i){c.slots[i].visible=!trailing_override(state)&&!(state.input.row.configuration.variant==ListItemVariant::resource&&i==2)&&has_action(state.input,reverse&&i<2?1-i:i);
            if(trailing_override(state))c.slots[i].icon_name.clear();}
        c.width=actions_width(state.input);c.height=22;c.horizontal_padding=0;c.vertical_padding=0;c.spacing=state.input.slot_spacing;
        if(state.input.row.configuration.variant==ListItemVariant::detail_actions||detail_controls(state.input)||state.input.row.configuration.variant==ListItemVariant::task||state.input.row.configuration.variant==ListItemVariant::form||preview_row(state.input))c.slot_width=state.input.action_width;
        if(has_menu_entries(state.input))c.slots[2].menu_indicator=true;
        c.enabled=state.input.row.configuration.enabled;value.icon_source=state.input.row.icon_source;value.icon_revision=state.input.row.icon_revision;return value;
    }
    [[nodiscard]] static ToolbarButtonInput header_more_input(const State& state){
        ToolbarButtonInput value;const auto& slot=state.input.more_menu;auto& c=value.configuration;
        if(const auto& a=state.input.accent){c.palette.primary=a->primary;c.palette.pressed=a->subtle;c.palette.focus=a->primary;}
        c.text=slot.text;if(has_menu_entries(state.input)||slot.menu_indicator)c.text+=slot.text.empty()?"⌄":" ⌄";
        c.accessibility_name=Policy::native_display_text(slot.accessibility_name.empty()?(slot.text.empty()?slot.icon_name:slot.text):slot.accessibility_name);
        c.width=state.input.action_width;c.height=22;c.enabled=state.input.row.configuration.enabled&&slot.enabled&&(!c.text.empty()||!slot.icon_name.empty());
        c.visible=state.input.row.configuration.variant==ListItemVariant::resource&&has_action(state.input,2)&&state.input.row.configuration.visible;
        if(c.visible)value.icon_name=slot.icon_name;value.icon_source=state.input.row.icon_source;value.icon_revision=state.input.row.icon_revision;return value;
    }
    [[nodiscard]] static ii::core::Result<void> paint_detail(const State& state,ii::gui::WidgetDrawing& drawing,
            const ii::gui::WidgetPaintState& view,const ListItemDrawing& row){
        try{
            const auto geometry=ListItemDetailActions::geometry(detail_configuration(state),view.size.width,view.size.height);
            if(!geometry)return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid detail item geometry");
            const auto& c=state.input.row.configuration;const bool enabled=view.enabled&&c.enabled;
            const auto text=ii::gui::srgb_to_linear(enabled?c.palette.text:c.palette.disabled_text);
            const auto caption=ii::gui::srgb_to_linear(enabled?c.palette.description:c.palette.disabled_text);
            const ii::gui::Rect clip{0,0,float(view.size.width),float(view.size.height)};
            const auto emit=[&](ListItemRect bounds,const std::string& value,bool body){
                if(value.empty()||bounds.width<=0||bounds.height<=0)return;
                drawing.texts.push_back({.bounds=list_item_rect(bounds),.text=Policy::native_display_text(value),.color=body?text:caption,
                    .font_size=body?13.f:11.f,.font_weight=body?500u:400u,.alignment=ii::gui::TextAlignment::left,
                    .font_family=view.font_family,.line_height=body?13.f:11.f,.elide_right=true,.clip=clip});
            };
            if(row.icon&&geometry->leading.width>0&&geometry->leading.height>0)
                drawing.images.push_back({.bounds=list_item_rect(geometry->leading),.image=row.icon,.opacity=enabled?1.f:.35f});
            if(state.bookmark&&geometry->bookmark.width>0&&geometry->bookmark.height>0)
                drawing.images.push_back({.bounds=list_item_rect(geometry->bookmark),.image=state.bookmark,.opacity=enabled?1.f:.35f});
            const auto& display=row.model.configuration();emit(geometry->label,display.label,true);
            if(geometry->description.width>0&&geometry->description.height>0&&!display.description.empty()){
                auto layout=NativeTextLayout::create(display.description,{.font_family=view.font_family,.font_size=11,.line_height=11},false,geometry->description.width);
                if(layout){
                    const auto lines=std::min<std::size_t>(2,layout->lines().size());
                    for(std::size_t i=0;i<lines;++i){auto line=std::string(layout->display_text(i));
                        if(i==1&&layout->lines().size()>2)line+="…";
                        auto bounds=geometry->description;bounds.y+=11*double(i);bounds.height=(std::max)(0.,(std::min)(11.,geometry->description.height-11*double(i)));
                        emit(bounds,line,false);}
                }else {auto bounds=geometry->description;bounds.height=(std::min)(11.,bounds.height);emit(bounds,display.description,false);}
            }
            const std::array<const std::string*,3> metadata{&state.input.date_text,&state.input.metadata1,&state.input.metadata2};
            for(std::size_t i=0;i<metadata.size();++i)emit(geometry->metadata[i],*metadata[i],false);
            emit(geometry->status,state.input.status_text,false);return {};
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Detail item drawing failed",std::current_exception());}
    }
    [[nodiscard]] static ii::core::Result<void> paint_controls(const State& state,ii::gui::WidgetDrawing& drawing,
            const ii::gui::WidgetPaintState& view,const ListItemDrawing& row){
        try{
            const auto geometry=ListItemDetailControls::geometry(controls_configuration(state),view.size.width,view.size.height);
            if(!geometry)return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid detail control geometry");
            const auto& c=state.input.row.configuration;const bool enabled=view.enabled&&c.enabled;
            const auto text=ii::gui::srgb_to_linear(enabled?c.palette.text:c.palette.disabled_text);
            const auto caption=ii::gui::srgb_to_linear(enabled?c.palette.description:c.palette.disabled_text);
            const ii::gui::Rect clip{0,0,float(view.size.width),float(view.size.height)};
            const auto emit=[&](ListItemRect bounds,const std::string& value,bool body){
                if(value.empty()||bounds.width<=0||bounds.height<=0)return;
                drawing.texts.push_back({.bounds=list_item_rect(bounds),.text=Policy::native_display_text(value),.color=body?text:caption,
                    .font_size=body?13.f:11.f,.font_weight=body?500u:400u,.alignment=ii::gui::TextAlignment::left,
                    .font_family=view.font_family,.line_height=body?13.f:11.f,.elide_right=true,.clip=clip});
            };
            if(row.icon&&geometry->leading.width>0&&geometry->leading.height>0)
                drawing.images.push_back({.bounds=list_item_rect(geometry->leading),.image=row.icon,.opacity=enabled?1.f:.35f});
            const auto& display=row.model.configuration();emit(geometry->label,display.label,true);emit(geometry->description,display.description,false);
            if(c.variant==ListItemVariant::detail_quantity){emit(geometry->quantity_caption,state.input.quantity_label,false);
                emit(geometry->status,state.input.option_label,false);}
            else{emit(geometry->mode_caption,state.input.mode_label,false);emit(geometry->scope_caption,state.input.scope_label,false);
                emit(geometry->status,state.input.status_text,false);}
            return {};
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Detail control drawing failed",std::current_exception());}
    }
    [[nodiscard]] static ii::core::Result<void> paint_task(const State& state,ii::gui::WidgetDrawing& drawing,
            const ii::gui::WidgetPaintState& view,const ListItemDrawing& row){
        try{
            const auto geometry=ListItemTask::geometry(task_configuration(state.input),view.size.width,view.size.height);
            if(!geometry)return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid task item geometry");
            const auto& c=state.input.row.configuration;const bool enabled=view.enabled&&c.enabled;
            const auto text=ii::gui::srgb_to_linear(enabled?c.palette.text:c.palette.disabled_text);
            const auto caption=ii::gui::srgb_to_linear(enabled?c.palette.description:c.palette.disabled_text);
            const ii::gui::Rect clip{0,0,float(view.size.width),float(view.size.height)};
            const auto emit=[&](ListItemRect bounds,const std::string& value,bool body){
                if(value.empty()||bounds.width<=0||bounds.height<=0)return;
                drawing.texts.push_back({.bounds=list_item_rect(bounds),.text=Policy::native_display_text(value),.color=body?text:caption,
                    .font_size=body?13.f:11.f,.font_weight=body?500u:400u,.alignment=ii::gui::TextAlignment::left,
                    .font_family=view.font_family,.line_height=body?13.f:11.f,.elide_right=true,.clip=clip});
            };
            const auto& display=row.model.configuration();emit(geometry->label,display.label,true);
            if(geometry->description.width>0&&geometry->description.height>0&&!display.description.empty()){
                auto layout=NativeTextLayout::create(display.description,{.font_family=view.font_family,.font_size=11,.line_height=11},false,geometry->description.width);
                if(layout){const auto lines=std::min<std::size_t>(2,layout->lines().size());
                    for(std::size_t i=0;i<lines;++i){auto line=std::string(layout->display_text(i));if(i==1&&layout->lines().size()>2)line+="…";
                        auto bounds=geometry->description;bounds.y+=11*double(i);bounds.height=(std::max)(0.,(std::min)(11.,geometry->description.height-11*double(i)));
                        emit(bounds,line,false);}
                }else{auto bounds=geometry->description;bounds.height=(std::min)(11.,bounds.height);emit(bounds,display.description,false);}
            }
            const std::array<const std::string*,3> metadata{&state.input.date_text,&state.input.metadata1,&state.input.metadata2};
            for(std::size_t i=0;i<metadata.size();++i)emit(geometry->metadata[i],*metadata[i],false);
            emit(geometry->status,state.input.quantity_label,false);return {};
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Task item drawing failed",std::current_exception());}
    }
    [[nodiscard]] static ii::core::Result<void> paint_preview(const State& state,ii::gui::WidgetDrawing& drawing,
            const ii::gui::WidgetPaintState& view,const ListItemDrawing& row){
        try{
            const auto geometry=ListItemPreviewRow::geometry(preview_configuration(state),view.size.width,view.size.height);
            if(!geometry)return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid preview item geometry");
            const auto& c=state.input.row.configuration;const bool enabled=view.enabled&&c.enabled;
            const auto text=ii::gui::srgb_to_linear(enabled?c.palette.text:c.palette.disabled_text);
            const auto caption=ii::gui::srgb_to_linear(enabled?c.palette.description:c.palette.disabled_text);
            const ii::gui::Rect clip{0,0,float(view.size.width),float(view.size.height)};
            const auto emit=[&](ListItemRect bounds,const std::string& value,bool body){
                if(value.empty()||bounds.width<=0||bounds.height<=0)return;
                drawing.texts.push_back({.bounds=list_item_rect(bounds),.text=Policy::native_display_text(value),.color=body?text:caption,
                    .font_size=body?13.f:11.f,.font_weight=body?500u:400u,.alignment=ii::gui::TextAlignment::left,
                    .font_family=view.font_family,.line_height=body?13.f:11.f,.elide_right=true,.clip=clip});
            };
            if(!state.custom_preview&&geometry->preview.width>0&&geometry->preview.height>0){
                const auto color=ii::gui::srgb_to_linear(Policy::panel_surface_alt());
                const auto radius=float((std::min)({6.,geometry->preview.width/2,geometry->preview.height/2}));
                drawing.panels.push_back({list_item_rect(geometry->preview),color,color,radius});
                if(state.preview){const auto& p=geometry->preview;const auto cap=state.input.preview?(std::min)(p.width,p.height):
                        (std::min)({p.width,p.height,state.input.detail_icon_size});
                    const auto scale=(std::min)(cap/double(state.preview->width()),cap/double(state.preview->height()));
                    const auto w=double(state.preview->width())*scale,h=double(state.preview->height())*scale;
                    drawing.images.push_back({.bounds={float(p.x+(p.width-w)/2),float(p.y+(p.height-h)/2),float(w),float(h)},
                        .image=state.preview,.opacity=enabled?1.f:.35f,.radius=state.input.preview?float((std::min)({double(radius),w/2,h/2})):0.f});}
            }
            const auto& display=row.model.configuration();emit(geometry->label,display.label,true);
            if(c.variant==ListItemVariant::media)emit(geometry->description,display.description,false);
            else if(geometry->description.width>0&&geometry->description.height>0&&!display.description.empty()){
                auto layout=NativeTextLayout::create(display.description,{.font_family=view.font_family,.font_size=11,.line_height=11},false,geometry->description.width);
                if(layout){const auto lines=std::min<std::size_t>(2,layout->lines().size());
                    for(std::size_t i=0;i<lines;++i){auto line=std::string(layout->display_text(i));if(i==1&&layout->lines().size()>2)line+="…";
                        auto bounds=geometry->description;bounds.y+=11*double(i);bounds.height=(std::max)(0.,(std::min)(11.,geometry->description.height-11*double(i)));
                        emit(bounds,line,false);}
                }else{auto bounds=geometry->description;bounds.height=(std::min)(11.,bounds.height);emit(bounds,display.description,false);}
            }
            const std::array<const std::string*,3> metadata{&state.input.date_text,&state.input.metadata1,&state.input.metadata2};
            for(std::size_t i=0;i<metadata.size();++i)emit(geometry->metadata[i],*metadata[i],false);
            emit(geometry->duration,state.input.duration_text,false);emit(geometry->media_detail,state.input.media_detail,false);
            emit(geometry->status,state.input.status_text,false);return {};
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Preview item drawing failed",std::current_exception());}
    }
    [[nodiscard]] static ii::core::Result<void> paint_form(const State& state,ii::gui::WidgetDrawing& drawing,
            const ii::gui::WidgetPaintState& view,const ListItemDrawing& row){
        try{
            const auto geometry=ListItemForm::geometry(form_configuration(state.input),view.size.width,view.size.height);
            if(!geometry)return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid form item geometry");
            const auto& c=state.input.row.configuration;const bool enabled=view.enabled&&c.enabled;
            const auto text=ii::gui::srgb_to_linear(enabled?c.palette.text:c.palette.disabled_text);
            const auto caption=ii::gui::srgb_to_linear(enabled?c.palette.description:c.palette.disabled_text);
            const ii::gui::Rect clip{0,0,float(view.size.width),float(view.size.height)};
            const auto emit=[&](ListItemRect bounds,const std::string& value,bool body){
                if(value.empty()||bounds.width<=0||bounds.height<=0)return;
                drawing.texts.push_back({.bounds=list_item_rect(bounds),.text=Policy::native_display_text(value),.color=body?text:caption,
                    .font_size=body?13.f:11.f,.font_weight=body?500u:400u,.alignment=ii::gui::TextAlignment::left,
                    .font_family=view.font_family,.line_height=body?13.f:11.f,.elide_right=true,.clip=clip});
            };
            const auto& display=row.model.configuration();emit(geometry->label,display.label,true);emit(geometry->description,display.description,false);
            emit(geometry->field_label1,state.input.field_label1,false);emit(geometry->field_label2,state.input.field_label2,false);
            emit(geometry->status,state.input.status_text,false);return {};
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Form item drawing failed",std::current_exception());}
    }
    [[nodiscard]] static ii::core::Result<void> place_custom(const std::optional<ii::gui::Widget>& container,
            const std::optional<ii::gui::Widget>& content,bool shown,ListItemRect bounds){
        if(!container||!content)return {};
        shown=shown&&bounds.width>0&&bounds.height>0;
        if(auto visible=container->set_visible(shown);!visible)return visible;
        if(!shown)return {};
        if(auto frame=container->set_frame(list_item_rect(bounds));!frame)return frame;
        return content->set_frame({0,0,float(bounds.width),float(bounds.height)});
    }
    [[nodiscard]] static ii::core::Result<void> frames(const std::shared_ptr<State>& state){
        if(!state->row||!state->toggle||!state->checkbox||!state->actions||!state->header_more||!state->stepper||!state->selector||!state->unit_selector||!state->segments||!state->input_field||!state->input_field2)return ii::core::fail(ii::core::ErrorCode::closed,"List item controls are unavailable");
        const auto width=state->extent.width,total_height=state->extent.height;
        if(!std::isfinite(width)||!std::isfinite(total_height)||width<0||total_height<0||width>8192||total_height>8192)
            return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid list item extent");
        const auto height=body_height(*state);
        if(state->footer){
            const auto shown=footer_allowed(*state)&&state->input.row.configuration.visible&&width>0;
            if(auto visible=state->footer->set_visible(shown);!visible)return visible;
            if(shown){const auto h=(std::min)(state->footer_height,total_height-height),y=total_height-h;
                if(auto placed=state->footer->set_frame({0,float(y),float(width),float(h)});!placed)return placed;}
        }
        if(auto placed=state->row->root().set_frame({0,0,float(width),float(height)});!placed)return placed;
        const auto custom_trailing=trailing_override(*state);
        if(state->trailing_container){
            const auto right=(std::max)(0.,width-(std::min)(state->input.slot_padding,width));
            const auto w=(std::min)(trailing_width(*state),right),h=(std::min)(trailing_height(*state),height);
            if(auto placed=place_custom(state->trailing_container,state->custom_trailing,custom_trailing&&state->input.row.configuration.visible,
                                        {right-w,(height-h)/2,w,h});!placed)return placed;
        }
        if(state->preview_container&&!preview_row(state->input))
            if(auto hidden=state->preview_container->set_visible(false);!hidden)return hidden;
        const auto leading_shown=leading_row(state->input)&&state->input.show_leading_icon&&state->input.row.configuration.visible;
        const auto type=state->input.row.configuration.variant;
        if(state->leading_container&&(type!=ListItemVariant::detail_actions&&type!=ListItemVariant::detail_settings)){
            ListItemModel model;if(!model.set_configuration(row_input(*state).configuration))
                return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid leading item configuration");
            const auto geometry=model.geometry(width,height);if(!geometry)return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid leading item geometry");
            if(auto placed=place_custom(state->leading_container,state->custom_leading,leading_shown,geometry->icon);!placed)return placed;
        }
        if(state->input.row.configuration.variant!=ListItemVariant::resource)
            if(auto hidden=state->header_more->root().set_visible(false);!hidden)return hidden;
        if(state->input.row.configuration.variant!=ListItemVariant::form)
            if(auto hidden=state->input_field2->root().set_visible(false);!hidden)return hidden;
        if(state->input.row.configuration.variant==ListItemVariant::form){
            const auto geometry=ListItemForm::geometry(form_configuration(state->input),width,height);
            if(!geometry)return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid form item geometry");
            const auto place_rect=[](ii::gui::Widget widget,bool shown,ListItemRect bounds)->ii::core::Result<void>{
                shown=shown&&bounds.width>0&&bounds.height>0;
                if(auto visible=widget.set_visible(shown);!visible)return visible;
                return shown?widget.set_frame(list_item_rect(bounds)):ii::core::Result<void>{};
            };
            if(auto result=place_rect(state->toggle->root(),has_toggle(state->input),geometry->toggle);!result)return result;
            if(auto result=place_rect(state->checkbox->root(),false,{});!result)return result;
            if(auto result=place_rect(state->actions->root(),actions_width(state->input)>0,geometry->actions);!result)return result;
            if(auto result=place_rect(state->stepper->root(),false,{});!result)return result;
            if(auto result=place_rect(state->quantity_label,false,{});!result)return result;
            if(auto result=place_rect(state->selector->root(),has_selector(state->input)&&state->input.row.configuration.visible,geometry->selector);!result)return result;
            if(auto result=place_rect(state->unit_selector->root(),false,{});!result)return result;
            if(auto result=place_rect(state->segments->root(),false,{});!result)return result;
            if(auto result=place_rect(state->input_field->root(),has_input(state->input)&&state->input.row.configuration.visible,geometry->input1);!result)return result;
            return place_rect(state->input_field2->root(),has_input2(state->input)&&state->input.row.configuration.visible,geometry->input2);
        }
        if(preview_row(state->input)){
            const auto geometry=ListItemPreviewRow::geometry(preview_configuration(*state),width,height);
            if(!geometry)return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid preview item geometry");
            const auto place_rect=[](ii::gui::Widget widget,bool shown,ListItemRect bounds)->ii::core::Result<void>{
                shown=shown&&bounds.width>0&&bounds.height>0;
                if(auto visible=widget.set_visible(shown);!visible)return visible;
                return shown?widget.set_frame(list_item_rect(bounds)):ii::core::Result<void>{};
            };
            if(state->preview_container&&state->custom_preview){
                if(auto result=place_rect(*state->preview_container,state->input.show_preview&&state->input.row.configuration.visible,geometry->preview);!result)return result;
                if(auto frame=state->custom_preview->set_frame({0,0,float(geometry->preview.width),float(geometry->preview.height)});!frame)return frame;
            }
            if(auto result=place_rect(state->header_more->root(),has_action(state->input,2),geometry->more);!result)return result;
            if(auto result=place_rect(state->toggle->root(),false,{});!result)return result;
            if(auto result=place_rect(state->checkbox->root(),false,{});!result)return result;
            if(auto result=place_rect(state->actions->root(),actions_width(state->input)>0,geometry->actions);!result)return result;
            if(auto result=place_rect(state->stepper->root(),false,{});!result)return result;
            if(auto result=place_rect(state->quantity_label,false,{});!result)return result;
            if(auto result=place_rect(state->selector->root(),false,{});!result)return result;
            if(auto result=place_rect(state->unit_selector->root(),false,{});!result)return result;
            if(auto result=place_rect(state->segments->root(),has_segments(state->input)&&state->input.row.configuration.visible,geometry->segments);!result)return result;
            return place_rect(state->input_field->root(),false,{});
        }
        if(state->input.row.configuration.variant==ListItemVariant::task){
            const auto geometry=ListItemTask::geometry(task_configuration(state->input),width,height);
            if(!geometry)return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid task item geometry");
            const auto place_rect=[](ii::gui::Widget widget,bool shown,ListItemRect bounds)->ii::core::Result<void>{
                shown=shown&&bounds.width>0&&bounds.height>0;
                if(auto visible=widget.set_visible(shown);!visible)return visible;
                return shown?widget.set_frame(list_item_rect(bounds)):ii::core::Result<void>{};
            };
            if(auto result=place_rect(state->toggle->root(),false,{});!result)return result;
            if(auto result=place_rect(state->checkbox->root(),has_checkbox(state->input),geometry->selection);!result)return result;
            if(auto result=place_rect(state->actions->root(),actions_width(state->input)>0,geometry->actions);!result)return result;
            if(auto result=place_rect(state->stepper->root(),has_stepper(state->input),geometry->quantity_stepper);!result)return result;
            if(auto result=place_rect(state->quantity_label,true,geometry->quantity_label);!result)return result;
            if(auto result=place_rect(state->selector->root(),has_selector(state->input)&&state->input.row.configuration.visible,geometry->selector);!result)return result;
            if(auto result=place_rect(state->unit_selector->root(),false,{});!result)return result;
            if(auto result=place_rect(state->segments->root(),false,{});!result)return result;
            return place_rect(state->input_field->root(),false,{});
        }
        if(detail_controls(state->input)){
            const auto geometry=ListItemDetailControls::geometry(controls_configuration(*state),width,height);
            if(!geometry)return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid detail control geometry");
            if(auto placed=place_custom(state->leading_container,state->custom_leading,leading_shown,geometry->leading);!placed)return placed;
            const auto place_rect=[](ii::gui::Widget widget,bool shown,ListItemRect bounds)->ii::core::Result<void>{
                shown=shown&&bounds.width>0&&bounds.height>0;
                if(auto visible=widget.set_visible(shown);!visible)return visible;
                return shown?widget.set_frame(list_item_rect(bounds)):ii::core::Result<void>{};
            };
            if(auto result=place_rect(state->toggle->root(),has_toggle(state->input),geometry->toggle);!result)return result;
            if(auto result=place_rect(state->checkbox->root(),false,{});!result)return result;
            if(auto result=place_rect(state->actions->root(),actions_width(state->input)>0,geometry->actions);!result)return result;
            if(auto result=place_rect(state->stepper->root(),has_stepper(state->input),geometry->quantity_stepper);!result)return result;
            if(auto result=place_rect(state->quantity_label,has_quantity(state->input),geometry->quantity_label);!result)return result;
            const auto selector=state->input.row.configuration.variant==ListItemVariant::detail_quantity?geometry->header_selector:geometry->selector;
            if(auto result=place_rect(state->selector->root(),has_selector(state->input)&&state->input.row.configuration.visible,selector);!result)return result;
            if(auto result=place_rect(state->unit_selector->root(),has_unit_selector(state->input)&&state->input.row.configuration.visible,geometry->unit_selector);!result)return result;
            if(auto result=place_rect(state->segments->root(),has_segments(state->input)&&state->input.row.configuration.visible,geometry->segments);!result)return result;
            return place_rect(state->input_field->root(),false,{});
        }
        if(auto hidden=state->unit_selector->root().set_visible(false);!hidden)return hidden;
        if(auto hidden=state->segments->root().set_visible(false);!hidden)return hidden;
        const auto right=(std::max)(0.,width-(std::min)(state->input.slot_padding,width));
        const auto place=[&](ii::gui::Widget widget,bool shown,double preferred_width,double preferred_height)->ii::core::Result<void>{
            const auto w=(std::min)(preferred_width,right),h=(std::min)(preferred_height,height);shown=shown&&w>0&&h>0;
            if(auto visible=widget.set_visible(shown);!visible)return visible;
            if(shown)return widget.set_frame({float(right-w),float((height-h)/2),float(w),float(h)});return {};
        };
        if(auto result=place(state->toggle->root(),has_toggle(state->input)&&!custom_trailing,38,22);!result)return result;
        if(custom_trailing&&type==ListItemVariant::checkable){
            ListItemModel model;if(!model.set_configuration(row_input(*state).configuration))
                return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid checkable item configuration");
            const auto geometry=model.geometry(width,height);if(!geometry)return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid checkable item geometry");
            const bool shown=has_checkbox(state->input)&&geometry->icon.width>0&&geometry->icon.height>0;
            if(auto visible=state->checkbox->root().set_visible(shown);!visible)return visible;
            if(shown)if(auto frame=state->checkbox->root().set_frame(list_item_rect(geometry->icon));!frame)return frame;
        }else if(auto result=place(state->checkbox->root(),has_checkbox(state->input),18,18);!result)return result;
        if(state->input.row.configuration.variant==ListItemVariant::detail_actions){
            const auto geometry=ListItemDetailActions::geometry(detail_configuration(*state),width,height);
            if(!geometry)return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid detail item geometry");
            if(auto placed=place_custom(state->leading_container,state->custom_leading,leading_shown,geometry->leading);!placed)return placed;
            const auto& a=geometry->actions;const bool shown=a.width>0&&a.height>0;
            if(auto result=state->actions->root().set_visible(shown);!result)return result;
            if(shown)if(auto result=state->actions->root().set_frame(list_item_rect(a));!result)return result;
        }else if(auto result=place(state->actions->root(),!custom_trailing&&actions_width(state->input)>0,actions_width(state->input),22);!result)return result;
        if(auto result=place(state->stepper->root(),has_stepper(state->input)&&!custom_trailing,18,18);!result)return result;
        const auto step_width=(std::min)(18.,right),label_right=(std::max)(0.,right-(has_stepper(state->input)?step_width+state->input.quantity_spacing:0.));
        const auto label_width=(std::min)(state->input.quantity_width,label_right),label_height=(std::min)(16.,height);
        const auto label_shown=has_quantity(state->input)&&!custom_trailing&&label_width>0&&label_height>0;
        if(auto visible=state->quantity_label.set_visible(label_shown);!visible)return visible;
        if(label_shown)if(auto frame=state->quantity_label.set_frame({float(label_right-label_width),float((height-label_height)/2),float(label_width),float(label_height)});!frame)return frame;
        if(auto result=place(state->selector->root(),has_selector(state->input)&&!custom_trailing&&state->input.row.configuration.visible,state->input.selector_width,20);!result)return result;
        const auto actions=actions_width(state->input);const auto input_right=(std::max)(0.,right-(actions>0?actions+state->input.slot_spacing:0.));
        const auto input_width=(std::min)(state->input.input_width,input_right),input_height=(std::min)(state->input.input_height,height);
        const auto input_shown=has_input(state->input)&&!custom_trailing&&state->input.row.configuration.visible&&input_width>0&&input_height>0;
        if(auto visible=state->input_field->root().set_visible(input_shown);!visible)return visible;
        if(input_shown)return state->input_field->root().set_frame({float(input_right-input_width),float((height-input_height)/2),float(input_width),float(input_height)});
        return {};
    }
    [[nodiscard]] static ii::core::Result<bool> apply(const std::shared_ptr<State>& state,std::uint64_t revision){
        const auto step=[&](auto&& operation)->ii::core::Result<bool>{if(state->revision!=revision)return false;auto result=operation();
            if(!result)return std::unexpected(result.error());return state->revision==revision;};
        const auto size=intrinsic(*state);
        auto result=step([&]{return state->root.set_extent({size.width,size.height});});if(!result||!*result)return result;
        result=step([&]{return state->root.set_visible(state->input.row.configuration.visible);});if(!result||!*result)return result;
        if(state->footer){result=step([&]{return state->footer->set_enabled(state->input.row.configuration.enabled);});if(!result||!*result)return result;}
        if(state->preview_container){result=step([&]{return state->preview_container->set_enabled(state->input.row.configuration.enabled);});if(!result||!*result)return result;}
        if(state->leading_container){result=step([&]{return state->leading_container->set_enabled(state->input.row.configuration.enabled);});if(!result||!*result)return result;}
        if(state->trailing_container){result=step([&]{return state->trailing_container->set_enabled(state->input.row.configuration.enabled);});if(!result||!*result)return result;}
        result=step([&]()->ii::core::Result<void>{
            std::shared_ptr<const ii::gui::RasterImage> icon;
            if(state->input.row.configuration.variant==ListItemVariant::detail_actions&&state->input.show_bookmark){
                auto resolved=state->bookmarks.resolve(state->input.bookmark_icon_name,state->input.row.icon_source,state->input.detail_icon_size);
                if(!resolved)return std::unexpected(resolved.error());icon=std::move(*resolved);
            }
            if(state->revision==revision)state->bookmark=std::move(icon);return {};
        });if(!result||!*result)return result;
        result=step([&]()->ii::core::Result<void>{
            std::shared_ptr<const ii::gui::RasterImage> image;
            if(preview_row(state->input)&&state->input.show_preview&&!state->custom_preview){
                image=state->input.preview;
                if(!image&&!state->input.preview_icon_name.empty()){
                    auto resolved=state->preview_icons.resolve(state->input.preview_icon_name,state->input.row.icon_source,state->input.detail_icon_size);
                    if(!resolved)return std::unexpected(resolved.error());image=std::move(*resolved);
                }
            }
            if(state->revision==revision)state->preview=std::move(image);return {};
        });if(!result||!*result)return result;
        result=step([&]{return state->row->set_options(row_input(*state));});if(!result||!*result)return result;
        result=step([&]{return state->toggle->set_options(toggle_input(*state));});if(!result||!*result)return result;
        result=step([&]{return state->checkbox->set_options(checkbox_input(*state));});if(!result||!*result)return result;
        result=step([&]{return state->actions->set_options(actions_input(*state));});if(!result||!*result)return result;
        result=step([&]{return state->header_more->set_options(header_more_input(*state));});if(!result||!*result)return result;
        result=step([&]{return state->stepper->set_options(stepper_input(*state));});if(!result||!*result)return result;
        result=step([&]{return state->quantity_label.set_label_options(quantity_label_input(*state));});if(!result||!*result)return result;
        result=step([&]{return state->selector->set_options(selector_input(*state));});if(!result||!*result)return result;
        result=step([&]{return state->unit_selector->set_options(unit_selector_input(*state));});if(!result||!*result)return result;
        result=step([&]{return state->segments->set_options(segments_input(*state));});if(!result||!*result)return result;
        result=step([&]()->ii::core::Result<void>{
            if(!state->action_menu)return {};
            for(auto& item:state->action_menu->items){
                auto options=item.options();if(!options)return std::unexpected(options.error());auto previous=*options;
                menu_item_accent(*options,state->input.accent);
                if(*options!=previous)if(auto changed=item.set_options(std::move(*options));!changed)return changed;
                if(state->revision!=revision)return {};
            }
            return {};
        });if(!result||!*result)return result;
        result=step([&]()->ii::core::Result<void>{auto options=input_field_input(*state);
            if(state->applied_input&&*state->applied_input==options)return {};
            auto previous=state->applied_input;if(previous)previous->palette=options.palette;
            auto changed=previous&&*previous==options?state->input_field->set_palette(options.palette):state->input_field->set_options(options);
            if(changed&&state->revision==revision)state->applied_input=std::move(options);return changed;});
        if(!result||!*result)return result;
        result=step([&]()->ii::core::Result<void>{auto options=input_field_input(*state,true);
            if(state->applied_input2&&*state->applied_input2==options)return {};
            auto previous=state->applied_input2;if(previous)previous->palette=options.palette;
            auto changed=previous&&*previous==options?state->input_field2->set_palette(options.palette):state->input_field2->set_options(options);
            if(changed&&state->revision==revision)state->applied_input2=std::move(options);return changed;});
        if(!result||!*result)return result;
        result=step([&]{return frames(state);});if(!result||!*result)return result;
        return step([&]{return state->row->root().request_repaint();});
    }
    enum class NaturalView { leading,preview,trailing };
    [[nodiscard]] static ii::gui::Size& natural_size(State& state,NaturalView view)noexcept{
        if(view==NaturalView::leading)return state.leading_natural;
        if(view==NaturalView::preview)return state.preview_natural;
        return state.trailing_natural;
    }
    [[nodiscard]] static bool uses_natural_size(const State& state,NaturalView view)noexcept{
        if(view==NaturalView::leading)return state.has_custom_leading&&!state.input.leading_size&&state.input.show_leading_icon&&leading_row(state.input);
        if(view==NaturalView::preview)return state.custom_preview&&!state.input.preview_size&&state.input.show_preview&&preview_row(state.input);
        return trailing_override(state)&&(!state.input.trailing_width||!state.input.trailing_height);
    }
    [[nodiscard]] static ii::core::Result<void> observe_natural(const std::shared_ptr<State>& state,ii::gui::Widget custom,NaturalView view){
        auto initial=custom.interaction();if(!initial)return std::unexpected(initial.error());
        if(!std::isfinite(initial->implicit_size.width)||!std::isfinite(initial->implicit_size.height))
            return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid custom list view measurement");
        natural_size(*state,view)={std::clamp(initial->implicit_size.width,0.,8192.),std::clamp(initial->implicit_size.height,0.,8192.)};
        std::weak_ptr<State> weak=state;
        return custom.set_interaction_handler([weak,view](const ii::gui::WidgetInteraction& interaction){
            auto state=weak.lock();if(!state||!state->live||!interaction.alive)return;
            const auto natural=interaction.implicit_size;
            if(!std::isfinite(natural.width)||!std::isfinite(natural.height))return;
            const ii::gui::Size next{std::clamp(natural.width,0.,8192.),std::clamp(natural.height,0.,8192.)};
            auto& current=natural_size(*state,view);if(next==current)return;
            const auto previous=current,old_extent=state->extent;const auto revision=state->revision;current=next;
            if(!uses_natural_size(*state,view))return;
            try{
                auto updated=apply(state,revision);
                if(!updated&&state->revision==revision){current=previous;state->extent=old_extent;
                    (void)apply(state,revision);state->failure=updated.error();}
            }catch(...){
                if(state->revision==revision){current=previous;state->extent=old_extent;
                    try{(void)apply(state,revision);}catch(...){}
                    state->failure=ii::core::Error{ii::core::ErrorCode::failed,"Custom list view measurement update failed",std::current_exception()};}
            }
        });
    }
    [[nodiscard]] static ii::core::Result<void> update(const std::shared_ptr<State>& state,ListItemCompositeInput input){
        if(state->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"List item requires its owner thread");
        try{
            if(auto live=state->root.snapshot();!live)return std::unexpected(live.error());if(!valid(input))return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid list item composition");
            if(input==state->input)return {};
            auto echo=state->input;echo.checked=input.checked;echo.quantity=input.quantity;echo.selector_index=input.selector_index;echo.selector.selected_index=input.selector.selected_index;
            echo.unit_index=input.unit_index;echo.unit_selector.selected_index=input.unit_selector.selected_index;echo.segment_index=input.segment_index;
            echo.input_text=input.input_text;echo.input_text2=input.input_text2;
            const bool checked_echo=input.checked==state->input.checked||(input.checked&&*input.checked==state->checked);
            const bool quantity_echo=input.quantity==state->input.quantity||(input.quantity&&*input.quantity==state->quantity.configuration().value);
            const bool index_echo=input.selector_index==state->input.selector_index||(input.selector_index&&*input.selector_index==state->selector_index);
            const bool config_echo=input.selector.selected_index==state->input.selector.selected_index||input.selector.selected_index==state->selector_index;
            const bool unit_echo=input.unit_index==state->input.unit_index||(input.unit_index&&*input.unit_index==state->unit_index);
            const bool unit_config_echo=input.unit_selector.selected_index==state->input.unit_selector.selected_index||input.unit_selector.selected_index==state->unit_index;
            const bool segment_echo=input.segment_index==state->input.segment_index||(input.segment_index&&*input.segment_index==state->segment_index);
            const bool text_echo=input.input_text==state->input.input_text||(input.input_text&&*input.input_text==state->input_text);
            const bool text2_echo=input.input_text2==state->input.input_text2||(input.input_text2&&*input.input_text2==state->input_text2);
            if(echo==input&&checked_echo&&quantity_echo&&index_echo&&config_echo&&unit_echo&&unit_config_echo&&segment_echo&&text_echo&&text2_echo){
                state->input=std::move(input);publish_context(state);return {};
            }
            if(state->revision==(std::numeric_limits<std::uint64_t>::max)())return ii::core::fail(ii::core::ErrorCode::out_of_range,"List item revision exhausted");
            const auto previous=state->input;const auto old_checked=state->checked;const auto old_extent=state->extent;const auto old_quantity=state->quantity;
            const auto old_index=state->selector_index,old_unit=state->unit_index,old_segment=state->segment_index;
            const auto old_text=state->input_text,old_text2=state->input_text2;
            if(state->popup&&
               (input.more_items!=previous.more_items||input.menu_entries!=previous.menu_entries||
                (input.menu_entries&&(input.row.icon_source!=previous.row.icon_source||input.row.icon_revision!=previous.row.icon_revision))||
                !input.row.configuration.visible||!input.row.configuration.enabled||!has_action(input,2)||!input.more_menu.enabled||trailing_override(*state,input))){
                auto popup=state->popup->root().snapshot();if(!popup)return std::unexpected(popup.error());
                const auto observed=state->revision;
                if(popup->opened)if(auto closed=state->popup->root().close_popup();!closed)return closed;
                if(state->revision!=observed)return {};
            }
            auto next_checked=state->checked;
            if(input.checked!=previous.checked&&input.checked)next_checked=*input.checked;
            else if(input.row.configuration.variant!=previous.row.configuration.variant&&!input.checked)next_checked=default_checked(input);
            auto quantity_value=state->quantity.configuration().value;
            if(input.quantity!=previous.quantity&&input.quantity)quantity_value=*input.quantity;
            else quantity_value=std::clamp(quantity_value,input.minimum_quantity,input.maximum_quantity);
            auto next_quantity=state->quantity;(void)next_quantity.set_configuration(quantity_input(input,quantity_value));
            auto next_index=state->selector_index;
            if(input.selector_index!=previous.selector_index&&input.selector_index)next_index=*input.selector_index;
            else if(input.selector.options!=previous.selector.options||input.selector.selected_index!=previous.selector.selected_index)
                next_index=input.selector_index.value_or(input.selector.selected_index);
            auto next_unit=state->unit_index;
            if(input.unit_index!=previous.unit_index&&input.unit_index)next_unit=*input.unit_index;
            else if(input.unit_selector.options!=previous.unit_selector.options||input.unit_selector.selected_index!=previous.unit_selector.selected_index)
                next_unit=input.unit_index.value_or(input.unit_selector.selected_index);
            auto next_segment=state->segment_index;
            if(input.segment_index!=previous.segment_index&&input.segment_index)next_segment=*input.segment_index;
            else if(input.segments!=previous.segments)next_segment=input.segment_index.value_or(0);
            auto next_text=state->input_text;if(input.input_text!=previous.input_text&&input.input_text)next_text=*input.input_text;
            auto next_text2=state->input_text2;if(input.input_text2!=previous.input_text2&&input.input_text2)next_text2=*input.input_text2;
            const auto revision=++state->revision;state->input=std::move(input);state->checked=next_checked;state->quantity=next_quantity;state->selector_index=next_index;
            state->unit_index=next_unit;state->segment_index=next_segment;state->input_text=std::move(next_text);state->input_text2=std::move(next_text2);
            auto result=[&]()->ii::core::Result<bool>{try{return apply(state,revision);}
                catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"List item layout failed",std::current_exception());}}();
            if(!result){if(state->revision==revision){state->input=previous;state->checked=old_checked;state->quantity=old_quantity;state->selector_index=old_index;
                state->unit_index=old_unit;state->segment_index=old_segment;state->input_text=old_text;state->input_text2=old_text2;state->extent=old_extent;(void)apply(state,revision);}return std::unexpected(result.error());}
            if(*result&&state->live&&state->revision==revision){state->failure.reset();publish_context(state);}return {};
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"List item update failed",std::current_exception());}
    }
    static void activated(const std::shared_ptr<State>& state)noexcept{
        try{if(state->live&&state->callbacks.activated){auto callback=state->callbacks.activated;callback();}}
        catch(...){try{state->failure=ii::core::Error{ii::core::ErrorCode::failed,"List item activation failed",std::current_exception()};}catch(...) {}}
    }
    static void menu_selected(const std::shared_ptr<State>& state,const std::shared_ptr<ActionMenu>& assembly,
                              const std::vector<std::size_t>& path)noexcept{
        try{
            if(!state->live||state->action_menu!=assembly||path.empty()||trailing_override(*state)||
               !state->input.row.configuration.visible||!state->input.row.configuration.enabled||!has_action(state->input,2)||
               !state->input.more_menu.enabled||state->input.more_items!=assembly->options||state->input.menu_entries!=assembly->authored||
               (assembly->authored&&(state->input.row.icon_source!=assembly->icon_source||state->input.row.icon_revision!=assembly->icon_revision)))return;
            const auto* entries=&assembly->entries;const ListItemMenuEntry* entry=nullptr;
            for(auto index:path){if(index>=entries->size())return;entry=&(*entries)[index];
                if(entry->divider||!entry->visible||!entry->enabled||entry->state==MenuEntryState::inactive)return;entries=&entry->children;}
            if(!entry||path.back()>std::size_t((std::numeric_limits<int>::max)()))return;
            const auto index=int(path.back());const ListItemOption item{entry->label,entry->enabled};
            ListItemMenuSelection selection{index,item};
            if(assembly->authored){selection.path=path;selection.entry=*entry;}
            const auto revision=state->revision;
            const auto current=[&]{return state->live&&state->revision==revision&&state->action_menu==assembly;};
            if(entry->command)entry->command->invoke(selection);if(!current())return;
            if(state->callbacks.menu_item_triggered){auto callback=state->callbacks.menu_item_triggered;callback(index,item);}
            if(!current())return;
            if(state->callbacks.menu_selection_triggered){auto callback=state->callbacks.menu_selection_triggered;callback(selection);}
            if(!current())return;
            if(state->callbacks.action_event){auto callback=state->callbacks.action_event;const ListItemActionPayload payload=selection;callback("menuItem",payload);}
        }catch(...){try{state->failure=ii::core::Error{ii::core::ErrorCode::failed,"List item menu callback failed",std::current_exception()};}catch(...) {}}
    }
    static void menu_item_accent(MenuItemInput& input,const std::optional<WindowAccentPalette>& accent){
        const MenuItemPalette defaults;
        input.configuration.palette.selected=accent?accent->primary:defaults.selected;
        input.configuration.palette.pressed=accent?accent->subtle:defaults.pressed;
        input.configuration.palette.focus=accent?accent->primary:defaults.focus;
    }
    [[nodiscard]] static ii::core::Result<void> menu_entries(ii::gui::WidgetBuilder& builder,ii::gui::Widget parent,
            const std::vector<ListItemMenuEntry>& entries,std::vector<std::size_t> path,const std::shared_ptr<State>& state,
            const std::shared_ptr<ActionMenu>& assembly,const std::function<ii::core::Result<void>(MenuItemPort)>& add){
        for(std::size_t index=0;index<entries.size();++index){
            const auto& entry=entries[index];auto child_path=path;child_path.push_back(index);
            std::string suffix;for(auto part:child_path)suffix+="-"+std::to_string(part);
            if(entry.divider){
                ii::gui::DividerConfiguration configuration;const auto divider_color=Policy::panel_surface_panel();configuration.color={divider_color.red,divider_color.green,divider_color.blue,divider_color.alpha};configuration.length=141;configuration.cross_padding=1;configuration.line_padding=4;
                auto divider=ii::gui::NativeDivider::create(builder,parent,"divider"+suffix,configuration);if(!divider)return std::unexpected(divider.error());
                if(!entry.visible)if(auto hidden=divider->set_visible(false);!hidden)return hidden;
                auto port=NativeMenu::divider_port(divider->weak_group_port());if(!port)return std::unexpected(port.error());
                if(auto added=add(std::move(*port));!added)return added;assembly->dividers.push_back(*divider);continue;
            }
            MenuItemInput input;auto& c=input.configuration;c.label=entry.label;c.shortcut=entry.shortcut;
            menu_item_accent(input,state->input.accent);
            c.accessibility_name=entry.accessibility_name;c.enabled=entry.enabled;c.state=entry.state;c.direction=entry.direction;
            c.show_icon_slot=entry.show_icon_slot.value_or(true);
            c.key_visible=entry.show_shortcut.value_or(entry.shortcut.find_first_not_of(" \t\r\n")!=std::string::npos);
            c.has_children=entry.has_child_items.value_or(!entry.children.empty());c.show_chevron=entry.show_chevron.value_or(c.has_children);
            c.expanded=entry.expanded;c.compact=true;input.visible=entry.visible;input.close_on_trigger=entry.close_on_trigger;
            if(c.show_icon_slot&&!entry.icon_name.empty()){
                IconCache icons;auto image=icons.resolve(entry.icon_name,assembly->icon_source,c.icon_size);
                if(!image)return std::unexpected(image.error());input.icon=std::move(*image);
            }
            c.icon_name.clear();
            auto view=NativeMenuItem::create(builder,parent,"option"+suffix,std::move(input));if(!view)return std::unexpected(view.error());
            assembly->item_widgets.emplace(child_path,view->root());assembly->items.push_back(*view);
            if(!entry.children.empty()){
                std::weak_ptr<State> weak=state;std::weak_ptr<ActionMenu> generation=assembly;
                MenuInput options;options.configuration.show_icon_slot=false;options.configuration.accessibility_name=entry.label;
                auto submenu=NativeMenu::create(builder,view->root(),"submenu"+suffix,options,{.triggered=[weak,generation,child_path](int selected){
                    auto owner=weak.lock();auto current=generation.lock();if(!owner||!current||selected<0)return;
                    auto selected_path=child_path;selected_path.push_back(std::size_t(selected));menu_selected(owner,current,selected_path);}});
                if(!submenu)return std::unexpected(submenu.error());assembly->submenu_widgets.emplace(child_path,submenu->root());
                auto add_child=[&submenu](MenuItemPort port){return submenu->register_item(std::move(port));};
                if(auto built=menu_entries(builder,submenu->content_root(),entry.children,child_path,state,assembly,add_child);!built)return built;
                if(auto finalized=submenu->finalize();!finalized)return finalized;
                if(auto attached=view->set_submenu(submenu->weak_popup_port());!attached)return attached;
                assembly->submenus.push_back(*submenu);
            }
            auto port=view->weak_group_port();auto present=std::move(port.present);
            const bool show_icon=entry.show_icon_slot.value_or(true);
            const bool selected=entry.state==MenuEntryState::selected;
            port.present=[present=std::move(present),show_icon,selected](MenuItemPresentation value){
                value.show_icon_slot=show_icon;if(selected)value.selected=true;return present(value);
            };
            if(auto added=add(std::move(port));!added)return added;
        }
        return {};
    }
    [[nodiscard]] static ii::core::Result<void> render_menu(const std::shared_ptr<State>& state){
        if(state->action_menu&&state->action_menu->options==state->input.more_items&&state->action_menu->authored==state->input.menu_entries&&
           (!state->input.menu_entries||(state->action_menu->icon_source==state->input.row.icon_source&&state->action_menu->icon_revision==state->input.row.icon_revision)))return {};
        try{
            auto assembly=std::make_shared<ActionMenu>();assembly->options=state->input.more_items;assembly->authored=state->input.menu_entries;
            assembly->icon_source=state->input.row.icon_source;assembly->icon_revision=state->input.row.icon_revision;
            if(assembly->authored)assembly->entries=*assembly->authored;
            else for(const auto& option:assembly->options){ListItemMenuEntry entry;entry.label=option.text;entry.enabled=option.enabled;entry.show_icon_slot=false;assembly->entries.push_back(std::move(entry));}
            const auto observed=state->revision;auto previous=state->action_menu;
            const auto current=[&]{return state->live&&state->revision==observed&&state->action_menu==previous;};
            ii::gui::Widget group;std::vector<MenuItemPort> ports;
            auto built=state->menu_host.set_content([&](ii::gui::WidgetBuilder& builder)->ii::core::Result<void>{
                auto root=builder.vstack({},"options",{.spacing=2,.alignment=ii::gui::StackAlignment::stretch});
                if(!root)return std::unexpected(root.error());group=*root;ports.reserve(assembly->entries.size());
                auto add=[&](MenuItemPort port)->ii::core::Result<void>{port.layout_groups.push_back(group);ports.push_back(std::move(port));return {};};
                if(auto result=menu_entries(builder,*root,assembly->entries,{},state,assembly,add);!result)return result;
                if(!current())return ii::core::fail(ii::core::ErrorCode::cancelled,"List item changed while building its menu");return {};
            },[&]()->ii::core::Result<void>{
                if(!current())return ii::core::fail(ii::core::ErrorCode::cancelled,"List item changed before menu publication");
                assembly->ports=ports;
                if(auto published=state->menu_section(std::move(ports));!published)return published;
                if(!current()){
                    if(state->live&&state->action_menu==previous){
                        auto restored=state->menu_section(previous?previous->ports:std::vector<MenuItemPort>{});
                        if(!restored)return restored;
                    }
                    return ii::core::fail(ii::core::ErrorCode::cancelled,"List item changed during menu publication");
                }
                return {};
            });
            if(!built)return built;state->action_menu=std::move(assembly);return {};
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"List item menu construction failed",std::current_exception());}
    }
    static void triggered(const std::shared_ptr<State>& state,std::size_t index,const ListFooterSlot& slot)noexcept{
        try{if(!state->live||index>2||trailing_override(*state)||!has_action(state->input,index)||!state->input.row.configuration.enabled)return;
            const auto revision=state->revision;
            std::optional<ListItemActionValues> values;
            if(state->callbacks.action_values_triggered||state->callbacks.action_event)values=action_values_of(*state);
            if(state->callbacks.action_triggered){auto callback=state->callbacks.action_triggered;callback(index==0?"primary":index==1?"secondary":"more",slot);}
            if(values&&state->live&&state->revision==revision&&state->callbacks.action_values_triggered){auto callback=state->callbacks.action_values_triggered;
                callback(index==0?"primary":index==1?"secondary":"menu",*values);}
            if(values&&state->live&&state->revision==revision&&state->callbacks.action_event){auto callback=state->callbacks.action_event;
                const ListItemActionPayload payload=*values;callback(index==0?"primary":index==1?"secondary":"menu",payload);}
            if(index!=2||!state->live||state->revision!=revision||!state->input.row.configuration.visible||!has_menu_entries(state->input))return;
            auto previous_menu=state->action_menu;
            if(auto result=render_menu(state);!result){if(state->live&&state->revision==revision&&state->action_menu==previous_menu)state->failure=result.error();return;}
            if(!state->live||state->revision!=revision||!state->action_menu||!state->popup)return;
            auto anchor=state->input.row.configuration.variant==ListItemVariant::resource?
                ii::core::Result<ii::gui::Widget>(state->header_more->root()):state->actions->slot(2);
            if(!anchor){state->failure=anchor.error();return;}
            auto snapshot=anchor->snapshot();if(!snapshot){state->failure=snapshot.error();return;}
            if(auto opened=state->popup->root().open_for(*anchor,0,snapshot->bounds.height);!opened)state->failure=opened.error();}
        catch(...){try{state->failure=ii::core::Error{ii::core::ErrorCode::failed,"List item action failed",std::current_exception()};}catch(...) {}}
    }
    static void changed(const std::shared_ptr<State>& state,bool value)noexcept{
        try{
            if(!state->live||!state->input.row.configuration.enabled||!state->input.row.configuration.visible)return;
            if(trailing_override(*state)&&state->input.row.configuration.variant==ListItemVariant::toggle)return;
            if((has_toggle(state->input)&&!state->input.toggle_enabled)||(has_checkbox(state->input)&&!state->input.selection_enabled))return;
            if(!has_toggle(state->input)&&!has_checkbox(state->input))return;
            if(state->revision==(std::numeric_limits<std::uint64_t>::max)())throw std::out_of_range("List item revision exhausted");
            const auto old=state->checked;const auto revision=++state->revision;state->checked=value;auto result=apply(state,revision);
            if(!result){if(state->revision==revision){state->checked=old;(void)apply(state,revision);}state->failure=result.error();return;}
            if(!*result||!state->live||state->revision!=revision)return;
            publish_context(state);if(!state->live||state->revision!=revision)return;
            if(state->callbacks.checked_changed){auto callback=state->callbacks.checked_changed;callback(value);}
            if(state->live&&state->revision==revision&&state->callbacks.edited){auto callback=state->callbacks.edited;const ListItemValue edit=value;callback("checked",edit);}
        }catch(...){try{state->failure=ii::core::Error{ii::core::ErrorCode::failed,"List item value change failed",std::current_exception()};}catch(...) {}}
    }
    static void stepped(const std::shared_ptr<State>& state,int direction)noexcept{
        try{
            if(!state->live||!state->input.row.configuration.visible||trailing_override(*state)||!has_stepper(state->input))return;
            auto next=state->quantity;const auto value=next.step(direction);if(!value)return;
            if(state->revision==(std::numeric_limits<std::uint64_t>::max)())throw std::out_of_range("List item revision exhausted");
            const auto old=state->quantity;const auto revision=++state->revision;state->quantity=next;auto result=apply(state,revision);
            if(!result){if(state->revision==revision){state->quantity=old;(void)apply(state,revision);}state->failure=result.error();return;}
            if(!*result||!state->live||state->revision!=revision)return;
            publish_context(state);if(!state->live||state->revision!=revision)return;
            if(state->callbacks.quantity_changed){auto callback=state->callbacks.quantity_changed;callback(*value);}
            if(state->live&&state->revision==revision&&state->callbacks.edited){auto callback=state->callbacks.edited;const ListItemValue edit=*value;callback("quantity",edit);}
        }catch(...){try{state->failure=ii::core::Error{ii::core::ErrorCode::failed,"List item quantity change failed",std::current_exception()};}catch(...) {}}
    }
    static void selected(const std::shared_ptr<State>& state,int value)noexcept{
        try{
            if(!state->live||!state->input.row.configuration.visible||!state->input.row.configuration.enabled||!state->input.selector_enabled||
               !state->input.selector.enabled||trailing_override(*state)||!has_selector(state->input)||state->selector_index==value)return;
            if(state->revision==(std::numeric_limits<std::uint64_t>::max)())throw std::out_of_range("List item revision exhausted");
            const auto old=state->selector_index;const auto revision=++state->revision;state->selector_index=value;auto result=apply(state,revision);
            if(!result){if(state->revision==revision){state->selector_index=old;(void)apply(state,revision);}state->failure=result.error();return;}
            if(!*result||!state->live||state->revision!=revision)return;
            publish_context(state);if(!state->live||state->revision!=revision)return;
            if(state->callbacks.selector_index_changed){auto callback=state->callbacks.selector_index_changed;callback(value);}
            if(state->live&&state->revision==revision&&state->callbacks.edited){auto callback=state->callbacks.edited;const ListItemValue edit=std::int64_t(value);callback("selectorIndex",edit);}
        }catch(...){try{state->failure=ii::core::Error{ii::core::ErrorCode::failed,"List item selection failed",std::current_exception()};}catch(...) {}}
    }
    static void unit_selected(const std::shared_ptr<State>& state,int value)noexcept{
        try{
            if(!state->live||!state->input.row.configuration.visible||!state->input.row.configuration.enabled||!state->input.unit_selector_enabled||
               !state->input.unit_selector.enabled||!has_unit_selector(state->input)||state->unit_index==value)return;
            if(state->revision==(std::numeric_limits<std::uint64_t>::max)())throw std::out_of_range("List item revision exhausted");
            const auto old=state->unit_index;const auto revision=++state->revision;state->unit_index=value;auto result=apply(state,revision);
            if(!result){if(state->revision==revision){state->unit_index=old;(void)apply(state,revision);}state->failure=result.error();return;}
            if(!*result||!state->live||state->revision!=revision)return;
            publish_context(state);if(!state->live||state->revision!=revision)return;
            if(state->callbacks.unit_index_changed){auto callback=state->callbacks.unit_index_changed;callback(value);}
            if(state->live&&state->revision==revision&&state->callbacks.edited){auto callback=state->callbacks.edited;const ListItemValue edit=std::int64_t(value);callback("unitIndex",edit);}
        }catch(...){try{state->failure=ii::core::Error{ii::core::ErrorCode::failed,"List item unit selection failed",std::current_exception()};}catch(...) {}}
    }
    static void segment_selected(const std::shared_ptr<State>& state,int value)noexcept{
        try{
            if(!state->live||!state->input.row.configuration.visible||!state->input.row.configuration.enabled||!state->input.segments_enabled||
               !has_segments(state->input)||state->segment_index==value)return;
            if(state->revision==(std::numeric_limits<std::uint64_t>::max)())throw std::out_of_range("List item revision exhausted");
            const auto old=state->segment_index;const auto revision=++state->revision;state->segment_index=value;auto result=apply(state,revision);
            if(!result){if(state->revision==revision){state->segment_index=old;(void)apply(state,revision);}state->failure=result.error();return;}
            if(!*result||!state->live||state->revision!=revision)return;
            publish_context(state);if(!state->live||state->revision!=revision)return;
            if(state->callbacks.segment_index_changed){auto callback=state->callbacks.segment_index_changed;callback(value);}
            if(state->live&&state->revision==revision&&state->callbacks.edited){auto callback=state->callbacks.edited;const ListItemValue edit=std::int64_t(value);callback("segmentIndex",edit);}
        }catch(...){try{state->failure=ii::core::Error{ii::core::ErrorCode::failed,"List item segment selection failed",std::current_exception()};}catch(...) {}}
    }
    static void text_changed(const std::shared_ptr<State>& state,std::string value,bool second=false)noexcept{
        try{
            auto& current=second?state->input_text2:state->input_text;
            if(!state->live||!state->input.row.configuration.visible||!state->input.row.configuration.enabled||trailing_override(*state)||
               !(second?state->input.input2_enabled:state->input.input_enabled)||
               (second?state->input.input2_read_only:state->input.input_read_only)||
               !(second?has_input2(state->input):has_input(state->input))||current==value)return;
            if(state->revision==(std::numeric_limits<std::uint64_t>::max)())throw std::out_of_range("List item revision exhausted");
            const auto old=current;const auto revision=++state->revision;current=std::move(value);auto result=apply(state,revision);
            if(!result){if(state->revision==revision){current=old;(void)(second?state->input_field2:state->input_field)->set_text(old);(void)apply(state,revision);}state->failure=result.error();return;}
            if(!*result||!state->live||state->revision!=revision)return;
            publish_context(state);if(!state->live||state->revision!=revision)return;
            const auto callback=second?state->callbacks.input_text2_changed:state->callbacks.input_text_changed;if(callback)callback(current);
            if(state->live&&state->revision==revision&&state->callbacks.edited){auto edited=state->callbacks.edited;const ListItemValue edit=current;edited(second?"inputText2":"inputText1",edit);}
        }catch(...){try{state->failure=ii::core::Error{ii::core::ErrorCode::failed,"List item text edit failed",std::current_exception()};}catch(...) {}}
    }
    static void text_submitted(const std::shared_ptr<State>& state,bool second=false)noexcept{
        try{
            const auto& field=second?state->input_field2:state->input_field;
            if(!state->live||!state->input.row.configuration.visible||!state->input.row.configuration.enabled||trailing_override(*state)||
               !(second?state->input.input2_enabled:state->input.input_enabled)||
               !(second?has_input2(state->input):has_input(state->input))||!field)return;
            auto snapshot=field->snapshot();if(!snapshot){state->failure=snapshot.error();return;}if(snapshot->marked)return;
            const auto revision=state->revision;
            const ListItemInputSubmission submission{second?"inputText2":"inputText1",snapshot->text};
            const auto callback=second?state->callbacks.input_submitted2:state->callbacks.input_submitted;if(callback)callback(submission.text);
            if(state->live&&state->revision==revision&&state->callbacks.action_event){auto action=state->callbacks.action_event;
                const ListItemActionPayload payload=submission;action("inputSubmitted",payload);}
        }catch(...){try{state->failure=ii::core::Error{ii::core::ErrorCode::failed,"List item text submission failed",std::current_exception()};}catch(...) {}}
    }
    [[nodiscard]] static ii::core::Result<bool> context_edit(const std::shared_ptr<State>& state,std::string field,ListItemValue value){
        if(state->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"List item context requires its owner thread");
        if(!state->live)return ii::core::fail(ii::core::ErrorCode::closed,"List item context is closed");
        if(!state->input.row.configuration.enabled)return false;
        try{
            auto previous=action_values_of(*state);auto next=previous;ListItemValue emitted;
            if(field=="checked"){
                const auto* selected=std::get_if<bool>(&value);if(!selected)return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Checked requires a Boolean");
                next.checked=*selected;emitted=*selected;
            }else if(field=="quantity"){
                double number=0;if(const auto* real=std::get_if<double>(&value))number=*real;
                else if(const auto* integer=std::get_if<std::int64_t>(&value))number=double(*integer);
                else return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Quantity requires a number");
                if(!std::isfinite(number))return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Quantity must be finite");
                next.quantity=std::clamp(number,state->input.minimum_quantity,state->input.maximum_quantity);emitted=next.quantity;
            }else if(field=="selectorIndex"||field=="unitIndex"||field=="segmentIndex"){
                std::int64_t index=-1;
                if(const auto* integer=std::get_if<std::int64_t>(&value))index=*integer;
                else if(const auto* real=std::get_if<double>(&value);real&&std::isfinite(*real)&&*real>=0&&
                        *real<=(std::numeric_limits<int>::max)()&&std::trunc(*real)==*real)index=std::int64_t(*real);
                const auto count=field=="selectorIndex"?state->input.selector.options.size():field=="unitIndex"?state->input.unit_selector.options.size():state->input.segments.size();
                if(index<0||index>(std::numeric_limits<int>::max)()||(count?std::uint64_t(index)>=count:index!=0))
                    return ii::core::fail(ii::core::ErrorCode::out_of_range,"List item index is out of range");
                if(field=="selectorIndex")next.selector_index=int(index);else if(field=="unitIndex")next.unit_index=int(index);else next.segment_index=int(index);
                emitted=index;
            }else if(field=="inputText1"||field=="inputText2"){
                const auto* text=std::get_if<std::string>(&value);
                if(!text||!Policy::valid_accessibility_text(*text))return ii::core::fail(ii::core::ErrorCode::invalid_argument,"List item input requires valid bounded text");
                if(field=="inputText1")next.input_text1=*text;else next.input_text2=*text;emitted=*text;
            }else return false;
            if(next==previous)return false;
            if(state->revision==(std::numeric_limits<std::uint64_t>::max)())return ii::core::fail(ii::core::ErrorCode::out_of_range,"List item revision exhausted");
            auto quantity=state->quantity;auto quantity_config=quantity.configuration();quantity_config.value=next.quantity;
            if(!quantity.set_configuration(quantity_config))return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid list item quantity");
            const auto old_quantity=state->quantity;const auto revision=++state->revision;
            state->checked=next.checked;state->quantity=quantity;state->selector_index=next.selector_index;state->unit_index=next.unit_index;
            state->segment_index=next.segment_index;state->input_text=std::move(next.input_text1);state->input_text2=std::move(next.input_text2);
            auto result=[&]()->ii::core::Result<bool>{try{return apply(state,revision);}
                catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Contextual list item layout failed",std::current_exception());}}();
            if(!result){if(state->revision==revision){state->checked=previous.checked;state->quantity=old_quantity;state->selector_index=previous.selector_index;
                state->unit_index=previous.unit_index;state->segment_index=previous.segment_index;state->input_text=std::move(previous.input_text1);state->input_text2=std::move(previous.input_text2);(void)apply(state,revision);}
                return std::unexpected(result.error());}
            if(!*result||!state->live||state->revision!=revision)return true;
            state->failure.reset();publish_context(state);if(!state->live||state->revision!=revision)return true;
            if(field=="checked"&&state->callbacks.checked_changed)state->callbacks.checked_changed(state->checked);
            else if(field=="quantity"&&state->callbacks.quantity_changed)state->callbacks.quantity_changed(state->quantity.configuration().value);
            else if(field=="selectorIndex"&&state->callbacks.selector_index_changed)state->callbacks.selector_index_changed(state->selector_index);
            else if(field=="unitIndex"&&state->callbacks.unit_index_changed)state->callbacks.unit_index_changed(state->unit_index);
            else if(field=="segmentIndex"&&state->callbacks.segment_index_changed)state->callbacks.segment_index_changed(state->segment_index);
            else if(field=="inputText1"&&state->callbacks.input_text_changed)state->callbacks.input_text_changed(state->input_text);
            else if(field=="inputText2"&&state->callbacks.input_text2_changed)state->callbacks.input_text2_changed(state->input_text2);
            if(state->live&&state->revision==revision&&state->callbacks.edited)state->callbacks.edited(std::move(field),emitted);
            return true;
        }catch(...){const auto cause=std::current_exception();
            try{state->failure=ii::core::Error{ii::core::ErrorCode::failed,"Contextual list item edit failed",cause};}catch(...){}
            return ii::core::fail(ii::core::ErrorCode::failed,"Contextual list item edit failed",cause);}
    }
    [[nodiscard]] static ii::core::Result<bool> context_step(const std::shared_ptr<State>& state,int direction){
        if(state->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"List item context requires its owner thread");
        if(!state->live)return ii::core::fail(ii::core::ErrorCode::closed,"List item context is closed");
        if(direction==0||!state->input.row.configuration.enabled||!state->input.show_stepper||!state->input.stepper_enabled||
           state->input.stepper_tone==ButtonTone::disabled||(direction>0&&state->input.stepper_arrow==StepperArrow::down)||
           (direction<0&&state->input.stepper_arrow==StepperArrow::up))return false;
        const auto& quantity=state->quantity.configuration();
        const auto target=static_cast<long double>(quantity.value)+static_cast<long double>(direction)*static_cast<long double>(quantity.step);
        const auto low=static_cast<long double>(quantity.minimum);
        const auto high=(std::min)(static_cast<long double>(quantity.maximum),static_cast<long double>((std::numeric_limits<double>::max)()));
        return context_edit(state,"quantity",static_cast<double>(std::clamp(target,low,high)));
    }
    [[nodiscard]] static ii::core::Result<bool> context_action(const std::shared_ptr<State>& state,std::string action,std::shared_ptr<const std::any> custom){
        if(state->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"List item context requires its owner thread");
        if(!state->live)return ii::core::fail(ii::core::ErrorCode::closed,"List item context is closed");
        if(!state->input.row.configuration.enabled)return false;
        if(action.size()>8192||!Policy::valid_accessibility_text(action))return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid list item action name");
        if(action=="more")action="menu";
        try{
            auto slot=action=="primary"?state->input.primary_action:action=="secondary"?state->input.secondary_action:state->input.more_menu;
            if(!slot.enabled)return false;const auto revision=state->revision;const auto values=action_values_of(*state);
            if(slot.command)slot.command->invoke();if(!state->live||state->revision!=revision)return true;
            if(state->callbacks.action_triggered)state->callbacks.action_triggered(action=="menu"?"more":action,slot);
            if(state->live&&state->revision==revision&&state->callbacks.action_values_triggered)state->callbacks.action_values_triggered(action,values);
            if(state->live&&state->revision==revision&&state->callbacks.action_event){
                auto event=values;event.custom_payload=std::move(custom);const ListItemActionPayload payload=std::move(event);
                state->callbacks.action_event(action,payload);}
            return true;
        }catch(...){const auto cause=std::current_exception();
            try{state->failure=ii::core::Error{ii::core::ErrorCode::failed,"Contextual list item action failed",cause};}catch(...){}
            return ii::core::fail(ii::core::ErrorCode::failed,"Contextual list item action failed",cause);}
    }
    [[nodiscard]] static ii::core::Result<void> ensure_context(const std::shared_ptr<State>& state){
        if(state->context)return {};
        try{
            std::weak_ptr<State> weak=state;
            auto commands=Policy::ContextPort::make_commands(
                [weak](std::string field,ListItemValue value)->ii::core::Result<bool>{auto state=weak.lock();
                    if(!state)return ii::core::fail(ii::core::ErrorCode::closed,"List item context is closed");return context_edit(state,std::move(field),std::move(value));},
                [weak](int direction)->ii::core::Result<bool>{auto state=weak.lock();
                    if(!state)return ii::core::fail(ii::core::ErrorCode::closed,"List item context is closed");return context_step(state,direction);},
                [weak](std::string action,std::shared_ptr<const std::any> payload)->ii::core::Result<bool>{auto state=weak.lock();
                    if(!state)return ii::core::fail(ii::core::ErrorCode::closed,"List item context is closed");return context_action(state,std::move(action),std::move(payload));});
            state->context_commands=commands;
            state->context=Policy::ContextPort::make(context_snapshot(*state),std::move(commands));
            return {};
        }catch(...){state->context_commands.reset();return ii::core::fail(ii::core::ErrorCode::failed,"List item context creation failed",std::current_exception());}
    }
public:
    [[nodiscard]] static ii::core::Result<BasicNativeListItemComposite> create(ii::gui::WidgetBuilder& builder,ii::gui::Widget parent,std::string_view id,
            ListItemCompositeInput input={},NativeListItemCompositeCallbacks callbacks={},NativeListItemCompositeViews views={}){
        try{
            if(!valid(input))return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid list item composition");
            if((views.footer&&views.footer_context)||(views.preview&&views.preview_context)||
               (views.leading&&views.leading_context)||(views.trailing&&views.trailing_context))
                return ii::core::fail(ii::core::ErrorCode::invalid_argument,"List item view cannot have two factories");
            auto state=std::make_shared<State>();state->input=std::move(input);state->callbacks=std::move(callbacks);
            state->has_custom_leading=bool(views.leading)||bool(views.leading_context);state->has_custom_trailing=bool(views.trailing)||bool(views.trailing_context);
            state->checked=state->input.checked.value_or(default_checked(state->input));
            state->selector_index=state->input.selector_index.value_or(state->input.selector.selected_index);
            state->unit_index=state->input.unit_index.value_or(state->input.unit_selector.selected_index);
            state->segment_index=state->input.segment_index.value_or(0);
            state->input_text=state->input.input_text.value_or("Value");state->input_text2=state->input.input_text2.value_or("Value");
            const auto initial_quantity=state->input.quantity.value_or(std::clamp(1.,state->input.minimum_quantity,state->input.maximum_quantity));
            (void)state->quantity.set_configuration(quantity_input(state->input,initial_quantity));
            const auto size=intrinsic(state->input);state->extent={size.width,size.height};std::weak_ptr<State> weak=state;
            if(views.footer_context||views.preview_context||views.leading_context||views.trailing_context){
                if(auto context=ensure_context(state);!context)return std::unexpected(context.error());
            }
            const auto mount=[&](ii::gui::Widget host,ii::gui::WidgetFactory plain,ListItemContextViewFactory contextual)->ii::core::Result<void>{
                if(!contextual)return host.set_content(std::move(plain));
                std::shared_ptr<const ListItemContext> context=state->context;
                return host.set_content([context=std::move(context),factory=std::move(contextual)](ii::gui::WidgetBuilder& child){return factory(child,context);});
            };
            const auto name=[&](std::string_view suffix){return std::string(id)+"."+std::string(suffix);};
            auto root=builder.absolute_stack(parent,id,state->extent);if(!root)return std::unexpected(root.error());state->root=*root;
            auto row=NativeListItem::create(builder,*root,name("row"),row_input(*state),{.activated=[weak]{if(auto state=weak.lock())activated(state);}});
            if(!row)return std::unexpected(row.error());state->row=*row;
            if(auto painter=row->set_content_painter([weak](auto& drawing,const auto& view,const auto& row)->ii::core::Result<void>{
                auto state=weak.lock();if(!state)return ii::core::fail(ii::core::ErrorCode::closed,"Detail item retired");
                if(state->input.row.configuration.variant==ListItemVariant::task)return paint_task(*state,drawing,view,row);
                if(state->input.row.configuration.variant==ListItemVariant::form)return paint_form(*state,drawing,view,row);
                if(preview_row(state->input))return paint_preview(*state,drawing,view,row);
                return detail_controls(state->input)?paint_controls(*state,drawing,view,row):paint_detail(*state,drawing,view,row);
            });!painter)return std::unexpected(painter.error());
            auto toggle=CompositeToggleSwitch::create(builder,*root,name("toggle"),toggle_input(*state),[weak](bool value){if(auto state=weak.lock())changed(state,value);});
            if(!toggle)return std::unexpected(toggle.error());state->toggle=*toggle;
            auto check=CompositeCheckBox::create(builder,*root,name("checkbox"),checkbox_input(*state),[weak](bool value){if(auto state=weak.lock())changed(state,value);});
            if(!check)return std::unexpected(check.error());state->checkbox=*check;
            auto actions=NativeListFooter::create(builder,*root,name("actions"),actions_input(*state),
                {.clicked=[weak](std::size_t index,const ListFooterSlot& slot){if(auto state=weak.lock()){
                    if(reverse_actions(state->input)&&index<2)index=1-index;
                    triggered(state,index,slot);}}});
            if(!actions)return std::unexpected(actions.error());state->actions=*actions;
            auto header_more=NativeToolbarButton::create(builder,*root,name("header-more"),header_more_input(*state),{
                .clicked=[weak]{if(auto state=weak.lock();state&&state->live)try{
                    if(state->input.row.configuration.variant!=ListItemVariant::resource||!state->input.row.configuration.visible||
                       !state->input.row.configuration.enabled||!has_action(state->input,2)||!state->input.more_menu.enabled)return;
                    const auto revision=state->revision;auto slot=state->input.more_menu;if(slot.command)slot.command->invoke();
                    if(state->live&&state->revision==revision)triggered(state,2,slot);
                }catch(...){state->failure=ii::core::Error{ii::core::ErrorCode::failed,"Resource menu action failed",std::current_exception()};}}});
            if(!header_more)return std::unexpected(header_more.error());state->header_more=*header_more;
            MenuInput menu_options;menu_options.configuration.show_icon_slot=false;menu_options.configuration.accessibility_name="More actions";
            auto popup=NativeMenu::create(builder,*root,name("popup"),menu_options,{.triggered=[weak](int index){
                auto state=weak.lock();if(state&&state->action_menu&&index>=0)
                    menu_selected(state,state->action_menu,{std::size_t(index)});
            }});if(!popup)return std::unexpected(popup.error());state->popup=*popup;
            auto menu_host=builder.content_host(popup->content_root(),name("menu-items"),{});if(!menu_host)return std::unexpected(menu_host.error());state->menu_host=*menu_host;
            auto section=popup->register_section(*menu_host);if(!section)return std::unexpected(section.error());state->menu_section=*section;
            if(auto finalized=popup->finalize();!finalized)return std::unexpected(finalized.error());
            auto stepper=NativeStepper::create(builder,*root,name("stepper"),stepper_input(*state),
                {.stepped=[weak](int direction){if(auto state=weak.lock())stepped(state,direction);}});
            if(!stepper)return std::unexpected(stepper.error());state->stepper=*stepper;
            auto quantity_label=builder.label(*root,name("quantity"),quantity_label_input(*state));
            if(!quantity_label)return std::unexpected(quantity_label.error());state->quantity_label=*quantity_label;
            auto selector=NativeListItemSelector::create(builder,*root,name("selector"),selector_input(*state),
                [weak](int index){if(auto state=weak.lock())selected(state,index);});
            if(!selector)return std::unexpected(selector.error());state->selector=*selector;
            auto unit_selector=NativeListItemSelector::create(builder,*root,name("unit-selector"),unit_selector_input(*state),
                [weak](int index){if(auto state=weak.lock())unit_selected(state,index);});
            if(!unit_selector)return std::unexpected(unit_selector.error());state->unit_selector=*unit_selector;
            auto segments=NativeListItemSegments::create(builder,*root,name("segments"),segments_input(*state),
                [weak](int index){if(auto state=weak.lock())segment_selected(state,index);});
            if(!segments)return std::unexpected(segments.error());state->segments=*segments;
            auto field_options=input_field_input(*state);auto input_field=NativeInputField::create(builder,*root,name("input1"),field_options,{
                .edited=[weak](std::string text){if(auto state=weak.lock())text_changed(state,std::move(text));},
                .submitted=[weak]{if(auto state=weak.lock())text_submitted(state);}});
            if(!input_field)return std::unexpected(input_field.error());state->input_field=*input_field;state->applied_input=std::move(field_options);
            auto field_options2=input_field_input(*state,true);auto input_field2=NativeInputField::create(builder,*root,name("input2"),field_options2,{
                .edited=[weak](std::string text){if(auto state=weak.lock())text_changed(state,std::move(text),true);},
                .submitted=[weak]{if(auto state=weak.lock())text_submitted(state,true);}});
            if(!input_field2)return std::unexpected(input_field2.error());state->input_field2=*input_field2;state->applied_input2=std::move(field_options2);
            if(views.preview||views.preview_context){
                auto container=builder.absolute_stack(*root,name("preview-container"),{});if(!container)return std::unexpected(container.error());state->preview_container=*container;
                if(auto clipped=container->set_clip_content(true);!clipped)return std::unexpected(clipped.error());
                auto custom=builder.content_host(*container,name("custom-preview"),{});if(!custom)return std::unexpected(custom.error());state->custom_preview=*custom;
                if(auto mounted=mount(*custom,std::move(views.preview),std::move(views.preview_context));!mounted)return std::unexpected(mounted.error());
                if(auto observing=observe_natural(state,*custom,NaturalView::preview);!observing)return std::unexpected(observing.error());
            }
            if(views.leading||views.leading_context){
                auto container=builder.absolute_stack(*root,name("leading-container"),{});if(!container)return std::unexpected(container.error());state->leading_container=*container;
                if(auto clipped=container->set_clip_content(true);!clipped)return std::unexpected(clipped.error());
                auto custom=builder.content_host(*container,name("custom-leading"),{});if(!custom)return std::unexpected(custom.error());state->custom_leading=*custom;
                if(auto mounted=mount(*custom,std::move(views.leading),std::move(views.leading_context));!mounted)return std::unexpected(mounted.error());
                if(auto observing=observe_natural(state,*custom,NaturalView::leading);!observing)return std::unexpected(observing.error());
            }
            if(views.trailing||views.trailing_context){
                auto container=builder.absolute_stack(*root,name("trailing-container"),{});if(!container)return std::unexpected(container.error());state->trailing_container=*container;
                if(auto clipped=container->set_clip_content(true);!clipped)return std::unexpected(clipped.error());
                auto custom=builder.content_host(*container,name("custom-trailing"),{});if(!custom)return std::unexpected(custom.error());state->custom_trailing=*custom;
                if(auto mounted=mount(*custom,std::move(views.trailing),std::move(views.trailing_context));!mounted)return std::unexpected(mounted.error());
                if(auto observing=observe_natural(state,*custom,NaturalView::trailing);!observing)return std::unexpected(observing.error());
            }
            if(views.footer||views.footer_context){
                auto footer=builder.content_host(*root,name("footer"),{});if(!footer)return std::unexpected(footer.error());state->footer=*footer;
                if(auto mounted=mount(*footer,std::move(views.footer),std::move(views.footer_context));!mounted)return std::unexpected(mounted.error());
                auto measured=footer->measure(size.width);if(!measured)return std::unexpected(measured.error());
                if(!std::isfinite(measured->height)||measured->height<0)return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid list item footer measurement");
                state->footer_height=std::clamp(measured->height,0.,8192.);
                const auto extended=intrinsic(*state);state->extent={extended.width,extended.height};
                if(auto observing=footer->set_measure_handler([weak](const ii::gui::WidgetMeasurement& measurement){
                    auto state=weak.lock();if(!state||!state->live||!std::isfinite(measurement.natural.height))return;
                    const auto height=std::clamp(measurement.natural.height,0.,8192.);if(height==state->footer_height)return;
                    const auto previous=state->footer_height;const auto old_extent=state->extent;const auto old_size=intrinsic(*state);state->footer_height=height;
                    const auto size=intrinsic(*state);
                    if(auto resized=state->root.set_extent({size.width,size.height});!resized){state->footer_height=previous;state->failure=resized.error();return;}
                    if(auto framed=frames(state);!framed){state->footer_height=previous;state->extent=old_extent;(void)state->root.set_extent({old_size.width,old_size.height});(void)frames(state);state->failure=framed.error();return;}
                    if(auto painted=state->row->root().request_repaint();!painted)state->failure=painted.error();
                });!observing)return std::unexpected(observing.error());
            }
            if(auto clipped=root->set_clip_content(true);!clipped)return std::unexpected(clipped.error());
            if(auto handler=root->set_layout_handler([weak](ii::gui::Rect bounds){if(auto state=weak.lock()){
                if(state->extent.width==bounds.width&&state->extent.height==bounds.height)return;state->extent={bounds.width,bounds.height};
                if(auto result=frames(state);!result)state->failure=result.error();
            }});!handler)return std::unexpected(handler.error());
            auto applied=apply(state,state->revision);if(!applied)return std::unexpected(applied.error());
            if(auto retained=builder.retain(state);!retained)return std::unexpected(retained.error());
            if(auto published=builder.on_publish([weak]{if(auto state=weak.lock()){
                state->live=true;
                // The row is constructed before custom views are measured; its
                // own publication hook has now made geometry updates possible.
                if(auto updated=state->row->set_options(row_input(*state));!updated)state->failure=updated.error();
            }});!published)return std::unexpected(published.error());
            if(auto retired=builder.on_retire([weak]{if(auto state=weak.lock()){state->live=false;if(state->context)Policy::ContextPort::close(*state->context);}});!retired)return std::unexpected(retired.error());
            return BasicNativeListItemComposite(std::move(state));
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"List item composition failed",std::current_exception());}
    }
    [[nodiscard]] ii::gui::Widget root()const{return state_->root;}
    [[nodiscard]] ii::gui::Widget row()const{return state_->row->root();}
    [[nodiscard]] ii::gui::Widget toggle()const{return state_->toggle->root();}
    [[nodiscard]] ii::gui::Widget checkbox()const{return state_->checkbox->root();}
    [[nodiscard]] ii::gui::Widget stepper()const{return state_->stepper->root();}
    [[nodiscard]] ii::core::Result<NativeListItemSelector> selector()const{
        if(state_->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"List item requires its owner thread");
        if(auto live=state_->root.snapshot();!live)return std::unexpected(live.error());return *state_->selector;
    }
    [[nodiscard]] ii::core::Result<int> selector_index()const{
        if(state_->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"List item requires its owner thread");
        if(auto live=state_->root.snapshot();!live)return std::unexpected(live.error());return state_->selector_index;
    }
    [[nodiscard]] ii::core::Result<NativeListItemSelector> unit_selector()const{
        if(state_->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"List item requires its owner thread");
        if(auto live=state_->root.snapshot();!live)return std::unexpected(live.error());return *state_->unit_selector;
    }
    [[nodiscard]] ii::core::Result<int> unit_index()const{
        if(state_->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"List item requires its owner thread");
        if(auto live=state_->root.snapshot();!live)return std::unexpected(live.error());return state_->unit_index;
    }
    [[nodiscard]] ii::core::Result<NativeListItemSegments> segments()const{
        if(state_->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"List item requires its owner thread");
        if(auto live=state_->root.snapshot();!live)return std::unexpected(live.error());return *state_->segments;
    }
    [[nodiscard]] ii::core::Result<int> segment_index()const{
        if(state_->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"List item requires its owner thread");
        if(auto live=state_->root.snapshot();!live)return std::unexpected(live.error());return state_->segment_index;
    }
    [[nodiscard]] ii::core::Result<NativeInputField> input_field()const{
        if(state_->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"List item requires its owner thread");
        if(auto live=state_->root.snapshot();!live)return std::unexpected(live.error());return *state_->input_field;
    }
    [[nodiscard]] ii::core::Result<NativeInputField> input_field2()const{
        if(state_->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"List item requires its owner thread");
        if(auto live=state_->root.snapshot();!live)return std::unexpected(live.error());return *state_->input_field2;
    }
    [[nodiscard]] ii::core::Result<std::string> input_text()const{
        if(state_->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"List item requires its owner thread");
        if(auto live=state_->root.snapshot();!live)return std::unexpected(live.error());return state_->input_text;
    }
    [[nodiscard]] ii::core::Result<std::string> input_text2()const{
        if(state_->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"List item requires its owner thread");
        if(auto live=state_->root.snapshot();!live)return std::unexpected(live.error());return state_->input_text2;
    }
    [[nodiscard]] ii::core::Result<ListItemActionValues> action_values()const{
        if(state_->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"List item requires its owner thread");
        if(auto live=state_->root.snapshot();!live)return std::unexpected(live.error());
        try{return action_values_of(*state_);}catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Cannot copy list item values",std::current_exception());}
    }
    [[nodiscard]] ii::core::Result<double> quantity()const{
        if(state_->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"List item requires its owner thread");
        if(auto live=state_->root.snapshot();!live)return std::unexpected(live.error());return state_->quantity.configuration().value;
    }
    [[nodiscard]] ii::core::Result<ii::gui::Widget> action(std::size_t index)const{
        if(state_->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"List item requires its owner thread");
        if(auto live=state_->root.snapshot();!live)return std::unexpected(live.error());
        if(state_->input.row.configuration.variant==ListItemVariant::resource&&index==2)return state_->header_more->root();
        if(reverse_actions(state_->input)&&index<2)index=1-index;
        return state_->actions->slot(index);
    }
    [[nodiscard]] ii::core::Result<ii::gui::Widget> action_menu()const{
        if(state_->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"List item requires its owner thread");
        if(auto live=state_->root.snapshot();!live)return std::unexpected(live.error());
        if(!state_->popup)return ii::core::fail(ii::core::ErrorCode::closed,"List item menu is not mounted");
        return state_->popup->root();
    }
    [[nodiscard]] ii::core::Result<ii::gui::Widget> menu_item(std::size_t index)const{
        return menu_item(std::vector<std::size_t>{index});
    }
    [[nodiscard]] ii::core::Result<ii::gui::Widget> menu_item(const std::vector<std::size_t>& path)const{
        if(state_->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"List item requires its owner thread");
        if(auto live=state_->root.snapshot();!live)return std::unexpected(live.error());
        if(!state_->action_menu)return ii::core::fail(ii::core::ErrorCode::out_of_range,"List item menu entry is unavailable");
        const auto found=state_->action_menu->item_widgets.find(path);
        if(found==state_->action_menu->item_widgets.end())return ii::core::fail(ii::core::ErrorCode::out_of_range,"List item menu entry is unavailable");
        return found->second;
    }
    [[nodiscard]] ii::core::Result<ii::gui::Widget> menu_submenu(const std::vector<std::size_t>& path)const{
        if(state_->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"List item requires its owner thread");
        if(auto live=state_->root.snapshot();!live)return std::unexpected(live.error());
        if(!state_->action_menu)return ii::core::fail(ii::core::ErrorCode::out_of_range,"List item submenu is unavailable");
        const auto found=state_->action_menu->submenu_widgets.find(path);
        if(found==state_->action_menu->submenu_widgets.end())return ii::core::fail(ii::core::ErrorCode::out_of_range,"List item submenu is unavailable");
        return found->second;
    }
    [[nodiscard]] ii::core::Result<ListItemCompositeInput> options()const{
        if(state_->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"List item requires its owner thread");
        if(auto live=state_->root.snapshot();!live)return std::unexpected(live.error());return state_->input;
    }
    [[nodiscard]] ii::core::Result<std::shared_ptr<const ListItemContext>> context()const{
        if(state_->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"List item requires its owner thread");
        if(auto live=state_->root.snapshot();!live)return std::unexpected(live.error());
        if(!state_->live)return ii::core::fail(ii::core::ErrorCode::closed,"List item context is closed");
        if(auto ready=ensure_context(state_);!ready)return std::unexpected(ready.error());return std::shared_ptr<const ListItemContext>(state_->context);
    }
    [[nodiscard]] ii::core::Result<bool> checked()const{
        if(state_->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"List item requires its owner thread");
        if(auto live=state_->root.snapshot();!live)return std::unexpected(live.error());return state_->checked;
    }
    [[nodiscard]] ii::core::Result<void> set_options(ListItemCompositeInput value)const{return update(state_,std::move(value));}
    [[nodiscard]] ii::core::Result<std::optional<ii::core::Error>> failure()const{
        if(state_->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"List item requires its owner thread");
        if(auto live=state_->root.snapshot();!live)return std::unexpected(live.error());
        if(state_->failure)return state_->failure;
        if(state_->actions){auto child=state_->actions->failure();if(!child)return std::unexpected(child.error());if(*child)return *child;}
        if(state_->header_more){auto child=state_->header_more->failure();if(!child)return std::unexpected(child.error());if(*child)return *child;}
        if(state_->selector){auto child=state_->selector->failure();if(!child)return std::unexpected(child.error());if(*child)return *child;}
        if(state_->unit_selector){auto child=state_->unit_selector->failure();if(!child)return std::unexpected(child.error());if(*child)return *child;}
        if(state_->segments){auto child=state_->segments->failure();if(!child)return std::unexpected(child.error());if(*child)return *child;}
        return std::optional<ii::core::Error>{};
    }
private:
    std::shared_ptr<State> state_;
};

} // namespace ii::gui
