#include "CocoaHostDelegate.h"
#include "HostFileRequests.h"
#include <utility>

@implementation IIGUIHostDelegate
- (void)applicationDidBecomeActive:(NSNotification*)notification {
    (void)notification;if(callbacks && callbacks->activity)callbacks->activity(true);
}
- (void)applicationDidResignActive:(NSNotification*)notification {
    (void)notification;if(callbacks && callbacks->activity)callbacks->activity(false);
}
- (void)applicationDidHide:(NSNotification*)notification {
    (void)notification;if(callbacks && callbacks->application_hidden)callbacks->application_hidden(true);
}
- (void)applicationDidUnhide:(NSNotification*)notification {
    (void)notification;if(callbacks && callbacks->application_hidden)callbacks->application_hidden(false);
}
- (void)application:(NSApplication*)application openFiles:(NSArray<NSString*>*)filenames {
    if(!callbacks||!callbacks->files||filenames.count>ii::gui::detail::HostFileRequests::maximum_paths){[application replyToOpenOrPrint:NSApplicationDelegateReplyFailure];return;}
    try {
        std::vector<std::string> paths;
        paths.reserve(filenames.count);
        for(NSString* path in filenames){
            NSData* bytes=[path dataUsingEncoding:NSUTF8StringEncoding allowLossyConversion:NO];
            if(!bytes||!bytes.length||bytes.length>ii::gui::detail::HostFileRequests::maximum_path_bytes){[application replyToOpenOrPrint:NSApplicationDelegateReplyFailure];return;}
            paths.emplace_back(static_cast<const char*>(bytes.bytes),bytes.length);
        }
        const bool accepted=callbacks->files(std::move(paths));
        [application replyToOpenOrPrint:(accepted ? NSApplicationDelegateReplySuccess : NSApplicationDelegateReplyFailure)];
    } catch (...) { [application replyToOpenOrPrint:NSApplicationDelegateReplyFailure]; }
}
- (void)displayFrame:(CADisplayLink*)sender {
    if(callbacks && callbacks->frame) callbacks->frame(sender.targetTimestamp,sender.targetTimestamp-sender.timestamp);
}
- (void)windowDidResize:(NSNotification*)notification {
    (void)notification;
    if (callbacks && callbacks->resize) callbacks->resize();
}
- (void)windowDidChangeBackingProperties:(NSNotification*)notification {
    (void)notification;
    if (callbacks && callbacks->backing_changed) callbacks->backing_changed();
}
- (void)windowDidMiniaturize:(NSNotification*)notification {
    (void)notification;if(callbacks && callbacks->window_visible)callbacks->window_visible(false);
}
- (void)windowDidDeminiaturize:(NSNotification*)notification {
    (void)notification;if(callbacks && callbacks->window_visible)callbacks->window_visible(true);
}
- (BOOL)windowShouldClose:(NSWindow*)sender {
    (void)sender;
    return !callbacks || !callbacks->may_close || callbacks->may_close();
}
- (void)windowWillClose:(NSNotification*)notification {
    (void)notification;
    if (callbacks && callbacks->did_close) callbacks->did_close();
}
- (void)requestQuit:(id)sender {
    (void)sender;
    if (callbacks && callbacks->request_close) callbacks->request_close();
}
- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication*)sender {
    (void)sender;
    [self requestQuit:nil];
    // Return through Host::run so C++ owners and Runtime workers can shut down.
    return NSTerminateCancel;
}
@end
