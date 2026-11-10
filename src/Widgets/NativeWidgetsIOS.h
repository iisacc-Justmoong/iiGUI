#pragma once
#import <UIKit/UIKit.h>
#include <iiGUI/WidgetSurface.h>
#include <iiSDKCore/Runtime/TouchInput.h>
#include "../Platform/UIKitTextInputProtocol.h"

@class IIGUIIOSElement;
@interface IIGUIIOSSurface : UIView <UITextInputTraits> {
@public
    ii::gui::detail::WidgetSurfaceCallbacks callbacks;
    std::function<void()> paint;
    __weak UITouch* pointer;
    NSMutableDictionary<NSString*,IIGUIIOSElement*>* peers;
    NSString* editing;
    std::shared_ptr<const void> editingIdentity;
    std::uint64_t editingFocus;
    bool editingReadOnly,editingSecure,editingMultiline,syncingInputViews,pendingInputViews;
    UIView* hiddenKeyboard;
    ii::runtime::touch::TouchRouter touchRouter;
    std::shared_ptr<ii::gui::detail::UIKitTextInputProtocol> textProtocol;
    __weak id<UITextInputDelegate> textDelegate;
    NSDictionary<NSAttributedStringKey,id>* markedStyle;
    UITextInputStringTokenizer* textTokenizer;
    unsigned int nativeTextDepth;
    unsigned int textNoticeDepth;
    BOOL textNoticeValue;
    id<UITextInputDelegate> textNoticeDelegate;
}
- (std::optional<ii::gui::WidgetSnapshot>)editor;
- (void)synchronizeEditor;
- (void)deliver:(ii::gui::WidgetInput)value;
@end
