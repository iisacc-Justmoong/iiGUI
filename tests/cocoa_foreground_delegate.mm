#import "../src/Platform/CocoaHostDelegate.h"
#include "../src/Platform/WindowForeground.h"
#include <stdexcept>
#include <vector>

int main() {
    @autoreleasepool {
        ii::gui::detail::NativeCallbacks callbacks;
        std::vector<bool> app_hidden, window_visible;
        callbacks.application_hidden = [&](bool value) { app_hidden.push_back(value); };
        callbacks.window_visible = [&](bool value) { window_visible.push_back(value); };
        IIGUIHostDelegate* delegate = [IIGUIHostDelegate new];
        delegate->callbacks = &callbacks;
        NSNotification* notification = [NSNotification notificationWithName:@"iiGUI.Foreground.Test" object:nil];
        [delegate applicationDidHide:notification];
        [delegate applicationDidUnhide:notification];
        [delegate windowDidMiniaturize:notification];
        [delegate windowDidDeminiaturize:notification];
        int resize_events = 0, backing_events = 0;
        callbacks.resize = [&]{++resize_events;};callbacks.backing_changed = [&]{++backing_events;};
        [delegate windowDidResize:notification];[delegate windowDidChangeBackingProperties:notification];
        if (resize_events != 1 || backing_events != 1) throw std::runtime_error("Backing and resize events were not separated");
        if (app_hidden != std::vector<bool>({true, false}) || window_visible != std::vector<bool>({false, true}))
            throw std::runtime_error("AppKit lifecycle notifications were not routed to the host");
        ii::gui::detail::WindowForeground tracker;
        int window = 0;
        std::vector<bool> foreground;
        tracker.set_handler([&](bool value) { foreground.push_back(value); });
        tracker.open_window(&window, true);
        tracker.open();
        tracker.deliver();
        callbacks.application_hidden = [&](bool value) { tracker.set_hidden(value); };
        callbacks.window_visible = [&](bool value) { tracker.set_window_visible(&window, value); };
        [delegate applicationDidHide:notification]; tracker.deliver();
        [delegate applicationDidUnhide:notification]; tracker.deliver();
        [delegate windowDidMiniaturize:notification]; tracker.deliver();
        [delegate windowDidDeminiaturize:notification]; tracker.deliver();
        if (foreground != std::vector<bool>({true, false, true, false, true}))
            throw std::runtime_error("AppKit notifications did not update the aggregated lifecycle");
        tracker.close();
        delegate->callbacks = nullptr;
        [delegate applicationDidHide:notification];
        [delegate windowDidMiniaturize:notification];
        if (app_hidden.size() != 2 || window_visible.size() != 2)
            throw std::runtime_error("Retired AppKit callbacks remained live");
    }
}
