#include <iiGUI/CompiledViewSource.h>
#include <iiGUI/WidgetState.h>
#include <memory>
#include <stdexcept>
namespace {
void check(bool v,const char* m){if(!v)throw std::runtime_error(m);}
template<class T>T take(ii::core::Result<T> r){if(!r)throw std::runtime_error(r.error().message);return std::move(*r);}
void good(ii::core::Result<void> r){if(!r)throw std::runtime_error(r.error().message);}
struct Generated final {static ii::core::Result<void> build(ii::gui::WidgetBuilder& b){
    auto root=b.vstack({},"page");if(!root)return std::unexpected(root.error());
    auto field=b.input_field(*root,"draft",{.text="original"});if(!field)return std::unexpected(field.error());return {};
}};
}
int main(){
    using namespace ii::gui;
    bool rejected=false;try{(void)compiled_view_source(WidgetFactory{});}catch(const std::invalid_argument&){rejected=true;}
    check(rejected,"empty factory accepted");
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{300,200}));auto source=compiled_view_source<Generated>();
    NativeView view,other;good(host.set_content([&](WidgetBuilder& b)->ii::core::Result<void>{
        auto row=b.hstack({},"row");if(!row)return std::unexpected(row.error());
        auto a=NativeView::create(b,*row,"a",{.source=source,.width=100,.height=80});if(!a)return std::unexpected(a.error());view=*a;
        auto c=NativeView::create(b,*row,"b",{.source=source,.width=100,.height=80});if(!c)return std::unexpected(c.error());other=*c;return {};
    }));
    auto first=take(view.content_item("draft")),second=take(other.content_item("draft"));good(first.set_text("edited"));
    check(take(second.snapshot()).text=="original","factory activations shared mutable state");
    auto failure=compiled_view_source([](WidgetBuilder&)->ii::core::Result<void>{throw std::runtime_error("failure");});
    auto result=view.set_source(failure);check(!result&&result.error().cause&&take(first.snapshot()).text=="edited","exception displaced current view");
    std::weak_ptr<NativeViewSource> weak=source;source.reset();good(host.clear_content());
    check(weak.expired()&&!first.snapshot()&&!second.snapshot(),"retirement retained the source or generation");tree->close();
}
