#include <iiGUI/NativeIconView.h>
#include <iiGUI/WidgetState.h>
#include <cmath>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(ii::core::Result<T> value){if(!value){if(value.error().cause)std::rethrow_exception(value.error().cause);throw std::runtime_error(value.error().message);}return std::move(*value);}
void good(ii::core::Result<void> value){if(!value){if(value.error().cause)std::rethrow_exception(value.error().cause);throw std::runtime_error(value.error().message);}}
struct Source final:ii::gui::ButtonIconSource {
    std::shared_ptr<const ii::gui::RasterImage> one,two;
    mutable unsigned calls=0;mutable std::function<void()> on_resolve;std::uint64_t version=1;
    std::uint64_t revision()const noexcept override{return version;}
    ii::core::Result<std::shared_ptr<const ii::gui::RasterImage>> resolve(std::string_view name,double)const override {
        ++calls;if(auto action=std::exchange(on_resolve,{}))action();
        if(name=="one")return one;if(name=="two")return two;
        return ii::core::fail(ii::core::ErrorCode::not_found,"Unknown icon");
    }
};
std::vector<ii::gui::ImageRun> images(const ii::gui::CanvasFrame& frame){
    auto result=frame.images;for(const auto& overlay:frame.overlays)result.insert(result.end(),overlay.images.begin(),overlay.images.end());return result;
}
void retire_during_resolution(bool destroy_handle){
    using namespace ii::gui;
    auto source=std::make_shared<Source>();
    source->one=take(rgba_image(2,2,std::vector<std::uint8_t>(16,255)));
    source->two=take(rgba_image(2,2,std::vector<std::uint8_t>(16,128)));
    IconViewInput input{.name="one",.source=source};
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"retiring-host",{80,80}));std::optional<IconView> icon;
    good(host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto created=IconView::create(builder,{},"icon",input);if(!created)return std::unexpected(created.error());icon=*created;return {};
    }));
    auto next=input;next.name="two";source->on_resolve=[&]{good(host.clear_content());if(destroy_handle)icon.reset();};
    good(icon->set_options(next));
    std::weak_ptr<Source> weak=source;input.source.reset();next.source.reset();source.reset();
    require(weak.expired(),"An in-flight icon resolution retained its source after retirement");
    require(destroy_handle?!icon:!icon->options(),"An icon retired during resolution remained active");tree->close();
}
}
int main(){
    using namespace ii::gui;
    retire_during_resolution(false);retire_during_resolution(true);
    auto source=std::make_shared<Source>();
    source->one=take(rgba_image(2,2,std::vector<std::uint8_t>(16,255)));
    source->two=take(rgba_image(2,2,std::vector<std::uint8_t>(16,128)));
    IconViewInput input{.name="one",.size=18,.source=source,.accessibility_name="First"};
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{120,120}));std::optional<IconView> icon;
    good(host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto row=builder.hstack({},"row",{.alignment=StackAlignment::center});if(!row)return std::unexpected(row.error());
        auto value=IconView::create(builder,*row,"icon",input);if(!value)return std::unexpected(value.error());icon=*value;return {};
    }));
    auto root=icon->root();auto first=images(take(tree->drawing_snapshot({120,120})));
    if(first.size()!=1||first[0].image!=source->one||first[0].bounds.width!=18||take(root.snapshot()).accessibility_name!="First")
        throw std::runtime_error("Icon presentation mismatch: images="+std::to_string(first.size())+
            ", width="+(first.empty()?std::string("none"):std::to_string(first[0].bounds.width))+
            ", name="+take(root.snapshot()).accessibility_name);
    const auto calls=source->calls;good(icon->set_options(input));require(source->calls==calls,"Unchanged icon resolved its source again");
    auto invalid=input;invalid.name=" ";require(!icon->set_options(invalid),"Blank icon name was accepted");
    invalid=input;invalid.size=-1;require(!icon->set_options(invalid),"Invalid icon size was accepted");
    invalid=input;invalid.revision=-1;require(!icon->set_options(invalid),"Negative icon revision was accepted");
    invalid=input;invalid.name="missing";require(!icon->set_options(invalid),"Missing icon was accepted");
    require(images(take(tree->drawing_snapshot({120,120})))[0].image==source->one,"Rejected icon replaced its image");
    auto older=input;older.name="two";older.size=24;older.accessibility_name="Older";
    source->on_resolve=[&]{good(icon->set_options(input));};
    good(icon->set_options(older));
    require(take(root.snapshot()).accessibility_name=="First"&&images(take(tree->drawing_snapshot({120,120})))[0].image==source->one,
            "A reentrant request for the current icon did not supersede the older preparation");
    good(icon->set_options(older));require(take(root.snapshot()).accessibility_name=="Older"&&images(take(tree->drawing_snapshot({120,120})))[0].image==source->two,
            "A valid icon update did not recover after reentry");
    bool wrong=false;std::thread worker([&]{auto result=icon->set_options(input);wrong=!result&&result.error().code==ii::core::ErrorCode::wrong_executor;});worker.join();
    require(wrong,"Icon accepted an update from another thread");
    std::weak_ptr<Source> weak=source;good(host.clear_content());input.source.reset();older.source.reset();invalid.source.reset();source.reset();
    require(weak.expired(),"Retired icon retained its source");require(!icon->set_options(input),"Retired icon accepted an update");tree->close();
}
