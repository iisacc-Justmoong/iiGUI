#import <AppKit/AppKit.h>
#include <iiGUI/NativeWidgets.h>
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace ii;
namespace {
void check(bool v,const char* s) { if(!v) throw std::runtime_error(s); }
template<class T> T take(core::Result<T> r) { if(!r) throw std::runtime_error(r.error().message);return std::move(*r); }
void good(core::Result<void> r) { if(!r) { if(r.error().cause) std::rethrow_exception(r.error().cause);throw std::runtime_error(r.error().message); } }
void key(gui::WidgetMount m,gui::WidgetKey k) { good(m.dispatch_input({.kind=gui::WidgetInputKind::key_down,.key=k})); }
void pointer(gui::WidgetMount m,gui::WidgetInputKind kind,gui::Rect r) { good(m.dispatch_input({.kind=kind,.x=r.x+r.width/2,.y=r.y+r.height/2})); }
NSView* surface(NSView* v) { if([v.identifier isEqualToString:@"iiGUI.widget.surface"]) return v;for(NSView* c in v.subviews) if(auto s=surface(c)) return s;return nil; }
void no_controls(NSView* v) { check(![v isKindOfClass:NSControl.class] && ![v isKindOfClass:NSStackView.class],"OS control in own menu");for(NSView* c in v.subviews) no_controls(c); }
struct Fixture final {
    gui::WidgetMount mount;
    gui::Widget menu,button,first,second,disabled,divider;
    int base_clicks=0,clicks=0,opened=0,closed=0,index=-1;
    std::string order;
    void build(gui::Host& host,gui::MenuOptions options={}) {
        mount=take(host.mount_widgets([&](gui::WidgetBuilder& b)->core::Result<void> {
            auto root=take(b.vstack({},"root",{.padding=24}));
            button=take(b.button(root,"button",{.text="Behind popup"},[&]{++base_clicks;}));
            take(b.label(root,"text",{.text="UNDERLYING TEXT"}));
            menu=take(b.menu(root,"menu",options,{.triggered=[&](int i){index=i;order+='t';},.opened=[&]{++opened;},.closed=[&]{++closed;order+='c';}}));
            first=take(b.menu_item(menu,"first",{.label="Open",.shortcut="⌘O",.show_icon_slot=false,.has_child_items=false},[&]{++clicks;order+='a';}));
            disabled=take(b.menu_item(menu,"disabled",{.label="Inactive",.state=gui::MenuItemState::inactive,.show_icon_slot=false,.has_child_items=false}));
            divider=take(b.menu_divider(menu,"divider"));
            second=take(b.menu_item(menu,"second",{.label="More",.shortcut="",.show_icon_slot=false,.has_child_items=true},[&]{++clicks;order+='b';}));return {};
        }));
    }
};
void lifecycle() {
    gui::Host host;Fixture f;
    good(host.run({.opened=[&] {
        f.build(host);auto before=take(f.button.snapshot()).bounds;
        check(!take(f.menu.snapshot()).opened && !take(f.first.snapshot()).visible,"Closed menu leaked into visible tree");
        good(f.button.focus());good(f.menu.open_for(f.button,0,before.height));
        auto menu=take(f.menu.snapshot());check(menu.opened && menu.menu_count==4 && f.opened==1,"Menu open or entry inventory failed");
        check(take(f.button.snapshot()).bounds==before && take(f.button.snapshot()).input_blocked,"Popup changed flow layout or did not capture input");
        check(take(f.first.snapshot()).size.height==24 && take(f.first.snapshot()).focused,"Regular menu metrics or initial focus failed");
        good(f.first.activate());check(f.clicks==1 && f.index==0 && f.order=="atc" && !take(f.menu.snapshot()).opened,"Trigger order or automatic close failed");
        check(take(f.button.snapshot()).focused && f.closed==1,"Menu close did not restore opener focus");
        good(f.menu.close_popup());check(f.closed==1,"Repeated close emitted a second event");
        good(f.menu.open_at(40,40));good(f.second.activate());
        check(take(f.menu.snapshot()).opened && f.index==3,"Child-bearing entry did not keep menu open or divider shifted its index");
        check(!f.menu.open_at(std::numeric_limits<double>::quiet_NaN(),0),"NaN popup position accepted");
        good(f.menu.close_popup());good(host.request_close());
    }}));
}
void placement() {
    gui::Host host;Fixture f;
    good(host.run({.opened=[&] {
        f.build(host,{.item_width=200});auto size=take(host.snapshot()).size;
        good(f.menu.open_at(size.width-2,size.height-2));auto a=take(f.menu.snapshot());
        check(a.bounds.x>=4 && a.bounds.y>=4 && a.bounds.x+a.bounds.width<=size.width-3 && a.bounds.y+a.bounds.height<=size.height-3,"Popup escaped visible edges");
        check(a.horizontal_direction==gui::MenuDirection::left && a.vertical_direction==gui::MenuDirection::up,"Popup did not flip at bottom/right edges");
        good(f.menu.open_at(-500,-500));auto b=take(f.menu.snapshot());check(b.bounds.x==4 && b.bounds.y==4,"Negative position did not clamp to margin");
        check(f.opened==1,"Repositioning an open popup emitted another opened event");
        good(f.menu.set_selected_index(3));check(take(f.second.snapshot()).selected,"Silent selectedIndex styling failed");
        check(!f.menu.set_selected_index(2) && !f.menu.set_selected_index(99),"Divider or invalid selection accepted");
        good(f.second.set_menu_state(gui::MenuItemState::inactive));good(f.second.set_enabled(true));
        check(!take(f.second.snapshot()).enabled,"Enabled setter bypassed inactive menu state");
        good(f.second.set_menu_state(gui::MenuItemState::normal));check(take(f.second.snapshot()).enabled,"State mutation did not restore requested enable state");
        good(host.request_close());
    }}));
}
void input() {
    gui::Host host;Fixture f;
    good(host.run({.opened=[&] {
        f.build(host);good(f.menu.open_at(30,80));
        key(f.mount,gui::WidgetKey::down);check(take(f.second.snapshot()).focused,"Down did not skip inactive entry and divider");
        key(f.mount,gui::WidgetKey::tab);check(take(f.first.snapshot()).focused,"Popup Tab escaped its focus scope");
        key(f.mount,gui::WidgetKey::end);check(take(f.second.snapshot()).focused,"End did not find last enabled entry");
        key(f.mount,gui::WidgetKey::home);key(f.mount,gui::WidgetKey::enter);good(f.mount.dispatch_input({.kind=gui::WidgetInputKind::key_up,.key=gui::WidgetKey::enter}));
        check(f.clicks==1 && !take(f.menu.snapshot()).opened,"Keyboard did not trigger and close own menu");
        good(f.menu.open_at(30,80));auto r=take(f.button.snapshot()).bounds;
        pointer(f.mount,gui::WidgetInputKind::pointer_down,r);pointer(f.mount,gui::WidgetInputKind::pointer_up,r);
        check(!take(f.menu.snapshot()).opened && f.base_clicks==0,"Outside dismissal clicked through to underlying button");
        good(f.menu.open_at(30,80));good(f.mount.dispatch_input({.kind=gui::WidgetInputKind::context_request,.x=0,.y=0}));
        check(!take(f.menu.snapshot()).opened,"Outside context request did not dismiss popup");
        good(f.menu.open_at(30,80));key(f.mount,gui::WidgetKey::escape);check(!take(f.menu.snapshot()).opened,"Escape did not close menu");
        good(f.menu.open_at(30,80));good(f.mount.dispatch_input({.kind=gui::WidgetInputKind::focus_lost}));check(!take(f.menu.snapshot()).opened,"Window focus loss left popup open");
        good(host.request_close());
    }}));
}
void rendering() {
    gui::Host host;Fixture f;
    good(host.run({.opened=[&] {
        f.build(host,{.item_width=220,.top_padding=30,.opacity=1,.background=gui::WidgetColor{.2,.3,.4,1}});
        auto behind=take(f.button.snapshot()).bounds;good(f.menu.open_at(behind.x,behind.y));
        auto image=take(host.capture_canvas());auto scale=take(host.snapshot()).backing_scale;auto r=take(f.menu.snapshot()).bounds;
        for(int x=24;x<160;++x) {
            auto offset=(std::size_t((r.y+behind.height/2)*scale)*image.width+std::size_t((r.x+x)*scale))*4;
            check(std::abs(int(image.rgba[offset])-51)<=2 && std::abs(int(image.rgba[offset+2])-102)<=2,"Popup layer did not cover underlying panels and text");
        }
        auto before=take(host.graphics_info());good(f.menu.close_popup());(void)take(host.capture_canvas());
        check(take(host.graphics_info()).foreground_texture_bytes<=before.foreground_texture_bytes,"Closed popup retained a foreground layer");
        auto compact=take(host.mount_widgets([](gui::WidgetBuilder& b)->core::Result<void> {
            auto root=take(b.vstack({},"root"));auto menu=take(b.menu(root,"context",{.style=gui::MenuStyle::context}));
            take(b.menu_item(menu,"compact",{.label="Compact",.shortcut="Key",.show_icon_slot=false,.has_child_items=false}));return {};
        }));
        auto menu=take(compact.find("context"));good(menu.open_at(10,10));
        auto row=take(take(compact.find("compact")).snapshot());check(row.compact && row.size.height==18 && row.font_size==12,"Compact menu paint or typography contract missing");
        for(NSWindow* w in NSApplication.sharedApplication.windows) if(w.visible && surface(w.contentView)) no_controls(w.contentView);
        good(host.request_close());
    }}));
}
void lifetime() {
    gui::Host host;gui::Widget menu,item;int triggered=0;
    good(host.run({.opened=[&] {
        auto mount=take(host.mount_widgets([&](gui::WidgetBuilder& b)->core::Result<void> {
            auto root=take(b.vstack({},"root"));menu=take(b.menu(root,"menu",{}, {.triggered=[&](int){++triggered;}}));
            item=take(b.menu_item(menu,"remove",{.label="Remove",.has_child_items=false},[&]{good(host.unmount_widgets());}));return {};
        }));
        good(menu.open_at(10,10));good(item.activate());
        check(!menu.snapshot() && !triggered,"Menu trigger continued through a retired generation");
        take(host.mount_widgets([&](gui::WidgetBuilder& b)->core::Result<void> {
            auto root=take(b.vstack({},"root"));menu=take(b.menu(root,"menu"));
            item=take(b.menu_item(menu,"reopen",{.label="Reopen",.has_child_items=false},[&]{good(menu.open_at(200,100));}));return {};
        }));
        good(menu.open_at(10,10));good(item.activate());check(take(menu.snapshot()).opened && take(menu.snapshot()).bounds.x==200,"Old trigger closed a reopened popup");
        good(host.request_close());
    }}));
}
void scroll() {
    gui::Host host;int selected=-1;
    good(host.run({.opened=[&] {
        auto mount=take(host.mount_widgets([&](gui::WidgetBuilder& b)->core::Result<void> {
            auto root=take(b.vstack({},"root"));auto menu=take(b.menu(root,"menu",{.style=gui::MenuStyle::context,.auto_close_on_trigger=false},{.triggered=[&](int i){selected=i;}}));
            for(int i=0;i<150;++i) take(b.menu_item(menu,"row"+std::to_string(i),{.label="Entry "+std::to_string(i),.shortcut="",.show_icon_slot=false,.has_child_items=false}));return {};
        }));
        auto menu=take(mount.find("menu")),first=take(mount.find("row0")),last=take(mount.find("row149"));good(menu.open_at(10,10));
        check(!take(last.snapshot()).visible && take(menu.snapshot()).content_height>take(host.snapshot()).size.height,"Long menu did not retain offscreen content");
        auto r=take(menu.snapshot()).bounds;good(mount.dispatch_input({.kind=gui::WidgetInputKind::scroll,.x=r.x+20,.y=r.y+20,.scroll_y=1e6}));
        check(take(menu.snapshot()).scroll_offset>0 && take(last.snapshot()).visible && !take(first.snapshot()).visible,"Wheel scroll did not clamp and expose last entries");
        pointer(mount,gui::WidgetInputKind::pointer_down,take(last.snapshot()).bounds);pointer(mount,gui::WidgetInputKind::pointer_up,take(last.snapshot()).bounds);
        check(selected==149,"Scrolled row hit test lost the original entry index");
        key(mount,gui::WidgetKey::home);check(take(first.snapshot()).focused && take(first.snapshot()).visible && take(menu.snapshot()).scroll_offset==0,"Keyboard navigation did not reveal the first row");
        key(mount,gui::WidgetKey::end);check(take(last.snapshot()).visible && take(last.snapshot()).focused,"End did not scroll the focused row into view");
        (void)take(host.capture_canvas());good(host.request_close());
    }}));
}
void nested() {
    gui::Host host;gui::Widget outer,inner,more,button;
    good(host.run({.opened=[&] {
        auto m=take(host.mount_widgets([&](gui::WidgetBuilder& b)->core::Result<void> {
            auto root=take(b.vstack({},"root"));button=take(b.button(root,"opener",{.text="Opener"}));
            outer=take(b.menu(root,"outer"));more=take(b.menu_item(outer,"more",{.label="More"},[&]{auto r=take(more.snapshot()).bounds;good(more.set_menu_expanded(true));good(inner.open_for(more,r.width,0));}));
            inner=take(b.menu(more,"inner",{}, {.closed=[&]{good(more.set_menu_expanded(false));}}));
            take(b.menu_item(inner,"leaf",{.label="Leaf",.has_child_items=false}));return {};
        }));
        good(button.focus());good(outer.open_at(30,80));good(more.activate());
        check(take(inner.snapshot()).opened && take(outer.snapshot()).opened && take(more.snapshot()).input_blocked,"Owned submenu did not establish its input scope");
        key(m,gui::WidgetKey::escape);check(!take(inner.snapshot()).opened && take(outer.snapshot()).opened && take(more.snapshot()).focused,"Child close did not restore parent menu focus");
        good(more.activate());good(m.dispatch_input({.kind=gui::WidgetInputKind::pointer_down,.x=0,.y=0}));
        check(!take(inner.snapshot()).opened && !take(outer.snapshot()).opened,"One outside press did not dismiss the full popup branch");
        check(!inner.open_at(10,10),"Popup opened beneath a hidden owner");good(host.request_close());
    }}));
}
void context() {
    gui::Host host;Fixture f;
    good(host.run({.opened=[&] {
        f.build(host);good(f.button.set_context_menu(f.menu));
        check(take(f.button.snapshot()).has_context_menu,"Context binding was not recorded");
        pointer(f.mount,gui::WidgetInputKind::context_request,take(f.button.snapshot()).bounds);check(take(f.menu.snapshot()).opened,"Context request did not open the bound popup");
        good(f.menu.close_popup());check(!f.button.set_context_menu(f.first),"Non-popup context binding accepted");
        good(f.button.set_context_menu({}));pointer(f.mount,gui::WidgetInputKind::context_request,take(f.button.snapshot()).bounds);
        check(!take(f.menu.snapshot()).opened && !take(f.button.snapshot()).has_context_menu,"Removing a context binding left it active");good(host.request_close());
    }}));
}
void native_events() {
    gui::Host host;Fixture f;
    good(host.run({.opened=[&] {
        f.build(host);good(f.button.set_context_menu(f.menu));bool found=false;
        for(NSWindow* w in NSApplication.sharedApplication.windows) if(w.visible && surface(w.contentView)) {
            NSView* view=surface(w.contentView);no_controls(w.contentView);
            auto r=take(f.button.snapshot()).bounds;auto point=[view convertPoint:NSMakePoint(r.x+r.width/2,r.y+r.height/2) toView:nil];
            auto event=[NSEvent mouseEventWithType:NSEventTypeRightMouseDown location:point modifierFlags:0 timestamp:0 windowNumber:w.windowNumber context:nil eventNumber:0 clickCount:1 pressure:1];
            [view rightMouseDown:event];check(take(f.menu.snapshot()).opened,"Native right mouse event did not reach the C++ context binding");
            key(f.mount,gui::WidgetKey::down);check([[[view accessibilityFocusedUIElement] accessibilityIdentifier] isEqualToString:@"second"],"Virtual menu focus did not follow keyboard navigation");
            for(NSAccessibilityElement* peer in view.accessibilityChildren) if([peer.accessibilityRole isEqualToString:NSAccessibilityMenuRole]) {
                found=true;bool row=false;
                for(NSAccessibilityElement* child in peer.accessibilityChildren) if([child.accessibilityIdentifier isEqualToString:@"first"]) {
                    row=true;check([child.accessibilityRole isEqualToString:NSAccessibilityMenuItemRole],"Menu item AX role differs");
                    check([child accessibilityPerformPress],"Own menu item AX action missing");
                }
                check(row,"Open menu AX entries missing");
            }
            bool context_action=false;
            for(NSAccessibilityElement* peer in view.accessibilityChildren) if([peer.accessibilityIdentifier isEqualToString:@"button"]) {
                context_action=true;
                check([peer accessibilityPerformShowMenu] && take(f.menu.snapshot()).opened,"Virtual context-menu action did not use own popup state");
            }
            check(context_action,"Closing a menu did not restore the base accessibility tree");
            id<NSTextInputClient> client=(id<NSTextInputClient>)view;
            [client setMarkedText:@"ㅎ" selectedRange:NSMakeRange(1,0) replacementRange:NSMakeRange(NSNotFound,0)];
            auto escape=[NSEvent keyEventWithType:NSEventTypeKeyDown location:NSZeroPoint modifierFlags:0 timestamp:0 windowNumber:w.windowNumber context:nil characters:@"\x1b" charactersIgnoringModifiers:@"\x1b" isARepeat:NO keyCode:53];
            [view keyDown:escape];check(!take(f.menu.snapshot()).opened,"Stale IME preedit swallowed popup Escape");
        }
        check(found && f.clicks==1,"Virtual menu tree or native action bridge missing");good(host.request_close());
    }}));
}
}
int main(int argc,char** argv) {
    try {
        std::string_view name=argc>1?argv[1]:"lifecycle";
        if(name=="lifecycle") lifecycle();else if(name=="placement") placement();else if(name=="input") input();else if(name=="rendering") rendering();else if(name=="lifetime") lifetime();else if(name=="native_events") native_events();else if(name=="scroll") scroll();else if(name=="nested") nested();else if(name=="context") context();else throw std::runtime_error("Unknown menu test");
        std::cout<<"OWN_MENUS_OK "<<name<<'\n';
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
