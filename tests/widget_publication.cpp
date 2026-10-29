#include <iiGUI/WidgetState.h>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
namespace gui=ii::gui;namespace core=ii::core;
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(core::Result<T> value){if(!value)throw std::runtime_error(value.error().message);return std::move(*value);}
void good(core::Result<void> value){if(!value)throw std::runtime_error(value.error().message);}
auto state(){return std::make_shared<gui::detail::WidgetState>([](ii::runtime::Task task){task();});}

void root_and_nested(){
    auto owner=state();std::vector<std::string> events;
    good(owner->construct([&](gui::WidgetBuilder& builder)->core::Result<void>{
        auto root=take(builder.vstack({},"root"));good(builder.on_publish([&]{events.push_back("root");owner->commit_publication();}));
        good(builder.on_publish([&]{events.push_back("next");}));
        auto slot=take(builder.content_host(root,"slot",{}));
        good(slot.set_content([&](gui::WidgetBuilder& child)->core::Result<void>{
            take(child.label({},"label",{.text="Child"}));return child.on_publish([&]{events.push_back("child");});
        }));
        check(events.empty(),"Nested candidate published before root commit");return {};
    }));
    check(events.empty(),"Constructing the root published its effects");
    owner->commit_publication();
    check(events==std::vector<std::string>({"root","next","child"}),"Reentry changed parent and child publication order");
    owner->commit_publication();check(events.size()==3,"Publication was delivered more than once");owner->close();
}
void rollback(){
    auto owner=state();auto host=take(owner->content_host({},"host",{}));std::vector<std::string> events;
    good(host.set_content([&](gui::WidgetBuilder& b)->core::Result<void>{take(b.label({},"old",{.text="Old"}));return b.on_publish([&]{events.push_back("old");});}));
    auto old=take(host.content_item());check(events==std::vector<std::string>{"old"},"Initial content was not published");
    auto failed=host.set_content([&](gui::WidgetBuilder& b)->core::Result<void>{
        auto root=take(b.vstack({},"candidate"));good(b.on_publish([&]{events.push_back("candidate");}));
        auto nested=take(b.content_host(root,"nested",{}));
        good(nested.set_content([&](gui::WidgetBuilder& c)->core::Result<void>{take(c.label({},"leaf",{}));return c.on_publish([&]{events.push_back("leaf");});}));
        check(events.size()==1,"Nested unpublished tree changed live state");
        return core::fail(core::ErrorCode::failed,"Rejected candidate");
    });
    check(!failed&&old.snapshot()&&events.size()==1,"Failed candidate published or retired the old content");
    failed=host.set_content([&](gui::WidgetBuilder& b)->core::Result<void>{take(b.label({},"new",{}));return b.on_publish([&]{events.push_back("uncommitted");});},
        []()->core::Result<void>{return core::fail(core::ErrorCode::failed,"Commit failure");});
    check(!failed&&old.snapshot()&&events.size()==1,"Failed commit published candidate effects");owner->close();
}
void keyed(){
    auto owner=state();auto host=take(owner->content_host({},"host",{}));std::vector<std::string> events;
    const auto row=[&](std::string key,bool fail=false){return gui::KeyedContentItem{key,[&,key,fail](gui::WidgetBuilder& b)->core::Result<void>{
        take(b.label({},"row",{.text=key}));good(b.on_publish([&,key]{events.push_back(key);}));
        if(fail)return core::fail(core::ErrorCode::failed,"Row failed");return {};
    }};};
    good(host.set_keyed_content({row("a"),row("b")}));
    check(events==std::vector<std::string>({"a","b"}),"Initial keyed rows did not publish once");
    auto old=take(host.keyed_content_item("a"));auto failed=host.set_keyed_content({row("a"),row("c"),row("bad",true)});
    check(!failed&&old.snapshot()&&events.size()==2,"Failed keyed replacement published candidate rows");
    good(host.set_keyed_content({row("a"),row("c")}));
    check(old.snapshot()&&events==std::vector<std::string>({"a","b","c"}),"Retained or new keyed row published incorrectly");owner->close();
}
void retirement(){
    auto owner=state();auto host=take(owner->content_host({},"host",{}));int count=0;
    auto result=host.set_content([&](gui::WidgetBuilder& b)->core::Result<void>{
        take(b.label({},"root",{}));good(b.on_publish([&]{++count;owner->close();}));
        return b.on_publish([&]{++count;});
    });
    check(!result&&count==1,"Retirement during publication delivered later effects");
}
void popup(){
    auto owner=state();auto host=take(owner->content_host({},"host",{}));gui::Widget popup;int count=0;bool fail=true;
    good(host.set_content([&](gui::WidgetBuilder& b)->core::Result<void>{
        auto root=take(b.vstack({},"root"));popup=take(b.popover(root,"popup",{.width=120,.height=100}));return {};
    }));
    good(popup.set_popup_content([&](gui::WidgetBuilder& b)->core::Result<void>{
        take(b.label({},"body",{.text="Popup"}));good(b.on_publish([&]{++count;}));
        if(fail)return core::fail(core::ErrorCode::failed,"Popup candidate failed");return {};
    }));
    check(!popup.open_at(0,0)&&count==0,"Failed popup candidate published");
    fail=false;good(popup.open_at(0,0));check(count==1,"Popup publication was not delivered after open");
    good(popup.close_popup());good(popup.open_at(0,0));check(count==2,"Popup reopening did not publish its fresh generation");owner->close();
}
}
int main(){root_and_nested();rollback();keyed();retirement();popup();}
