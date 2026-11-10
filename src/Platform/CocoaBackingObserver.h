#pragma once
#import <AppKit/AppKit.h>
#include <functional>
#include <stdexcept>
#include <utility>

namespace ii::gui::detail {
// Only for borrowed windows, whose delegate is owned by another application.
// Notification delivery and retirement belong to the host's AppKit thread.
class CocoaBackingObserver final {
    NSNotificationCenter* center_;
    id token_;
public:
    CocoaBackingObserver(NSNotificationCenter* center, id window, std::function<void()> changed)
        : center_(center), token_(nil) {
        if (!center || !window || !changed) throw std::invalid_argument("Backing observer requires a window and callback");
        token_ = [center_ addObserverForName:NSWindowDidChangeBackingPropertiesNotification
                                      object:window queue:nil usingBlock:^(NSNotification*) {
            auto action = changed;
            if (action) action();
        }];
        if (!token_) throw std::runtime_error("Cannot observe window backing changes");
    }
    CocoaBackingObserver(const CocoaBackingObserver&) = delete;
    CocoaBackingObserver& operator=(const CocoaBackingObserver&) = delete;
    ~CocoaBackingObserver() { if (token_) [center_ removeObserver:token_]; }
};
}
