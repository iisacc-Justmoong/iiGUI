#import <AppKit/AppKit.h>
#include <iiGUI/NativeWidgets.h>
#include <iostream>
#include <stdexcept>
using namespace ii;
namespace {
template<class T>T take(core::Result<T> r){if(!r)throw std::runtime_error(r.error().message);return std::move(*r);}
void good(core::Result<void> r){if(!r){if(r.error().cause)std::rethrow_exception(r.error().cause);throw std::runtime_error(r.error().message);}}
void check(bool v,const char* m){if(!v)throw std::runtime_error(m);}
unsigned surfaces(NSView* v){check(![v isKindOfClass:NSControl.class]&&![v isKindOfClass:NSStackView.class],"OS control or layout created by activation");unsigned n=[v.identifier isEqualToString:@"iiGUI.widget.surface"]?1:0;for(NSView* child in v.subviews)n+=surfaces(child);return n;}
NSView* surface(NSView* v){if([v.identifier isEqualToString:@"iiGUI.widget.surface"])return v;for(NSView* child in v.subviews)if(auto found=surface(child))return found;return nil;}
id element(NSArray* entries,NSString* identity){for(id item in entries){if([item respondsToSelector:@selector(accessibilityIdentifier)]&&[[item accessibilityIdentifier] isEqualToString:identity])return item;if([item respondsToSelector:@selector(accessibilityChildren)])if(id child=element([item accessibilityChildren],identity))return child;}return nil;}
}
int main(){try{
    gui::Host host({.title="iiGUI activation native boundary"});good(host.run({.opened=[&]{gui::Widget popup;int clicks=0;std::string edited;auto m=take(host.mount_widgets([&](gui::WidgetBuilder& b)->core::Result<void>{auto root=take(b.vstack({},"root"));popup=take(b.popover(root,"popup",{.width=260,.height=160}));return popup.set_popup_content([&](gui::WidgetBuilder&w)->core::Result<void>{auto root=take(w.vstack({},"form"));take(w.input_field(root,"field",{.text=""},[&](std::string s){edited=std::move(s);}));take(w.button(root,"button",{.text="Action"},[&]{++clicks;}));return {};});}));
        NSWindow* window=nil;for(NSWindow* candidate in NSApp.windows)if([candidate.title isEqualToString:@"iiGUI activation native boundary"]){window=candidate;break;}check(window!=nil,"Native host window missing");auto view=surface(window.contentView);check(view&&surfaces(window.contentView)==1,"Initial own-rendered surface missing");good(popup.open_at(40,70));auto field=take(popup.popup_content("field")),button=take(popup.popup_content("button"));auto field_id=take(field.snapshot()).id,button_id=take(button.snapshot()).id;
        id focused=[view accessibilityFocusedUIElement];check(focused&&[[focused accessibilityIdentifier] isEqualToString:[NSString stringWithUTF8String:field_id.c_str()]],"Native focus/IME lookup selected the popup shell instead of its active field");
        [(id<NSTextInputClient>)view insertText:@"native" replacementRange:NSMakeRange(NSNotFound,0)];check(edited=="native","Native text input did not reach active content");id stale=element([view accessibilityChildren],[NSString stringWithUTF8String:button_id.c_str()]);check(stale&&[stale accessibilityPerformPress]&&clicks==1,"Virtual AX action did not route to activation");
        good(popup.close_popup());good(popup.open_at(40,70));check(![stale accessibilityPerformPress]&&clicks==1,"Retired virtual AX element activated replacement content");good(popup.close_popup());
        for(int i=0;i<24;++i){good(popup.open_at(40,70));check(surface(window.contentView)==view&&surfaces(window.contentView)==1,"Reopen created a native surface/control per activation");good(popup.close_popup());(void)[view accessibilityChildren];check([(NSDictionary*)[view valueForKey:@"peers"] count]<=2,"Retired activation AX peers accumulated in the native cache");}good(host.request_close());}}));std::cout<<"POPUP_CONTENT_NATIVE_OK surfaces=1 native_text=true stale_ax_rejected=true ax_cache_bounded=true\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
