#pragma once
#import "NativeWidgetsIOS.h"

// Kept in a separate translation unit. The explicit initialization symbol
// ensures the category is linked even when iiGUI is consumed as a static SDK.
@interface IIGUIIOSSurface (UIKitTextInput) <UITextInput>
@end

namespace ii::gui::detail {
void initialize_uikit_text_input(IIGUIIOSSurface* view);
runtime::Task begin_uikit_text_input_change(IIGUIIOSSurface* view,bool value_changed);
}
