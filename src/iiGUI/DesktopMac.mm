#import <AppKit/AppKit.h>
#include "DesktopCommon.h"
namespace ii::gui {
namespace {
NSString* native(std::string_view value){return [[NSString alloc] initWithBytes:value.data() length:value.size() encoding:NSUTF8StringEncoding];}
core::Result<void> main_thread(){if(!NSThread.isMainThread)return core::fail(core::ErrorCode::wrong_executor,"Desktop services require the main thread");return {};}
}
namespace detail {
core::Result<ClipboardContent> read_pasteboard(void* native_board){
    if(auto ready=main_thread();!ready)return std::unexpected(ready.error());
    ClipboardContent value;auto board=(__bridge NSPasteboard*)native_board;
    if(auto text=[board stringForType:NSPasteboardTypeString])value.text=text.UTF8String;
    if(auto text=[board stringForType:NSPasteboardTypeURL])value.url=text.UTF8String;
    auto files=[board readObjectsForClasses:@[NSURL.class] options:@{NSPasteboardURLReadingFileURLsOnlyKey:@YES}];
    if(files.count>256)return core::fail(core::ErrorCode::out_of_range,"Too many clipboard files");
    for(NSURL* url in files)if(url.fileURL)value.files.emplace_back(url.path.UTF8String);
    for(const auto& pair:std::vector<std::pair<NSPasteboardType,std::string>>{{NSPasteboardTypePNG,"image/png"},{NSPasteboardTypeTIFF,"image/tiff"},{@"public.jpeg","image/jpeg"},{@"com.microsoft.bmp","image/bmp"}}){
        auto data=[board dataForType:pair.first];if(!data)continue;
        if(data.length>64*1024*1024)return core::fail(core::ErrorCode::out_of_range,"Clipboard image exceeds 64 MiB");
        const auto* bytes=static_cast<const std::uint8_t*>(data.bytes);value.image.assign(bytes,bytes+data.length);value.image_mime=pair.second;break;
    }
    if(!detail::valid_clipboard(value))return core::fail(core::ErrorCode::invalid_argument,"Invalid clipboard payload");return value;
}
}
core::Result<ClipboardContent> read_clipboard(){return detail::read_pasteboard((__bridge void*)NSPasteboard.generalPasteboard);}
core::Result<void> write_clipboard(const ClipboardContent& value){
    if(auto ready=main_thread();!ready)return ready;
    if(!detail::valid_clipboard(value))return core::fail(core::ErrorCode::invalid_argument,"Invalid clipboard payload");
    auto board=NSPasteboard.generalPasteboard;[board clearContents];
    if(!value.text.empty())[board setString:native(value.text) forType:NSPasteboardTypeString];
    if(!value.url.empty())[board setString:native(value.url) forType:NSPasteboardTypeURL];
    if(!value.files.empty()){NSMutableArray* files=[NSMutableArray new];for(const auto& path:value.files)[files addObject:[NSURL fileURLWithPath:native(path)]];if(![board writeObjects:files])return core::fail(core::ErrorCode::failed,"Cannot write clipboard files");}
    if(!value.image.empty()){
        auto type=value.image_mime=="image/png"?NSPasteboardTypePNG:value.image_mime=="image/tiff"?NSPasteboardTypeTIFF:value.image_mime=="image/bmp"?@"com.microsoft.bmp":@"public.jpeg";
        if(![board setData:[NSData dataWithBytes:value.image.data() length:value.image.size()] forType:type])return core::fail(core::ErrorCode::failed,"Cannot write clipboard image");
    }
    return {};
}
core::Result<void> open_external_url(std::string_view value){
    if(auto ready=main_thread();!ready)return ready;
    if(!detail::valid_title(value)||value.size()>8192||!(value.starts_with("https://")||value.starts_with("http://")||value.starts_with("mailto:")))return core::fail(core::ErrorCode::invalid_argument,"Expected an HTTP(S) or mailto URL");
    if(![NSWorkspace.sharedWorkspace openURL:[NSURL URLWithString:native(value)]])return core::fail(core::ErrorCode::failed,"Cannot open external URL");return {};
}
}
