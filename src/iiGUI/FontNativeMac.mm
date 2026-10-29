#import <Foundation/Foundation.h>
#import <CoreText/CoreText.h>
#include "FontNative.h"
#include <algorithm>

namespace ii::gui::detail {
namespace {
NSString* native(std::string_view value){return [[NSString alloc] initWithBytes:value.data() length:value.size() encoding:NSUTF8StringEncoding];}
std::string string(CFStringRef value){if(!value)return {};auto text=(__bridge NSString*)value;return std::string(text.UTF8String?text.UTF8String:"");}
std::string family(CTFontRef font){if(!font)return {};auto value=CTFontCopyFamilyName(font);auto result=string(value);if(value)CFRelease(value);return result;}
class Provider final:public FontProvider {
    std::string directory_;
public:
    explicit Provider(std::string directory):directory_(std::move(directory)){}
    std::vector<std::string> families()const override{
        @autoreleasepool {
            auto values=CTFontManagerCopyAvailableFontFamilyNames();std::vector<std::string> result;
            if(values){for(CFIndex i=0;i<CFArrayGetCount(values);++i)result.push_back(string(static_cast<CFStringRef>(CFArrayGetValueAtIndex(values,i))));CFRelease(values);}
            std::ranges::sort(result);result.erase(std::unique(result.begin(),result.end()),result.end());return result;
        }
    }
    std::string system_family()const override{
        @autoreleasepool {auto font=CTFontCreateUIFontForLanguage(kCTFontUIFontSystem,13,nullptr);if(!font)throw std::runtime_error("Cannot query the native system font");auto result=family(font);CFRelease(font);return result;}
    }
    std::string resolve(const FontFace& face)const override{
        @autoreleasepool {
            auto attributes=@{(__bridge NSString*)kCTFontFamilyNameAttribute:native(face.family),(__bridge NSString*)kCTFontStyleNameAttribute:native(face.style)};
            auto descriptor=CTFontDescriptorCreateWithAttributes((__bridge CFDictionaryRef)attributes);
            auto font=CTFontCreateWithFontDescriptor(descriptor,13,nullptr);CFRelease(descriptor);
            auto result=family(font);if(font)CFRelease(font);return result;
        }
    }
    FontLoadReport load(std::span<const FontResource> resources)override{
        @autoreleasepool {
            FontLoadReport result;const auto directory=font_resource_directory(directory_);
            for(const auto& resource:resources){
                const auto file=directory/std::string(resource.file);auto path=native(file.string());auto url=[NSURL fileURLWithPath:path];
                CFErrorRef error=nullptr;bool loaded=CTFontManagerRegisterFontsForURL((__bridge CFURLRef)url,kCTFontManagerScopeProcess,&error);
                std::string failure;
                if(error){if(CFEqual(CFErrorGetDomain(error),kCTFontManagerErrorDomain)&&CFErrorGetCode(error)==kCTFontManagerErrorAlreadyRegistered)loaded=true;
                    if(!loaded){auto description=CFErrorCopyDescription(error);failure=string(description);if(description)CFRelease(description);}CFRelease(error);}
                if(!loaded&&failure.empty())failure="Native font registration failed";
                result.resources.push_back({std::string(resource.file),loaded,std::move(failure)});
            }
            return result;
        }
    }
};
}
std::filesystem::path font_resource_directory(std::string_view configured){
    if(!configured.empty())return std::filesystem::path(configured);
    @autoreleasepool {auto path=NSBundle.mainBundle.resourcePath;if(!path)throw std::runtime_error("Native bundle has no resource directory");return std::filesystem::path(path.UTF8String);}
}
std::shared_ptr<FontProvider> native_font_provider(std::string directory){return std::make_shared<Provider>(std::move(directory));}
}
