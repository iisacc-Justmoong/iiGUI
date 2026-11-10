#import <AppKit/AppKit.h>
#include <iiGUI/Host.h>
#include <iiGUI/NativeWidgets.h>
#include <cstdio>
#include <memory>
#include <string>

@interface IIGUITestParentView : NSView
@end
@implementation IIGUITestParentView
- (BOOL)acceptsFirstResponder { return YES; }
@end

namespace {
void stop(NSApplication* app) {
    [app stop:nil];
    [app postEvent:[NSEvent otherEventWithType:NSEventTypeApplicationDefined location:NSZeroPoint
        modifierFlags:0 timestamp:0 windowNumber:0 context:nil subtype:0 data1:0 data2:0] atStart:YES];
}
struct State final {
    std::unique_ptr<ii::gui::Host> host;
    std::string error;
    bool opened=false, closed=false;
    bool closeParent=false;
    NSUInteger initialSubviews=0;
    __strong NSWindow* window=nil;
    __strong NSView* parent=nil;
    __strong id<NSWindowDelegate> windowDelegate=nil;
    __strong id<NSApplicationDelegate> appDelegate=nil;
};
}

int main(int argc, char**) {
    @autoreleasepool {
        NSApplication* app=NSApplication.sharedApplication;
        [app setActivationPolicy:NSApplicationActivationPolicyProhibited];
        auto state=std::make_shared<State>();
        state->closeParent=argc>1;
        state->appDelegate=(id<NSApplicationDelegate>)[NSObject new];app.delegate=state->appDelegate;
        state->window=[[NSWindow alloc] initWithContentRect:NSMakeRect(0,0,400,240)
            styleMask:NSWindowStyleMaskBorderless backing:NSBackingStoreBuffered defer:NO];
        state->window.releasedWhenClosed=NO;
        state->window.contentView=[[IIGUITestParentView alloc] initWithFrame:NSMakeRect(0,0,400,240)];
        state->windowDelegate=(id<NSWindowDelegate>)[NSObject new];state->window.delegate=state->windowDelegate;
        state->parent=state->window.contentView;state->initialSubviews=state->parent.subviews.count;
        if(![state->window makeFirstResponder:state->parent])return 1;
        state->host=std::make_unique<ii::gui::Host>();
        dispatch_async(dispatch_get_main_queue(), ^{
            auto opened=[state] {
                state->opened=true;
                auto mounted=state->host->mount_widgets([](ii::gui::WidgetBuilder& builder)->ii::core::Result<void>{
                    auto root=builder.vstack({},"root",{});if(!root)return std::unexpected(root.error());
                    auto text=builder.label(*root,"message",{.text="Embedded view"});
                    if(!text)return std::unexpected(text.error());return {};
                });
                if(!mounted){
                    state->error="The native widget surface could not be mounted: "+mounted.error().message;
                    if(mounted.error().cause)try{std::rethrow_exception(mounted.error().cause);}
                    catch(const std::exception& error){state->error+=" / ";state->error+=error.what();}
                }
                else if(state->parent.subviews.count!=state->initialSubviews+1)
                    state->error="The native widget surface was not mounted in the supplied view";
                if (mounted) {
                    NSView* surface=state->parent.subviews.lastObject;
                    [surface display];
                    bool transparentMetal=false;
                    for(NSView* child in surface.subviews)
                        if([child.layer isKindOfClass:NSClassFromString(@"CAMetalLayer")] && !child.layer.opaque)
                            transparentMetal=true;
                    if(!transparentMetal)state->error="Embedded Metal surface is not transparent after rendering";
                }
                if(state->window.delegate!=state->windowDelegate)
                    state->error="The external window delegate was replaced";
                auto title=state->host->set_title("must not replace the external window title");
                if(title || title.error().code!=ii::core::ErrorCode::invalid_argument)
                    state->error="Embedded host accepted external window mutation";
                if(state->closeParent)[state->window close];
                else if(!state->host->request_close())state->error="Embedded host could not close";
            };
            auto closed=[state,app] {
                state->closed=true;
                if(state->parent.subviews.count!=state->initialSubviews || state->window.delegate!=state->windowDelegate ||
                   (!state->closeParent && state->window.firstResponder!=state->parent) ||
                   app.delegate!=state->appDelegate || !app.running)
                    state->error="Embedded host did not restore the external window";
                state->host.reset();stop(app);
            };
            auto attached=state->host->attach_external((__bridge void*)state->parent,
                {.opened=std::move(opened),.closed=std::move(closed)});
            if(!attached){state->error=attached.error().message;stop(app);}
        });
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW,5*NSEC_PER_SEC),dispatch_get_main_queue(), ^{
            if(!state->closed){state->error="Embedded host timed out";if(state->host)(void)state->host->request_close();stop(app);}
        });
        [app run];
        if(!state->error.empty() || !state->opened || !state->closed){
            std::fprintf(stderr,"%s\n",state->error.empty()?"Embedded host callbacks were lost":state->error.c_str());return 1;
        }
        return 0;
    }
}
