#include <iiGUI/NativeColorSurfaceView.h>
#include <iiGUI/WidgetState.h>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
namespace {void check(bool v,const char* m){if(!v)throw std::runtime_error(m);}
template<class T>T take(ii::core::Result<T> r){if(!r)throw std::runtime_error(r.error().message);return std::move(*r);}
void good(ii::core::Result<void> r){if(!r)throw std::runtime_error(r.error().message);}
struct Source final:ii::gui::ColorSurfaceSource {mutable unsigned reads=0;mutable std::function<void()> on_revision,on_frame;std::uint64_t rev=0;
 std::uint64_t revision()const noexcept override{auto cb=std::exchange(on_revision,{});if(cb)cb();return rev;}
 ii::gui::ColorSurfaceResult<ii::gui::ColorSurfaceFrame> frame(ii::gui::ColorSurfaceKind,std::uint32_t w,std::uint32_t h)const override{
  ++reads;auto cb=std::exchange(on_frame,{});if(cb)cb();ii::gui::ColorSurfaceFrame f;f.color={1,0,0,1};f.marker={.5,.5};f.revision=rev;f.width=w;f.height=h;f.rgba.assign(std::size_t(w)*h*4,255);return f;}
 ii::gui::ColorSurfaceResult<std::uint64_t> edit(ii::gui::ColorSurfaceKind,ii::gui::ColorSurfacePoint,std::uint64_t)override{return ++rev;}
};}
void retention(){using namespace ii::gui;auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task t){t();});auto host=take(tree->content_host({},"host",{200,200}));
 auto source=std::make_shared<Source>();std::weak_ptr<Source> weak_source=source;ColorSurfaceInput input;input.source=source;input.configuration.width=input.configuration.height=64;
 auto owner=std::make_shared<int>(1);std::weak_ptr<int> weak_owner=owner;std::optional<NativeColorSurface> surface;
 good(host.set_content([&](WidgetBuilder& b)->ii::core::Result<void>{auto v=NativeColorSurface::create(b,{},"surface",input,[keep=owner](WidgetColor){});if(!v)return std::unexpected(v.error());surface=*v;return {};}));
 check(source->reads==1,"Initial color frame missing");good(surface->set_options(input));check(source->reads==1,"Unchanged color surface reread its source");
 (void)take(tree->drawing_snapshot({200,200}));input.source.reset();source.reset();owner.reset();check(!weak_source.expired()&&!weak_owner.expired(),"Live surface lost resources");
 good(host.clear_content());check(weak_source.expired()&&weak_owner.expired()&&!surface->options(),"Retired surface retained resources");tree->close();}

void revision_retirement(){using namespace ii::gui;auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task t){t();});
 auto host=take(tree->content_host({},"host",{100,100}));auto source=std::make_shared<Source>();ColorSurfaceInput input;input.source=source;
 input.configuration.width=input.configuration.height=64;std::optional<NativeColorSurface> surface;
 good(host.set_content([&](WidgetBuilder& b)->ii::core::Result<void>{auto v=NativeColorSurface::create(b,{},"surface",input);if(!v)return std::unexpected(v.error());surface=*v;return {};}));
 source->on_revision=[&]{(void)host.clear_content();};auto result=surface->set_options(input);
 check(!result&&result.error().code==ii::core::ErrorCode::closed,"Revision callback retirement did not stop the update");tree->close();}
void frame_retirement(){using namespace ii::gui;auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task t){t();});
 auto host=take(tree->content_host({},"host",{100,100}));auto source=std::make_shared<Source>();ColorSurfaceInput input;input.source=source;
 input.configuration.width=input.configuration.height=64;std::optional<NativeColorSurface> surface;
 good(host.set_content([&](WidgetBuilder& b)->ii::core::Result<void>{auto v=NativeColorSurface::create(b,{},"surface",input);if(!v)return std::unexpected(v.error());surface=*v;return {};}));
 source->on_frame=[&]{(void)host.clear_content();};++input.source_revision;auto result=surface->set_options(input);
 check(!result&&result.error().code==ii::core::ErrorCode::closed,"Frame callback retirement did not stop the update");tree->close();}
void canceled_drag(){using namespace ii::gui;auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task t){t();});
 auto host=take(tree->content_host({},"host",{100,100}));auto source=std::make_shared<Source>();ColorSurfaceInput input;input.source=source;
 input.configuration.width=input.configuration.height=64;
 good(host.set_content([&](WidgetBuilder& b)->ii::core::Result<void>{auto v=NativeColorSurface::create(b,{},"surface",input);if(!v)return std::unexpected(v.error());return {};}));
 (void)take(tree->drawing_snapshot({100,100}));
 good(tree->input({.kind=WidgetInputKind::pointer_down,.x=10,.y=10,.device=PointerDevice::touch}));
 const auto revision=source->rev;check(revision>0,"Color touch did not start editing");
 good(tree->input({.kind=WidgetInputKind::pointer_cancel,.device=PointerDevice::touch}));
 good(tree->input({.kind=WidgetInputKind::pointer_move,.x=30,.y=30,.device=PointerDevice::touch}));
 check(source->rev==revision,"Canceled color touch continued editing");tree->close();}
int main(){retention();revision_retirement();frame_retirement();canceled_drag();}
