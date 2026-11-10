#include <iiGUI/WidgetState.h>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <thread>

#if !defined(IIGUI_ACCESSIBILITY_VALUE_HANDLER_API)
#error "The public iiGUI header must expose accessible value handler support"
#endif

namespace {
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
template<class T> T take(ii::core::Result<T> result) {
    if (!result) throw std::runtime_error(result.error().message);
    return std::move(*result);
}
void good(ii::core::Result<void> result) { if (!result) throw std::runtime_error(result.error().message); }
void reentrant_action(){
    namespace gui=ii::gui;bool armed=false;gui::Widget action;
    gui::detail::WidgetFrameHooks hooks;
    hooks.accessibility_changed=[&]{
        if(!armed)return;armed=false;auto nested=take(action.action_options());
        nested.enabled=true;nested.text="Nested";good(action.set_action_options(nested));
    };
    auto tree=std::make_shared<gui::detail::WidgetState>([](ii::runtime::Task task){task();},gui::WidgetAppearance{},std::move(hooks));
    auto host=take(tree->content_host({},"host",{}));
    good(host.set_content([&](gui::WidgetBuilder& builder)->ii::core::Result<void>{
        auto surface=builder.action_surface({},"action",{.text="Initial"});if(!surface)return std::unexpected(surface.error());action=*surface;return {};
    }));
    auto outer=take(action.action_options());outer.enabled=false;outer.text="Outer";armed=true;
    good(action.set_action_options(outer));
    const auto final=take(action.snapshot());const auto options=take(action.action_options());
    require(final.enabled&&final.text=="Nested"&&options.enabled,"Reentrant accessibility update was overwritten by an older action update");
    tree->close();
}
void label_text_notifications(){
    namespace gui=ii::gui;
    unsigned notifications=0;bool reenter=false;gui::Widget label;
    gui::detail::WidgetFrameHooks hooks;
    hooks.accessibility_changed=[&]{
        ++notifications;
        if(reenter){reenter=false;good(label.set_text("Newest"));}
    };
    auto tree=std::make_shared<gui::detail::WidgetState>([](ii::runtime::Task task){task();},gui::WidgetAppearance{},hooks);
    auto host=take(tree->content_host({},"label-host",{200,80}));
    gui::LabelOptions initial{.text="Initial"};
    good(host.set_content([&](gui::WidgetBuilder& builder)->ii::core::Result<void>{
        auto created=builder.label({},"label",initial);if(!created)return std::unexpected(created.error());label=*created;return {};
    }));
    auto before=notifications;good(label.set_label_options(initial));
    require(notifications==before,"Unchanged label options published an accessibility update");
    auto styled=initial;styled.font_size=18;good(label.set_label_options(styled));
    require(notifications==before,"A visual-only label change published an accessibility update");
    styled.text="Older";reenter=true;good(label.set_label_options(styled));
    require(!reenter&&notifications==before+2&&take(label.snapshot()).text=="Newest",
            "Label text accessibility notification lost or overwrote a reentrant update");
    before=notifications;good(label.set_text("Newest"));
    require(notifications==before,"Unchanged label text published an accessibility update");
    good(label.set_text("Changed"));
    require(notifications==before+1&&take(label.snapshot()).text=="Changed",
            "Direct label text mutation did not publish an accessibility update");
    tree->close();
}
void secure_text_provider(){
    namespace gui=ii::gui;
    unsigned notifications=0;gui::detail::WidgetFrameHooks hooks;
    hooks.accessibility_changed=[&]{++notifications;};
    auto tree=std::make_shared<gui::detail::WidgetState>([](ii::runtime::Task task){task();},gui::WidgetAppearance{},hooks);
    auto host=take(tree->content_host({},"host",{200,80}));gui::Widget editor;
    bool secure=true;std::string content="pass";
    auto provider=std::make_shared<gui::TextInputProvider>();
    provider->state=[&]()->ii::core::Result<gui::TextInputState>{
        return gui::TextInputState{.length=content.size(),.selection={content.size(),0},.caret={8,4,1,16},.multiline=false,.password=secure};
    };
    provider->read=[&](gui::TextRange range)->ii::core::Result<gui::TextContent>{
        return gui::TextContent{range,content.substr(range.location,range.length)};
    };
    provider->set_text=[&](std::string_view value)->ii::core::Result<void>{content=value;return {};};
    good(host.set_content([&](gui::WidgetBuilder& builder)->ii::core::Result<void>{
        auto value=builder.absolute_stack({},"secure",{200,30});if(!value)return std::unexpected(value.error());
        editor=*value;return editor.set_text_input_provider(provider);
    }));
    auto snapshot=take(editor.snapshot());
    require(snapshot.text_input&&snapshot.password&&snapshot.text.empty(),"Secure custom input metadata exposed or lost its value");
    require(take(editor.text_content({0,4})).text=="pass","Text input services lost their bounded content channel");
    good(editor.set_text("reset"));require(take(editor.text_content({0,5})).text=="reset","Custom text mutation did not reach its provider");
    require(!editor.set_text(std::string("bad\0text",8))&&content=="reset","Invalid text reached the provider");
    provider->set_text=[](std::string_view)->ii::core::Result<void>{throw std::runtime_error("mutation failed");};
    auto failed=editor.set_text("new");require(!failed&&bool(failed.error().cause),"Text mutation exception lost its cause");
    secure=false;const auto previous=notifications;good(editor.notify_text_input_changed(false));
    require(notifications>previous&&!take(editor.snapshot()).password,"Custom input did not publish its updated metadata");
    good(editor.set_text_input_provider(nullptr));
    require(!editor.notify_text_input_changed(),"A widget without a text provider published text changes");
    tree->close();
}
void accessibility_value_delivery(){
    namespace gui=ii::gui;unsigned changed=0,calls=0;double delivered=0;
    gui::detail::WidgetFrameHooks hooks;hooks.accessibility_value_changed=[&]{++changed;};
    auto tree=std::make_shared<gui::detail::WidgetState>([](ii::runtime::Task task){task();},gui::WidgetAppearance{},hooks);
    auto host=take(tree->content_host({},"value-host",{200,80}));gui::Widget slider,custom,progress;
    good(host.set_content([&](gui::WidgetBuilder& builder)->ii::core::Result<void>{
        auto root=builder.vstack({},"value-root");if(!root)return std::unexpected(root.error());
        auto native=builder.slider(*root,"native",{.minimum=0,.maximum=10,.value=2,.step=2},[&](double value){++calls;delivered=value;});
        if(!native)return std::unexpected(native.error());slider=*native;
        auto surface=builder.action_surface(*root,"custom",{});if(!surface)return std::unexpected(surface.error());custom=*surface;
        if(auto r=custom.set_accessibility_semantics(gui::AccessibilitySemantics{.role=gui::AccessibilityRole::slider,.range=gui::AccessibilityRange{0,10,2}});!r)return r;
        auto bar=builder.progress(*root,"progress",{});if(!bar)return std::unexpected(bar.error());progress=*bar;
        return {};
    }));
    const auto native_id=take(slider.snapshot()).id,custom_id=take(custom.snapshot()).id,progress_id=take(progress.snapshot()).id;
    require(take(tree->can_set_accessibility_value(native_id,7)),"Native slider rejected an accessible value");
    const auto previous=changed;good(tree->set_accessibility_value(native_id,7));
    require(calls==1&&delivered==8&&take(slider.snapshot()).value==8&&changed>previous,"Native slider lost step, callback or value notification");
    require(!take(tree->can_set_accessibility_value(custom_id,4)),"Custom slider without a setter accepted a value");
    good(custom.set_accessibility_value_handler([&](double value)->ii::core::Result<void>{
        ++calls;delivered=value;auto state=custom.snapshot();if(!state)return std::unexpected(state.error());
        auto semantics=*state->accessibility_semantics;semantics.range->value=value;return custom.set_accessibility_semantics(semantics);
    }));
    const auto before_custom=changed;good(tree->set_accessibility_value(custom_id,4));
    require(calls==2&&delivered==4&&changed>before_custom&&take(custom.snapshot()).value==4,"Custom slider setter lost the requested value or notification");
    require(!take(tree->can_set_accessibility_value(progress_id,.5)),"Progress unexpectedly accepted a value");
    require(!take(tree->can_set_accessibility_value(custom_id,11))&&!take(tree->can_set_accessibility_value(custom_id,std::numeric_limits<double>::quiet_NaN())),"Out-of-range or nonfinite value was accepted");
    good(custom.set_enabled(false));require(!take(tree->can_set_accessibility_value(custom_id,4)),"Disabled slider accepted a value");
    require(!tree->set_accessibility_value(custom_id,4)&&calls==2,"Disabled slider delivered a value");
    good(custom.set_enabled(true));good(custom.set_accessibility_value_handler([](double)->ii::core::Result<void>{throw std::runtime_error("value handler failed");}));
    auto thrown=tree->set_accessibility_value(custom_id,4);require(!thrown&&bool(thrown.error().cause),"Value handler exception lost its cause");
    good(custom.set_accessibility_value_handler([&](double)->ii::core::Result<void>{return host.clear_content();}));
    good(tree->set_accessibility_value(custom_id,5));require(!tree->can_set_accessibility_value(native_id,4),"Retired slider remained addressable");tree->close();
}
}

int main() {
    namespace gui = ii::gui;
    auto tree = std::make_shared<gui::detail::WidgetState>([](ii::runtime::Task task) { task(); });
    auto host = take(tree->content_host({}, "host", {400, 300}));
    gui::Widget custom, ordinary, segmented, selectable, adjustable, custom_stepper, root_widget;
    good(host.set_content([&](gui::WidgetBuilder& widgets) -> ii::core::Result<void> {
        auto root = take(widgets.vstack({}, "root"));root_widget=root;
        custom = take(widgets.absolute_stack(root, "custom", {100, 30}));
        ordinary = take(widgets.button(root, "button", {.text="Button"}));
        segmented = take(widgets.segmented_control(root, "segmented"));
        selectable = take(widgets.action_surface(root, "choice", {.text="Choice", .role=gui::ActionRole::check_box, .checked=true}));
        adjustable = take(widgets.action_surface(root, "adjustable", {.accessibility_name="Volume"}));
        custom_stepper = take(widgets.action_surface(root, "custom-stepper", {.accessibility_name="Direction"}));
        return {};
    }));
    auto before = take(custom.snapshot());
    require(!before.accessibility_semantics, "Custom semantics were installed implicitly");
    gui::AccessibilitySemantics progress{.role=gui::AccessibilityRole::progress,
        .name="Encoding", .description="Current encoding progress",
        .range=gui::AccessibilityRange{0, 1, .25}};
    good(custom.set_accessibility_semantics(progress));
    auto current = take(custom.snapshot());
    require(current.id == before.id && current.accessibility_semantics == progress &&
            current.accessibility_name == "Encoding" && current.accessibility_description == "Current encoding progress" &&
            current.minimum == 0 && current.maximum == 1 && current.value == .25,
            "Semantics were not exposed through the retained widget snapshot");
    progress.range->value = .75;
    good(custom.set_accessibility_semantics(progress));
    require(take(custom.snapshot()).value == .75, "Semantic value update did not retain the widget");
    const auto valid = take(custom.snapshot()).accessibility_semantics;
    auto invalid = progress;
    invalid.range->value = std::numeric_limits<double>::quiet_NaN();
    require(!custom.set_accessibility_semantics(invalid) && take(custom.snapshot()).accessibility_semantics == valid,
            "Invalid semantic value partially committed");
    invalid = progress; invalid.range->minimum = 1;
    require(!custom.set_accessibility_semantics(invalid), "An invalid semantic range was accepted");
    invalid = progress; invalid.range->value = 1.01;
    require(!custom.set_accessibility_semantics(invalid), "An out-of-range semantic value was accepted");
    invalid = progress; invalid.name = std::string("bad\0name", 8);
    require(!custom.set_accessibility_semantics(invalid), "A NUL semantic name was accepted");
    invalid = progress; invalid.name = std::string("\xc0\xaf", 2);
    require(!custom.set_accessibility_semantics(invalid), "Invalid UTF-8 semantic text was accepted");
    invalid = progress; invalid.description = std::string(65537, 'x');
    require(!custom.set_accessibility_semantics(invalid), "An oversized semantic description was accepted");
    invalid = progress; invalid.role = static_cast<gui::AccessibilityRole>(99);
    require(!custom.set_accessibility_semantics(invalid), "An unknown semantic role was accepted");
    require(!ordinary.set_accessibility_semantics(progress), "A built-in control accepted custom semantics");
    require(!segmented.set_accessibility_semantics(progress), "A built-in segmented control accepted custom semantics");
    gui::AccessibilitySemantics tab_group{.role=gui::AccessibilityRole::tab_group,.name="Sections"};
    good(root_widget.set_accessibility_semantics(tab_group));
    require(take(root_widget.snapshot()).accessibility_semantics==tab_group,"Custom tab group semantics were not exposed");
    auto invalid_group=tab_group;invalid_group.range=gui::AccessibilityRange{0,1,.5};
    require(!root_widget.set_accessibility_semantics(invalid_group)&&take(root_widget.snapshot()).accessibility_semantics==tab_group,
            "Invalid tab group range changed its semantics");
    gui::AccessibilitySemantics menu_group{.role=gui::AccessibilityRole::menu,.name="Actions"};
    good(root_widget.set_accessibility_semantics(menu_group));
    require(take(root_widget.snapshot()).accessibility_semantics==menu_group,"Custom menu semantics were not exposed");
    for(auto role:{gui::ActionRole::check_box,gui::ActionRole::radio_button,gui::ActionRole::toggle_switch,gui::ActionRole::tab,gui::ActionRole::menu_item}){
        auto options=take(selectable.action_options());options.role=role;
        good(selectable.set_action_options(options));
        require(take(selectable.snapshot()).action_role==role&&take(selectable.snapshot()).checked,
                "Action choice role or checked state was not exposed");
    }
    good(selectable.set_checked(false));
    require(!take(selectable.snapshot()).checked&&!take(selectable.action_options()).checked,
            "Custom action checked update was not retained");
    auto changed_action=take(selectable.action_options());changed_action.checked=true;
    good(selectable.set_action_options(changed_action));
    require(take(selectable.snapshot()).checked&&take(selectable.snapshot()).selected,"Whole-action update lost the menu selection value");
    good(selectable.activate());require(take(selectable.snapshot()).selected,"Activation changed externally owned selection");
    auto invalid_action=take(selectable.action_options());invalid_action.role=static_cast<gui::ActionRole>(99);
    require(!selectable.set_action_options(invalid_action)&&take(selectable.snapshot()).action_role==gui::ActionRole::menu_item,
            "Invalid action role partially committed");
    int adjustment=0;
    good(adjustable.set_input_handler([&](const gui::WidgetInput& event)->ii::core::Result<bool>{
        if(event.kind==gui::WidgetInputKind::increment){++adjustment;return true;}
        if(event.kind==gui::WidgetInputKind::decrement){--adjustment;return true;}
        return false;
    }));
    gui::AccessibilitySemantics slider{.role=gui::AccessibilityRole::slider,.name="Volume",
        .range=gui::AccessibilityRange{-10,10,2}};
    good(adjustable.set_accessibility_semantics(slider));
    const auto adjustable_snapshot=take(adjustable.snapshot());
    require(adjustable_snapshot.accessibility_semantics==slider&&adjustable_snapshot.minimum==-10&&
        adjustable_snapshot.maximum==10&&adjustable_snapshot.value==2,"Custom slider semantics were not exposed");
    good(tree->input({.kind=gui::WidgetInputKind::increment,.target=adjustable_snapshot.id}));
    require(adjustment==1,"Custom slider increment did not reach its input handler");
    good(tree->input({.kind=gui::WidgetInputKind::decrement,.target=adjustable_snapshot.id}));
    require(adjustment==0,"Custom slider decrement did not reach its input handler");
    auto missing_range=slider;missing_range.range.reset();
    require(!adjustable.set_accessibility_semantics(missing_range)&&take(adjustable.snapshot()).accessibility_semantics==slider,
        "Custom slider accepted a missing range");
    good(adjustable.set_enabled(false));
    good(tree->input({.kind=gui::WidgetInputKind::increment,.target=adjustable_snapshot.id}));
    require(adjustment==0,"Disabled custom slider received an accessibility action");
    good(adjustable.set_enabled(true));
    good(adjustable.set_input_handler([](const gui::WidgetInput&)->ii::core::Result<bool>{
        return ii::core::fail(ii::core::ErrorCode::failed,"Adjustment rejected");
    }));
    auto failed_adjustment=tree->input({.kind=gui::WidgetInputKind::increment,.target=adjustable_snapshot.id});
    require(!failed_adjustment&&failed_adjustment.error().code==ii::core::ErrorCode::failed,
        "Custom slider adjustment error was not propagated");
    good(adjustable.set_input_handler([](const gui::WidgetInput&)->ii::core::Result<bool>{throw std::runtime_error("Adjustment threw");}));
    failed_adjustment=tree->input({.kind=gui::WidgetInputKind::decrement,.target=adjustable_snapshot.id});
    require(!failed_adjustment&&bool(failed_adjustment.error().cause),"Custom slider adjustment exception lost its cause");
    int step_count=0;
    good(custom_stepper.set_input_handler([&](const gui::WidgetInput& event)->ii::core::Result<bool>{
        if(event.kind==gui::WidgetInputKind::increment){++step_count;return true;}
        if(event.kind==gui::WidgetInputKind::decrement){--step_count;return true;}return false;
    }));
    gui::AccessibilitySemantics stepper{.role=gui::AccessibilityRole::stepper,.name="Direction",.can_increment=false};
    good(custom_stepper.set_accessibility_semantics(stepper));const auto step_id=take(custom_stepper.snapshot()).id;
    good(tree->input({.kind=gui::WidgetInputKind::increment,.target=step_id}));
    good(custom_stepper.step(1));good(custom_stepper.step(-1));
    require(step_count==-1,"Custom Stepper did not constrain or route adjustment directions");
    good(root_widget.set_enabled(false));good(tree->input({.kind=gui::WidgetInputKind::decrement,.target=step_id}));
    require(step_count==-1,"A disabled ancestor allowed custom adjustment");good(root_widget.set_enabled(true));
    auto invalid_stepper=stepper;invalid_stepper.can_decrement=false;
    require(!custom_stepper.set_accessibility_semantics(invalid_stepper)&&take(custom_stepper.snapshot()).accessibility_semantics==stepper,
        "A Stepper with no adjustment direction changed valid semantics");
    invalid_stepper=stepper;invalid_stepper.range=gui::AccessibilityRange{0,1,.5};
    require(!custom_stepper.set_accessibility_semantics(invalid_stepper),"A valueless Stepper accepted a numeric range");
    auto indeterminate = progress;
    indeterminate.range.reset();
    good(custom.set_accessibility_semantics(indeterminate));
    require(take(custom.snapshot()).accessibility_semantics == indeterminate,
            "Indeterminate progress did not retain its role");
    gui::AccessibilitySemantics image{.role=gui::AccessibilityRole::image, .name="Thumbnail"};
    good(custom.set_accessibility_semantics(image));
    require(take(custom.snapshot()).accessibility_semantics == image &&
            take(custom.snapshot()).accessibility_name == "Thumbnail", "Image semantics were not applied");
    image.range = gui::AccessibilityRange{0, 1, .5};
    require(!custom.set_accessibility_semantics(image), "Image semantics accepted a numeric range");
    good(custom.set_accessibility_semantics(std::nullopt));
    require(!take(custom.snapshot()).accessibility_semantics, "Clearing semantics did not restore the host default");
    bool rejected = false;
    std::thread worker([&] { rejected = !custom.set_accessibility_semantics(progress); });
    worker.join();
    require(rejected, "A foreign thread modified native semantics");
    good(host.clear_content());
    require(!custom.set_accessibility_semantics(progress), "Retired semantics remained writable");
    tree->close();
    reentrant_action();
    label_text_notifications();
    secure_text_provider();
    accessibility_value_delivery();
}
