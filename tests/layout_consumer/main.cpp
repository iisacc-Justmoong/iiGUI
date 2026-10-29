#include <iiGUI/NativeWidgets.h>
#include <LVUI/NativeWidgetLayout.h>
#include "LayoutPackage.generated.h"
#include "WindowPackage.generated.h"
#include "ReadonlyPackage.generated.h"
#include "ObjectInputPackage.generated.h"
#include "MethodPackage.generated.h"
#include "MethodReferencePackage.generated.h"
#include <iostream>
auto installed_layout_factory=&lvui_generated::LayoutPackage::build;
auto installed_window_factory=&lvui_generated::WindowPackage::build;
auto installed_readonly_factory=&lvui_generated::ReadonlyPackage::build;
auto installed_object_input_factory=&lvui_generated::ObjectInputPackage::build;
auto installed_method_factory=&lvui_generated::MethodPackage::build;
auto installed_method_reference_factory=&lvui_generated::MethodReferencePackage::build;
int main() {
    auto value=lvui::native::stack_layout({.spacing=-1,.default_spacing=6,.padding=4,.left_padding=2,.width=200,.height=80});
    if (!ii::gui::valid_stack_layout(value) || value.stack.spacing!=6 || value.stack.insets!=ii::gui::Insets{2,4,4,4}) return 1;
    ii::gui::Widget retired;
    auto write=retired.set_stack_layout(value);auto read=retired.stack_layout();
    if (write || write.error().code!=ii::core::ErrorCode::closed || read || read.error().code!=ii::core::ErrorCode::closed || !installed_layout_factory) return 2;
    auto geometry=ii::gui::resolve_window_geometry({"Before",{700,500},{320,220}},{.title="After"});
    if(!geometry||geometry->title!="After"||geometry->size!=ii::gui::Size{700,500})return 3;
    ii::gui::WindowPropertiesTarget endpoint;
    if(endpoint.update({.geometry={.title="Closed"}})||endpoint.snapshot()||!installed_window_factory)return 4;
    ii::gui::Host unopened;
    auto host_update=unopened.set_window_geometry({.title="Closed"});
    if(host_update||host_update.error().code!=ii::core::ErrorCode::closed)return 5;
    ii::core::Observable<LayoutRecord> record{{"Record",1}};int updates=0;
    auto lease=std::as_const(record).observe([&](const auto&,auto){++updates;});
    if(!record.set({"Record",1})||updates!=1||!installed_readonly_factory||!installed_object_input_factory||!installed_method_factory||!installed_method_reference_factory)return 6;
    std::cout<<"Relocated native layout, Window and readonly value factories and public contracts passed\n";
}
