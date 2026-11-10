#include <iiGUI/TableViewGroup.h>
#include <iiGUI/WidgetState.h>
#include <iiSDKCore/Core/Property/Observable.h>
#include <memory>
#include <stdexcept>
#include <thread>
#include <unordered_map>
namespace {
using namespace ii::gui;
void require(bool v,const char* m){if(!v)throw std::runtime_error(m);}
template<class T>T take(ii::core::Result<T> r){if(!r)throw std::runtime_error(r.error().message);return std::move(*r);}
void good(ii::core::Result<void> r){if(!r)throw std::runtime_error(r.error().message);}
struct Value{std::string column_key,text;bool enabled=true;friend bool operator==(const Value&,const Value&)=default;};
struct Context{
    std::shared_ptr<ii::core::Observable<Value>> value;
    const Value& get()const{if(!value)throw std::logic_error("closed");return value->get();}
    auto observe(std::function<void(const Value&,std::uint64_t)> f)const{return value->observe(std::move(f));}
};
struct Policy{
    using Snapshot=Value;using Context=::Context;inline static int creations=0;
    using Factory=std::function<ii::core::Result<void>(WidgetBuilder&,std::shared_ptr<const Context>)>;
    struct Item{std::string key;Snapshot value;Rect frame;};
    static std::shared_ptr<Context> make_context(Snapshot value){++creations;return std::make_shared<Context>(std::make_shared<ii::core::Observable<Snapshot>>(std::move(value)));}
    static void publish(Context& context,Snapshot value){context.value->set(std::move(value));}
    static void close(Context& context){context.value.reset();}
};
}
int main(){
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    auto outer=take(tree->content_host({},"outer",{200,80}));Widget host;
    good(outer.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto stack=builder.absolute_stack({},"stack",{200,80});if(!stack)return std::unexpected(stack.error());
        auto content=builder.content_host(*stack,"host",{200,80});if(!content)return std::unexpected(content.error());host=*content;return {};
    }));
    int builds=0;std::shared_ptr<const Context> observed;
    Policy::Factory factory=[&](WidgetBuilder& b,std::shared_ptr<const Context> c)->ii::core::Result<void>{
        ++builds;observed=c;auto label=b.label({},"text",{.text=c->get().text});if(!label)return std::unexpected(label.error());return {};
    };
    auto group=std::make_unique<BasicNativeTableViewGroup<Policy>>(host,factory,std::map<std::string,Policy::Factory,std::less<>>{});std::vector<Policy::Item> values{{"one",{"name","A",true},{0,0,100,20}}};
    good(group->update(values,{200,80}));auto first=observed;require(builds==1&&first->get().text=="A","initial view missing");
    const auto creations=Policy::creations;bool wrong_thread=false;
    std::thread worker([&]{auto foreign=values;foreign[0].key="foreign";auto result=group->update(foreign,{200,80});
        wrong_thread=!result&&result.error().code==ii::core::ErrorCode::wrong_executor;});worker.join();
    require(wrong_thread&&Policy::creations==creations&&first->get().text=="A","foreign thread reached context preparation");
    values[0].value.text="B";good(group->update(values,{200,80}));require(builds==1&&observed==first&&first->get().text=="B","retained context was rebuilt");
    auto duplicate=values;duplicate.push_back(values.front());require(!group->update(duplicate,{200,80})&&first->get().text=="B","duplicate key changed committed view");
    good(group->update({}, {200,80}));bool closed=false;try{(void)first->get();}catch(const std::logic_error&){closed=true;}require(closed,"removed context was not closed");
    good(group->update(values,{200,80}));require(builds==2&&observed!=first,"retired context was reused");
    auto current=observed;bool retired_during_notification=false;
    auto subscription=current->observe([&](const Value& value,std::uint64_t){
        if(value.text=="Retire"){group.reset();retired_during_notification=true;}
    });
    values[0].value.text="Retire";auto result=group->update(values,{200,80});
    require(retired_during_notification&&!group&&bool(result),"reentrant group retirement did not safely stop the update");
    good(outer.clear_content());closed=false;try{(void)current->get();}catch(const std::logic_error&){closed=true;}
    require(closed,"retired view context survived its owner");tree->close();
}
