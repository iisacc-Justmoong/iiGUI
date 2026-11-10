#import <AppKit/AppKit.h>
#include "../src/Platform/CocoaBackingObserver.h"
#include <memory>
#include <stdexcept>

int main() {
    @autoreleasepool {
        auto center = [NSNotificationCenter new];
        auto window = [NSObject new];
        auto other = [NSObject new];
        int changes = 0;
        auto observer = std::make_unique<ii::gui::detail::CocoaBackingObserver>(center, window, [&]{++changes;});
        [center postNotificationName:NSWindowDidChangeBackingPropertiesNotification object:other];
        if (changes != 0) throw std::runtime_error("Unrelated window changed the scale");
        [center postNotificationName:NSWindowDidChangeBackingPropertiesNotification object:window];
        if (changes != 1) throw std::runtime_error("Backing change was not observed");
        observer.reset();
        [center postNotificationName:NSWindowDidChangeBackingPropertiesNotification object:window];
        if (changes != 1) throw std::runtime_error("Retired observer received an event");
        observer = std::make_unique<ii::gui::detail::CocoaBackingObserver>(center, window, [&]{++changes; observer.reset();});
        [center postNotificationName:NSWindowDidChangeBackingPropertiesNotification object:window];
        [center postNotificationName:NSWindowDidChangeBackingPropertiesNotification object:window];
        if (changes != 2 || observer) throw std::runtime_error("Reentrant retirement failed");
    }
}
