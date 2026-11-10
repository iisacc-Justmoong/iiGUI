#include "input_field_policy.h"
#include <iiGUI/WidgetState.h>
#include <memory>
#include <optional>
#include <stdexcept>
namespace {void check(bool v,const char* m){if(!v)throw std::runtime_error(m);}
template<class T>T take(ii::core::Result<T> r){if(!r)throw std::runtime_error(r.error().message);return std::move(*r);}
void good(ii::core::Result<void> r){if(!r)throw std::runtime_error(r.error().message);}}
void callback_retirement(){using namespace ii::gui;using View=BasicNativeInputField<input_field_test::Policy>;
 auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task t){t();});auto root=take(tree->content_host({},"root",{300,60}));
 auto owner=std::make_shared<int>(1);std::weak_ptr<int> weak=owner;input_field_test::Configuration options;options.text="A😀Z";
 std::optional<View> view;int edits=0;good(root.set_content([&](WidgetBuilder& b)->ii::core::Result<void>{input_field_test::Callbacks callbacks;
  callbacks.edited=[&,keep=owner](std::string){++edits;(void)root.clear_content();};
  auto item=View::create(b,{},"input",options,std::move(callbacks));if(!item)return std::unexpected(item.error());view=*item;return {};}));
 owner.reset();check(take(view->snapshot()).text=="A😀Z","Input field lost initial text");good(view->clear_as_user());
 check(edits==1&&weak.expired(),"Retired input field retained callbacks");check(!view->snapshot(),"Retired input handle remained active");tree->close();}

namespace {
struct TrackedBoundary final:ii::gui::TextEditBoundary {
 std::shared_ptr<const ii::gui::TextEditBoundary> inner;std::shared_ptr<int> token;
 TrackedBoundary(std::shared_ptr<const ii::gui::TextEditBoundary> value,std::shared_ptr<int> marker):inner(std::move(value)),token(std::move(marker)){}
 std::size_t length()const noexcept override{return inner->length();}
 std::optional<std::size_t> byte_offset(std::size_t n)const override{return inner->byte_offset(n);}
 std::size_t previous_grapheme(std::size_t n)const noexcept override{return inner->previous_grapheme(n);}
 std::size_t next_grapheme(std::size_t n)const noexcept override{return inner->next_grapheme(n);}
 std::size_t previous_word(std::size_t n)const noexcept override{return inner->previous_word(n);}
 std::size_t next_word(std::size_t n)const noexcept override{return inner->next_word(n);}
 std::optional<ii::gui::TextEditRange> word_range(std::size_t n)const noexcept override{return inner->word_range(n);}
};
struct TrackingPolicy:input_field_test::Policy {
 inline static std::weak_ptr<int> last;
 template<class Index>static ii::gui::TextEditResult<std::shared_ptr<const ii::gui::TextEditBoundary>> boundary(std::string_view text){
  auto source=ii::gui::make_text_input_boundary(text);if(!source)return std::unexpected(source.error());
  auto marker=std::make_shared<int>(1);last=marker;return std::shared_ptr<const ii::gui::TextEditBoundary>(std::make_shared<TrackedBoundary>(*source,std::move(marker)));}
};
}
void retained_handle_releases_model(){using namespace ii::gui;using View=BasicNativeInputField<TrackingPolicy>;
 auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task t){t();});auto root=take(tree->content_host({},"root",{300,60}));
 input_field_test::Configuration input;input.text="Private input";std::optional<View> view;
 good(root.set_content([&](WidgetBuilder& b)->ii::core::Result<void>{auto field=View::create(b,{},"input",input);if(!field)return std::unexpected(field.error());view=*field;return {};}));
 auto weak=TrackingPolicy::last;check(!weak.expired(),"Text boundary was not retained while active");
 good(root.clear_content());check(weak.expired(),"Retired input handle retained text model resources");tree->close();}
int main(){callback_retirement();retained_handle_releases_model();}
