#import <AppKit/AppKit.h>
#include <iiGUI/NativeWidgets.h>
#include <iostream>
#include <algorithm>
#include <stdexcept>
using namespace ii;
namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void good(core::Result<void> value){if(!value){if(value.error().cause)std::rethrow_exception(value.error().cause);throw std::runtime_error(value.error().message);}}
template<class T>T take(core::Result<T> value){if(!value)throw std::runtime_error(value.error().message);return std::move(*value);}
NSView* surface(NSView* view){if([view.identifier isEqualToString:@"iiGUI.widget.surface"])return view;for(NSView* child in view.subviews)if(auto found=surface(child))return found;return nil;}
void windows(){
    gui::Host parent;std::unique_ptr<gui::Host> child,other;int closed=0,late=0;gui::UiExecutor retired;
    good(parent.run({.opened=[&]{
        child=std::make_unique<gui::Host>(gui::HostOptions{.title="Preferences"});
        good(child->show(parent,{.opened=[&]{
            auto mount=take(child->mount_widgets([](gui::WidgetBuilder& b)->core::Result<void>{auto text=b.label({},"text",{.text="Secondary"});return text?core::Result<void>{}:std::unexpected(text.error());}));
            check(take(mount.find("text")).snapshot().has_value(),"Secondary widget mounting failed");
            retired=child->executor();check(bool(retired.after(std::chrono::seconds(1),[&]{++late;})),"Child timer admission failed");
            good(child->request_close());
        },.closed=[&]{
            ++closed;check(take(parent.snapshot()).visible,"Closing child stopped parent");check(!retired.post([&]{++late;}),"Closed child admitted work");
            child.reset();other=std::make_unique<gui::Host>();good(other->show(parent,{.opened=[&]{good(parent.request_close());}}));
        }}));
    }}));
    check(closed==1&&late==0&&other&&take(other->snapshot()).closed,"Shared-loop window lifetime failed");
}
void dialogs(){
    gui::Host host;bool selected=false;int menu_calls=0;
    good(host.run({.opened=[&]{
        check(!host.choose_file({.filters={{"Bad",{"../png"}}}},[](auto){}),"Invalid native dialog filter accepted");
        std::vector<gui::AppMenuItem> items{
            {.id="file",.title="File",.children={
                {.id="open",.title="Open",.shortcut=gui::AppShortcut{"o"},.triggered=[&]{++menu_calls;}}
            }}
        };
        good(host.set_app_menu(std::move(items)));auto file=NSApp.mainMenu.itemArray.lastObject;auto open=file.submenu.itemArray.firstObject;
        check([open.identifier isEqualToString:@"open"]&&[open.keyEquivalent isEqualToString:@"o"],"Native menu did not preserve identifiers/shortcut");
        check([NSApp sendAction:open.action to:open.target from:open],"Native menu action failed");
        good(host.choose_file({.title="Native selection",.filters={{"Canvas",{"iisc","png"}}}},[&](auto value){
            check(value&&value->canceled,"Native file cancellation was lost");selected=true;good(host.request_close());
        }));
        check(!host.choose_file({},[](auto){}),"Concurrent native dialog was admitted");
        check(bool(host.executor().after(std::chrono::milliseconds(60),[]{
            for(NSWindow* window in NSApp.windows)if(window.attachedSheet){[(NSSavePanel*)window.attachedSheet cancel:nil];return;}
            throw std::runtime_error("Native sheet was not attached");
        })),"Dialog cancellation schedule failed");
    }}));check(selected&&menu_calls==1,"Native OS services did not dispatch through Core");
}
void clipboard(){
    // Preserve every advertised item/type, including rich content outside this API.
    NSMutableArray<NSPasteboardItem*>* saved=[NSMutableArray new];
    for(NSPasteboardItem* old in NSPasteboard.generalPasteboard.pasteboardItems){auto copy=[NSPasteboardItem new];for(NSPasteboardType type in old.types)if(auto data=[old dataForType:type])[copy setData:data forType:type];[saved addObject:copy];}
    const auto restore=[&]{[NSPasteboard.generalPasteboard clearContents];if(saved.count)[NSPasteboard.generalPasteboard writeObjects:saved];};
    try{
        gui::ClipboardContent content;content.text="한글 🙂";content.url="https://example.com/image.png";content.image_mime="image/png";content.image={137,80,78,71,13,10,26,10};
        good(gui::write_clipboard(content));auto read=take(gui::read_clipboard());check(read.text==content.text&&read.url==content.url&&read.image==content.image,"Native clipboard payload did not round trip");
        check(!gui::open_external_url("file:///etc/passwd"),"Invalid external URL accepted");
        content.image_mime="unsupported";check(!gui::write_clipboard(content),"Unknown clipboard image type accepted");
        restore();
    }catch(...){restore();throw;}
}
void files(){
    gui::Host host;int opened=0;
    good(host.run({.opened=[&]{[NSApp.delegate application:NSApp openFiles:@[@"/Workspace/한글.iisc"]];},.files_opened=[&](std::vector<std::string> paths){check(std::ranges::find(paths,"/Workspace/한글.iisc")!=paths.end(),"Native file-open paths changed");++opened;good(host.request_close());}}));
    check(opened==1,"Native file-open callback was lost");
}
void ime(){
    gui::Host host;
    good(host.run({.opened=[&]{
        auto mount=take(host.mount_widgets([](gui::WidgetBuilder& b)->core::Result<void>{auto input=b.input_field({},"ime",{.text="A🙂한B",.multiline=true});return input?core::Result<void>{}:std::unexpected(input.error());}));
        auto field=take(mount.find("ime"));good(field.focus());NSView<NSTextInputClient>* view=nil;
        for(NSWindow* window in NSApp.windows)if(auto found=surface(window.contentView)){view=(NSView<NSTextInputClient>*)found;break;}
        check(view!=nil,"Native text surface missing");
        [view setMarkedText:@"ㅎ" selectedRange:NSMakeRange(1,0) replacementRange:NSMakeRange(3,1)];
        check([view hasMarkedText]&&NSEqualRanges([view markedRange],NSMakeRange(3,1))&&NSEqualRanges([view selectedRange],NSMakeRange(4,0)),"Native IME ranges were lost");
        [view insertText:@"국" replacementRange:NSMakeRange(3,1)];check(take(field.snapshot()).text=="A🙂국B"&&!view.hasMarkedText,"Native IME replacement did not commit");
        good(host.request_close());
    }}));
}
}
int main(int argc,char** argv){try{check(argc==2,"Expected case");std::string name=argv[1];if(name=="windows")windows();else if(name=="dialogs")dialogs();else if(name=="clipboard")clipboard();else if(name=="files")files();else if(name=="ime")ime();else throw std::runtime_error("Unknown desktop test");std::cout<<"DESKTOP_OK "<<name<<'\n';return 0;}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
