#import <AppKit/AppKit.h>
#include <iiGUI/Host.h>
#include <iiGUI/NativeWidgets.h>
#include <iiGUI/WidgetSurfaceNative.h>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

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

// Deliver AppKit mouse events without depending on the global mouse state.
// In particular, these events do not update NSEvent.pressedMouseButtons.
void check_pointer_buttons(State& state) {
    using Kind = ii::gui::WidgetInputKind;
    std::vector<ii::gui::WidgetInput> inputs;
    ii::gui::detail::NativeWidgetSurface native_surface((__bridge void*)state.parent,
        {.input = [&](ii::gui::WidgetInput input) { inputs.push_back(std::move(input)); }});
    NSView* surface = state.parent.subviews.lastObject;
    const auto modifiers = NSEventModifierFlagCommand | NSEventModifierFlagOption;
    const auto event = [&](NSEventType type, NSPoint location) {
        return [NSEvent mouseEventWithType:type location:location modifierFlags:modifiers
            timestamp:NSProcessInfo.processInfo.systemUptime windowNumber:state.window.windowNumber
            context:nil eventNumber:0 clickCount:1 pressure:1];
    };
    [surface mouseDown:event(NSEventTypeLeftMouseDown, NSMakePoint(24, 32))];
    [surface mouseDragged:event(NSEventTypeLeftMouseDragged, NSMakePoint(40, 32))];
    [surface mouseUp:event(NSEventTypeLeftMouseUp, NSMakePoint(40, 32))];
    [surface rightMouseDown:event(NSEventTypeRightMouseDown, NSMakePoint(24, 32))];
    [surface rightMouseUp:event(NSEventTypeRightMouseUp, NSMakePoint(24, 32))];
    const auto check = [&](std::size_t index, Kind kind, std::uint32_t buttons) {
        const auto& input = inputs[index];
        return input.kind == kind && input.buttons == buttons && input.alt && input.meta;
    };
    if (inputs.size() != 5 || !check(0, Kind::pointer_down, 1) ||
        !check(1, Kind::pointer_move, 1) || !check(2, Kind::pointer_up, 0) ||
        !check(3, Kind::context_request, 2) || !check(4, Kind::pointer_up, 0)) {
        state.error = "AppKit events lost their changed button or modifier flags";
        for (const auto& input : inputs)
            std::fprintf(stderr, "kind=%d buttons=%u option=%d command=%d\n",
                static_cast<int>(input.kind), input.buttons, input.alt, input.meta);
    }
}
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
            check_pointer_buttons(*state);
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
                [state->parent setFrameSize:NSMakeSize(280,160)];
                const auto resized=state->host->external_parent_resized();
                const auto snapshot=state->host->snapshot();
                if(!resized || !snapshot || snapshot->size.width!=280 || snapshot->size.height!=160)
                    state->error="The embedded host did not adopt its explicitly resized parent";
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
