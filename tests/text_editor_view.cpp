#include <iiGUI/NativeTextEditorView.h>
#include "input_field_policy.h"
#include <iiGUI/WidgetState.h>
#include <memory>
#include <optional>
#include <stdexcept>
namespace {
void check(bool v,const char* m){if(!v)throw std::runtime_error(m);}
template<class T>T take(ii::core::Result<T> r){if(!r)throw std::runtime_error(r.error().message);return std::move(*r);}
void good(ii::core::Result<void> r){if(!r)throw std::runtime_error(r.error().message);}
struct Configuration {input_field_test::Configuration field;double minimum_height=1;};
struct Policy {using Field=ii::gui::BasicNativeInputField<input_field_test::Policy>;using Configuration=::Configuration;
 using Callbacks=input_field_test::Callbacks;using Clipboard=input_field_test::Clipboard;using Snapshot=input_field_test::Snapshot;
 static bool valid_configuration(const Configuration& v){return std::isfinite(v.minimum_height)&&v.minimum_height>=1&&v.minimum_height<=8192&&
  v.field.multiline&&!v.field.password&&!v.field.clear_button_visible&&input_field_test::Policy::valid_configuration(v.field);}};
}
int main(){using namespace ii::gui;using View=BasicNativeTextEditor<Policy>;
 auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task t){t();});auto root=take(tree->content_host({},"root",{300,150}));
 Configuration c;c.field.multiline=true;c.field.text="hello\nworld";c.field.height=40;c.minimum_height=72;
 auto capture=std::make_shared<int>(1);std::weak_ptr<int> weak=capture;std::optional<View> view;
 good(root.set_content([&](WidgetBuilder& b)->ii::core::Result<void>{input_field_test::Callbacks cb;cb.edited=[held=capture](std::string){};
  auto editor=View::create(b,{},"editor",c,std::move(cb));if(!editor)return std::unexpected(editor.error());view=*editor;return {};}));
 capture.reset();(void)take(tree->drawing_snapshot({300,150}));check(take(view->root().snapshot()).implicit_size.height==72&&c.field.height==40,"Editor minimum height lost or changed authored data");
 check(take(view->snapshot()).text=="hello\nworld","Editor lost its text");auto invalid=c;invalid.minimum_height=0;
 check(!view->set_options(invalid)&&take(view->snapshot()).text==c.field.text,"Invalid editor options replaced state");
 good(root.clear_content());check(weak.expired()&&!view->snapshot(),"Retired editor retained resources");tree->close();}
