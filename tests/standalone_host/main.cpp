#include <iiGUI/Graphics.h>
#include <iiGUI/CompiledViewSource.h>
#include <iiGUI/SrgbColor.h>
#include <iiGUI/InputClearDrawing.h>
#include <iiGUI/NativeTextLayout.h>
#include <iiGUI/TextFieldMaterial.h>
#include <iiGUI/PanelPainter.h>
#include <iiGUI/NativeStepperView.h>
#include <iiGUI/DropdownButtonModel.h>
#include <iiGUI/NativeDropdownButtonView.h>
#include <iiGUI/CheckBoxModel.h>
#include <iiGUI/RadioButtonModel.h>
#include <iiGUI/RadioGroup.h>
#include <iiGUI/NativeRadioButtonView.h>
#include <iiGUI/TableViewGroup.h>
#include <iiGUI/TableHeaderPresentation.h>
#include <iiGUI/AlertPresentation.h>
#include <iiGUI/ModalPresentation.h>
#include <iiGUI/MenuItemModel.h>
#include <iiGUI/MenuModel.h>
#include <iiGUI/NativeMenuView.h>
#include <iiGUI/MaterialGradient.h>
#include <iiGUI/MaterialModel.h>
#include <iiGUI/NativeMaterialView.h>
#include <iiGUI/PopoverModel.h>
#include <iiGUI/NativePopoverView.h>
#include <iiGUI/NativePageRouterView.h>
#include <iiGUI/ColorPickerButtonModel.h>
#include <iiGUI/NativeColorPickerButtonView.h>
#include <iiGUI/TabModel.h>
#include <iiGUI/SliderModel.h>
#include <iiGUI/NativeSliderView.h>
#include <iiGUI/NativeSearchFieldView.h>
#include <iiGUI/NativeTableCellItemView.h>
#include <iiGUI/TableCellItemModel.h>
#include <iiGUI/TableRowViewModel.h>
#include <iiGUI/NativeTableRowView.h>
#include <iiGUI/NativeColorPickerValuesView.h>
#include <iiGUI/NativeColorChannelView.h>
#include <iiGUI/ColorPickerLayout.h>
#include <iiGUI/NativeColorPickerView.h>
#include <iiGUI/MobileNavigationModel.h>
#include <iiGUI/MobileNavigationMotion.h>
#include <iiGUI/NativeMobileNavigationView.h>
#include <iiGUI/NativeHierarchyItemView.h>
#include <iiGUI/HierarchyItemModel.h>
#include <iiGUI/TableHeaderModel.h>
#include <iiGUI/NativeTableHeaderView.h>
#include <iiGUI/NativeTableView.h>
#include <iiGUI/TableViewport.h>
#include <iiGUI/HierarchyToolbarModel.h>
#include <iiGUI/NativeHierarchyToolbarView.h>
#include <iiGUI/NativeHierarchyListView.h>
#include <iiGUI/HierarchyViewport.h>
#include <iiGUI/HierarchyDrag.h>
#include <iiGUI/PanelLayout.h>
#include <iiGUI/NativeHierarchyPanelView.h>
#include <iiGUI/NativeListPanelView.h>
#include <iiGUI/NativeListView.h>
#include <iiGUI/ListViewport.h>
#include <iiGUI/ListDrag.h>
#include <iiGUI/NativeListItemCompositeView.h>
#include <iiGUI/ListItemDetailActionsLayout.h>
#include <iiGUI/ListItemDetailControlsLayout.h>
#include <iiGUI/ListItemTaskLayout.h>
#include <iiGUI/ListItemPreviewRowLayout.h>
#include <iiGUI/ListItemFormLayout.h>
#include <iiGUI/ListItemQuantityLayout.h>
#include <iiGUI/ListItemMenuModel.h>
#include <iiGUI/CollectionNavigation.h>
#include <iiGUI/VirtualViewport.h>
#include <iiGUI/VirtualLineIndex.h>
#include <iiGUI/VirtualCollectionPlanner.h>
#include <iiGUI/VirtualMeasurements.h>
#include <iiGUI/CodeViewport.h>
#include <iiGUI/FocusRing.h>
#include <iiGUI/NativeFocusRingView.h>
#include <iiGUI/MobileTabPainter.h>
#include <iiGUI/NativeMobileTabView.h>
#include <iiGUI/WindowNoiseTexture.h>
#include <iiGUI/NativeWindowNoiseTextureView.h>
#include <iiGUI/NativeWheelScrollGuard.h>
#include <iiGUI/NativeCodeEditorView.h>
#include <iiGUI/TooltipModel.h>
#include <iiGUI/TooltipController.h>
#include <iiGUI/TooltipOutline.h>
#include <iiGUI/NativeTooltipPresentationView.h>
#include <iiGUI/NativeTooltipTargetView.h>
#include <iiGUI/TooltipScheduler.h>
#include <iiGUI/NativeTooltipView.h>
#include <iiGUI/NativeTextEditorView.h>
#include <iiGUI/NativeInputFieldView.h>
#include <iiGUI/TextInputModel.h>
#include <iiGUI/ColorChannelSurface.h>
#include <iiGUI/ListItemSelectorModel.h>
#include <iiGUI/NativeListItemSelectorView.h>
#include <iiGUI/ListItemSegmentsModel.h>
#include <iiGUI/NativeListItemSegmentsView.h>
#include <iiGUI/CardModel.h>
#include <iiGUI/NativeCardView.h>
#include <iiGUI/NativeTabBarView.h>
#include <iiGUI/NativeTabView.h>
#include <iiGUI/ColorSurfaceModel.h>
#include <iiGUI/NativeColorSurfaceView.h>
#include <iiGUI/CollectionWidgetNavigation.h>
#include <iiGUI/DocumentLayout.h>
#include <iiGUI/NativeEditorTimelineView.h>
#include <iiGUI/NativeEditorCanvasView.h>
#include <iiGUI/LinkModel.h>
#include <iiGUI/NativeLinkView.h>
#include <iiGUI/ComboBoxModel.h>
#include <iiGUI/NativeComboBoxView.h>
#include <iiGUI/NativeMenuItemView.h>
#include <iiGUI/NativeSegmentedControlView.h>
#include <iiGUI/NativeCheckBoxView.h>
#include <iiGUI/ToggleSwitchModel.h>
#include <iiGUI/NativeToggleSwitchView.h>
#include <iiGUI/ProgressDrawing.h>
#include <iiGUI/ProgressIndicator.h>
#include <iiGUI/NativeDivider.h>
#include <iiGUI/NativeImageView.h>
#include <iiGUI/AvatarModel.h>
#include <iiGUI/NativeAvatarView.h>
#include <iiGUI/SheetModel.h>
#include <iiGUI/NativeSheetView.h>
#include <iiGUI/ButtonModel.h>
#include <iiGUI/NativeButtonView.h>
#include <iiGUI/ModalModel.h>
#include <iiGUI/NativeModalView.h>
#include <iiGUI/ListItemModel.h>
#include <iiGUI/NativeListItemView.h>
#include <iiGUI/AppCardModel.h>
#include <iiGUI/NativeAppCardView.h>
#include <iiGUI/AppHeaderModel.h>
#include <iiGUI/NativeAppHeaderView.h>
#include <iiGUI/EventListenerModel.h>
#include <iiGUI/NativeEventListenerView.h>
#include <iiGUI/AlertButtonModel.h>
#include <iiGUI/AlertModel.h>
#include <iiGUI/NativeAlertView.h>
#include <iiGUI/NativeAlertButtonView.h>
#include <iiGUI/ToolbarButtonModel.h>
#include <iiGUI/NativeToolbarButtonView.h>
#include <iiGUI/ListFooterModel.h>
#include <iiGUI/NativeListFooterView.h>
#include <iiGUI/ListToolbarModel.h>
#include <iiGUI/HierarchyToolbarLayout.h>
#include <iiGUI/NativeListToolbarView.h>
#include <iiGUI/NativeIconView.h>
#include <iiGUI/SvgImageCache.h>
#include <iiGUI/Text.h>
#include <stdexcept>

ii::core::Result<ii::gui::AlertView> build_standalone_alert(
        ii::gui::WidgetBuilder& builder,ii::gui::Widget parent){
    return ii::gui::AlertView::create(builder,parent,"alert",ii::gui::AlertInput{});
}

ii::core::Result<ii::gui::NativeStepper> build_standalone_stepper(
        ii::gui::WidgetBuilder& builder,ii::gui::Widget parent){
    return ii::gui::NativeStepper::create(builder,parent,"stepper",ii::gui::StepperConfiguration{});
}

ii::core::Result<ii::gui::NativeDropdownButton> build_standalone_dropdown(
        ii::gui::WidgetBuilder& builder,ii::gui::Widget parent){
    return ii::gui::NativeDropdownButton::create(builder,parent,"dropdown",ii::gui::DropdownInput{});
}

ii::core::Result<ii::gui::NativeRadioButton> build_standalone_radio(
        ii::gui::WidgetBuilder& builder,ii::gui::Widget parent){
    return ii::gui::NativeRadioButton::create(builder,parent,"radio",std::make_shared<ii::gui::RadioGroups>(),ii::gui::RadioButtonConfiguration{});
}

ii::core::Result<ii::gui::NativeCheckBox> build_standalone_checkbox(
        ii::gui::WidgetBuilder& builder,ii::gui::Widget parent){
    return ii::gui::NativeCheckBox::create(builder,parent,"checkbox",ii::gui::CheckBoxConfiguration{});
}

ii::core::Result<ii::gui::NativeToggleSwitch> build_standalone_toggle(
        ii::gui::WidgetBuilder& builder,ii::gui::Widget parent){
    return ii::gui::NativeToggleSwitch::create(builder,parent,"toggle",ii::gui::ToggleSwitchConfiguration{});
}

ii::core::Result<ii::gui::NativeButton> build_standalone_button(
        ii::gui::WidgetBuilder& builder, ii::gui::Widget parent) {
    return ii::gui::NativeButton::create(builder, parent, "button", ii::gui::ButtonInput{});
}

ii::core::Result<ii::gui::NativeModal> build_standalone_modal(
        ii::gui::WidgetBuilder& builder, ii::gui::Widget parent) {
    return ii::gui::NativeModal::create(builder, parent, "modal", ii::gui::ModalInput{});
}

ii::core::Result<ii::gui::NativeListItem> build_standalone_list_item(
        ii::gui::WidgetBuilder& builder, ii::gui::Widget parent) {
    return ii::gui::NativeListItem::create(builder, parent, "row", ii::gui::ListItemInput{});
}

ii::core::Result<ii::gui::NativeAppCard> build_standalone_app_card(
        ii::gui::WidgetBuilder& builder, ii::gui::Widget parent) {
    return ii::gui::NativeAppCard::create(builder, parent, "card", ii::gui::AppCardOptions{});
}

ii::core::Result<ii::gui::NativeAppHeader> build_standalone_app_header(
        ii::gui::WidgetBuilder& builder, ii::gui::Widget parent) {
    return ii::gui::NativeAppHeader::create(builder, parent, "header", ii::gui::AppHeaderOptions{});
}

ii::core::Result<ii::gui::NativeEventListener> build_standalone_event_listener(
        ii::gui::WidgetBuilder& builder, ii::gui::Widget parent) {
    return ii::gui::NativeEventListener::create(builder, parent, "listener", ii::gui::EventListenerOptions{});
}

ii::core::Result<ii::gui::AlertButtonView> build_standalone_alert_button(
        ii::gui::WidgetBuilder& builder, ii::gui::Widget parent) {
    return ii::gui::AlertButtonView::create(builder, parent, "alert", ii::gui::AlertButtonOptions{});
}

ii::core::Result<ii::gui::ToolbarButtonView> build_standalone_toolbar_button(
        ii::gui::WidgetBuilder& builder, ii::gui::Widget parent) {
    return ii::gui::ToolbarButtonView::create(builder, parent, "toolbar", ii::gui::ToolbarButtonInput{});
}

ii::core::Result<ii::gui::ListFooterView> build_standalone_list_footer(
        ii::gui::WidgetBuilder& builder, ii::gui::Widget parent) {
    return ii::gui::ListFooterView::create(builder, parent, "footer", ii::gui::FooterInput{});
}

ii::core::Result<ii::gui::ListToolbarView> build_standalone_list_toolbar(
        ii::gui::WidgetBuilder& builder, ii::gui::Widget parent) {
    return ii::gui::ListToolbarView::create(builder, parent, "list-toolbar", ii::gui::ListBarInput{});
}

ii::core::Result<ii::gui::IconView> build_standalone_icon(
        ii::gui::WidgetBuilder& builder, ii::gui::Widget parent, const ii::gui::IconViewInput& input) {
    return ii::gui::IconView::create(builder, parent, "icon", input);
}

int main() {
    auto source=ii::gui::compiled_view_source([](ii::gui::WidgetBuilder&)->ii::core::Result<void>{return {};});
    if (!source) throw std::runtime_error("Independent native source was not usable");
    auto groups=std::make_shared<ii::gui::RadioGroups>();
    if (!groups->navigation_members("test").empty()) throw std::runtime_error("Independent radio group was not empty");
    ii::gui::RadioButtonModel radio;
    ii::gui::SegmentedControlModel segments;
    ii::gui::PopoverModel popover;
    if(!popover.set_configuration({})||!popover.natural_size({10,10}))throw std::runtime_error("Independent popover model was not usable");
    ii::gui::MaterialModel material;
    if(!material.set_configuration({},{}))throw std::runtime_error("Independent material model was not usable");
    ii::gui::MenuModel menu;
    if(!menu.set_entries({})||!menu.open())throw std::runtime_error("Independent menu model was not usable");
    ii::gui::MenuItemModel menu_item;
    if(!menu_item.set_configuration({})||menu_item.width()!=161)throw std::runtime_error("Independent menu item model was not usable");
    if(!segments.geometry(100,30))throw std::runtime_error("Independent segmented control model was not usable");
    if(ii::gui::AlertPresentation::title("Title",ii::gui::WidgetColor{}).text!="Title")
        throw std::runtime_error("Independent alert presentation was not usable");
    if(ii::gui::ModalPresentation::title("Title",ii::gui::WidgetColor{}).text!="Title")
        throw std::runtime_error("Independent modal presentation was not usable");
    if (!radio.set_configuration({}, std::chrono::steady_clock::now()))
        throw std::runtime_error("Independent radio model was not usable");
    ii::gui::CheckBoxModel checkbox;
    if (!checkbox.set_configuration({}, std::chrono::steady_clock::now()))
        throw std::runtime_error("Independent checkbox model was not usable");
    ii::gui::ToggleSwitchModel toggle;
    if (!toggle.set_configuration({}, std::chrono::steady_clock::now()))
        throw std::runtime_error("Independent toggle model was not usable");
    ii::gui::DropdownButtonModel dropdown;
    if (!dropdown.preferred_width(12)) throw std::runtime_error("Independent dropdown model was not usable");
    const auto linear=ii::gui::srgb_to_linear(ii::gui::WidgetColor{.5,.5,.5,1});
    if (linear.red < .214f || linear.red > .215f) throw std::runtime_error("Independent color conversion failed");
    ii::gui::WidgetPaintState paint;paint.size={16,16};
    if (!ii::gui::input_field_clear_drawing(paint,ii::gui::WidgetColor{},ii::gui::WidgetColor{},13,13,"",false))
        throw std::runtime_error("Independent clear drawing failed");
    auto paragraph=ii::gui::NativeTextLayout::create("Text");
    if(!paragraph||paragraph->length()!=4)throw std::runtime_error("Independent text layout failed");
    ii::gui::PanelPainter panel;
    if (!panel.set_colors({})) throw std::runtime_error("Independent panel painter was not usable");
    ii::gui::SvgImageCache svg_cache;
    if(svg_cache.stats().entries!=0)throw std::runtime_error("New SVG cache was not empty");
    ii::gui::CanvasFrame frame{.viewport = {0, 0, 100, 100}};
    if (!ii::gui::valid_canvas(frame)) throw std::runtime_error("Standalone host rejected its canvas");
    auto text = ii::gui::measure_wrapped_text_layout({.bounds = {0, 0, 40, 100}, .text = "Native host"});
    if (!text || text->height <= 0) throw std::runtime_error("Standalone host did not measure text");
    auto progress=ii::gui::progress_drawing({.width=80,.height=6,.fill_width=40,.track_radius=3,.fill_radius=3});
    if(!progress||(*progress)->panels.size()!=2)throw std::runtime_error("Standalone host did not prepare its progress drawing");
    ii::gui::ProgressModel model;
    if(!model.set_current(50)||!model.geometry(100,6))throw std::runtime_error("Standalone progress model failed");
    ii::gui::Divider divider;
    if(!divider.geometry(100,3))throw std::runtime_error("Standalone divider failed");
    ii::gui::Image image;
    if(!image.set_configuration({},ii::gui::ImageSize{4,2}) || !image.geometry(40,40))
        throw std::runtime_error("Standalone image model failed");
    if(!ii::gui::HierarchyToolbarLayout::geometry({}, {}, {}))
        throw std::runtime_error("Standalone hierarchy toolbar layout failed");
    ii::gui::AlertModel alert;
    if(!alert.layout(800,600,0,0))throw std::runtime_error("Standalone alert model failed");
    ii::gui::Avatar avatar;
    if(avatar.image_configuration().width!=64)throw std::runtime_error("Standalone avatar model failed");
    auto avatar_source=ii::gui::make_avatar_placeholder_source([](std::uint32_t)
            ->ii::core::Result<std::shared_ptr<const ii::gui::RasterImage>> {
        return ii::core::fail(ii::core::ErrorCode::failed,"Unexpected decode");
    });
    if(!avatar_source||!(*avatar_source)({0,0},1)||*(*avatar_source)({0,0},1))
        throw std::runtime_error("Standalone avatar source failed");
}
