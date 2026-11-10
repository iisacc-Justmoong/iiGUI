#include <iiGUI/NativePageRouterView.h>
#include <iiGUI/WidgetState.h>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
namespace {
using namespace ii::gui;using ii::core::Result;
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(Result<T> value){if(!value)throw std::runtime_error(value.error().message);return std::move(*value);}
void good(Result<void> value){if(!value)throw std::runtime_error(value.error().message);}
struct PageBase{virtual ~PageBase()=default;};struct Entry{std::shared_ptr<const PageBase> page;};
struct Router;
struct Context{std::weak_ptr<Router> target_;std::weak_ptr<const void> owner_;
    void set_target(const std::shared_ptr<Router>& value){target_=value;owner_.reset();}
    void bind_target(const std::shared_ptr<Router>& value,const std::shared_ptr<const void>& owner){target_=value;owner_=owner;}
    void unbind_target(const std::shared_ptr<const void>& owner){if(owner_.lock()==owner){target_.reset();owner_.reset();}}
};
struct Page:PageBase{virtual Result<void> build(WidgetBuilder&,const Entry&,const std::shared_ptr<Context>&)const=0;};
struct Presenter{virtual ~Presenter()=default;virtual Result<void> present(const Entry&,std::function<Result<bool>()>)=0;};
struct Router{std::weak_ptr<Presenter> presenter;
    Result<bool> synchronize(const std::shared_ptr<Presenter>&){return false;}
    Result<void> set_presenter(const std::shared_ptr<Presenter>& value){presenter=value;return {};}
    Result<void> navigate(Entry entry){auto p=presenter.lock();if(!p)return ii::core::fail(ii::core::ErrorCode::closed,"Missing presenter");
        return p->present(entry,[]()->Result<bool>{return true;});}
};
struct TestPage final:Page{Result<void> build(WidgetBuilder& builder,const Entry&,const std::shared_ptr<Context>&)const override{
    auto label=builder.label({},"page",{.text="Page"});if(!label)return std::unexpected(label.error());return {};
}};
struct Policy{using Page=::Page;using Entry=::Entry;using Context=::Context;using Presenter=::Presenter;using Router=::Router;};
}
int main(){using Host=ii::gui::BasicNativePageRouter<Policy>;
    auto tree=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});
    auto outer=take(tree->content_host({},"outer",{300,150}));auto router=std::make_shared<Router>();auto context=std::make_shared<Context>();std::optional<Host> host;
    good(outer.set_content([&](auto& builder)->Result<void>{auto result=Host::create(builder,{},"router",router,context);if(!result)return std::unexpected(result.error());host=*result;return {};}));
    require(!context->target_.expired(),"Published page router did not bind its context");
    good(router->navigate({std::make_shared<TestPage>()}));auto page=take(host->root().content_item("page"));
    require(take(page.snapshot()).text=="Page","Page content was not published");
    good(outer.clear_content());require(context->target_.expired()&&router->presenter.expired(),"Retired page router retained its binding");
    require(!host->root().snapshot(),"Retired page host stayed active");tree->close();
}
