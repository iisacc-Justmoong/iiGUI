#pragma once

#import <AppKit/AppKit.h>
#import <QuartzCore/QuartzCore.h>
#include <functional>
#include <string>
#include <vector>

namespace ii::gui::detail {
struct NativeCallbacks final {
    std::function<void()> resize,backing_changed;
    std::function<bool()> may_close;
    std::function<void()> did_close;
    std::function<void()> request_close;
    std::function<void(double,double)> frame;
    std::function<bool(std::vector<std::string>)> files;
    std::function<void(bool)> activity;
    std::function<void(bool)> application_hidden;
    std::function<void(bool)> window_visible;
};
}

@interface IIGUIHostDelegate : NSObject <NSWindowDelegate, NSApplicationDelegate> {
@public
    ii::gui::detail::NativeCallbacks* callbacks;
}
- (void)requestQuit:(id)sender;
- (void)displayFrame:(CADisplayLink*)sender API_AVAILABLE(macos(14.0));
@end
