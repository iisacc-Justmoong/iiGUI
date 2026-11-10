#include <iiGUI/NativeSliderView.h>
#include <iiGUI/WidgetState.h>
#include <cmath>
#include <memory>
#include <optional>
#include <stdexcept>
#include <vector>
namespace {
void check(bool b,const char* m){if(!b)throw std::runtime_error(m);}
template<class T>T take(ii::core::Result<T> r){if(!r)throw std::runtime_error(r.error().message);return std::move(*r);}
void good(ii::core::Result<void> r){if(!r)throw std::runtime_error(r.error().message);}
}
int main(){using namespace ii::gui;auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task t){t();});
 auto root=take(tree->content_host({},"root",{400,200}));std::optional<NativeSlider> slider;std::vector<double> values;
 SliderConfiguration c;c.minimum=0;c.maximum=10;c.step=1;c.value=5;c.width=100;c.palette.track={.3,.3,.3,1};c.palette.fill={.2,.4,.8,1};c.palette.thumb={1,1,1,1};
 auto held=std::make_shared<int>(7);std::weak_ptr<int> weak=held;
 good(root.set_content([&](WidgetBuilder& b)->ii::core::Result<void>{auto v=NativeSlider::create(b,{},"slider",c,[&,owned=held](double x){values.push_back(x);});
  if(!v)return std::unexpected(v.error());slider=*v;return {};}));held.reset();
 auto snapshot=take(slider->root().snapshot());check(snapshot.accessibility_semantics&&snapshot.accessibility_semantics->role==AccessibilityRole::slider,"Slider accessibility missing");
 auto drawing=take(tree->drawing_snapshot({400,200}));bool painted=!drawing.panels.empty();for(const auto& layer:drawing.overlays)painted|=!layer.panels.empty();check(painted,"Slider drawing missing");snapshot=take(slider->root().snapshot());
 good(tree->input({.kind=WidgetInputKind::pointer_down,.x=snapshot.bounds.x+snapshot.bounds.width-9,.y=snapshot.bounds.y+16}));
 good(tree->input({.kind=WidgetInputKind::pointer_up,.x=snapshot.bounds.x+snapshot.bounds.width-9,.y=snapshot.bounds.y+16}));
 if(take(slider->options()).value!=10||values.empty()||values.back()!=10)throw std::runtime_error("Slider pointer failed: value="+std::to_string(take(slider->options()).value)+" callbacks="+std::to_string(values.size())+" bounds="+std::to_string(snapshot.bounds.x)+","+std::to_string(snapshot.bounds.y)+","+std::to_string(snapshot.bounds.width)+","+std::to_string(snapshot.bounds.height));
 good(tree->input({.kind=WidgetInputKind::pointer_down,.x=snapshot.bounds.x+20,.y=snapshot.bounds.y+16,.device=PointerDevice::touch}));
 const auto canceled_value=take(slider->options()).value;
 good(tree->input({.kind=WidgetInputKind::pointer_cancel,.device=PointerDevice::touch}));
 good(tree->input({.kind=WidgetInputKind::pointer_move,.x=snapshot.bounds.x+80,.y=snapshot.bounds.y+16,.device=PointerDevice::touch}));
 check(take(slider->options()).value==canceled_value,"Canceled touch continued dragging the slider");
 good(tree->set_accessibility_value(snapshot.id,4.4));check(take(slider->options()).value==4&&values.back()==4,"Slider accessibility value failed");
 auto invalid=c;invalid.maximum=invalid.minimum;check(!slider->set_options(invalid)&&take(slider->options()).value==4,"Invalid state replaced current state");
 good(root.clear_content());check(weak.expired(),"Retired slider retained callback resources");
 check(!slider->set_options(c),"Retired slider accepted input");tree->close();}
