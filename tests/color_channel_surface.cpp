#include <iiGUI/ColorChannelSurface.h>
#include <iiGUI/ColorSurfaceModel.h>
#include <array>
#include <memory>
#include <stdexcept>
namespace {
using namespace ii::gui;
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(ColorSurfaceResult<T> value){if(!value)throw std::runtime_error(value.error().message);return std::move(*value);}
enum class Channel{red};
struct ChannelFrame{Channel channel=Channel::red;WidgetColor color{.5,0,0,1};double value=.5,minimum=0,maximum=1;std::array<WidgetColor,7> stops;std::uint64_t revision=0;};
class Source{public:double value=.5;mutable std::uint64_t version=0;mutable bool invalidate=false;
 ColorSurfaceResult<WidgetColor> color()const{return WidgetColor{value,0,0,1};}
 std::uint64_t revision()const noexcept{return version;}
 ColorSurfaceResult<ChannelFrame> channel_frame(Channel)const{ChannelFrame f;f.value=value;f.color={value,0,0,1};f.revision=version;
  for(std::size_t i=0;i<f.stops.size();++i)f.stops[i]={double(i)/6,0,0,1};if(invalidate)++version;return f;}
 ColorSurfaceResult<std::uint64_t> edit_channel(Channel,double next,std::uint64_t expected){if(expected!=version)return std::unexpected(ColorSurfaceError{ColorSurfaceErrorCode::stale,"stale"});value=next;return ++version;}};
struct Policy{using SurfaceSource=ColorSurfaceSource;using Source=::Source;using Channel=::Channel;using Kind=ColorSurfaceKind;
 using Point=ColorSurfacePoint;using Frame=ColorSurfaceFrame;using Error=ColorSurfaceError;using ErrorCode=ColorSurfaceErrorCode;
 using FrameResult=ColorSurfaceResult<Frame>;using EditResult=ColorSurfaceResult<std::uint64_t>;
 static bool valid_channel(Channel){return true;}static bool valid_frame(const ChannelFrame& frame){return frame.minimum<frame.maximum&&frame.value>=frame.minimum&&frame.value<=frame.maximum;}};
}
int main(){auto source=std::make_shared<Source>();ii::gui::BasicColorChannelSurface<Policy> surface(source,Channel::red);
 const auto frame=take(surface.frame(ColorSurfaceKind::spectrum,7,2));check(frame.rgba.size()==56&&frame.rgba[0]==0&&frame.rgba[12]==128&&frame.rgba[24]==255&&frame.rgba[3]==255&&frame.marker.x==.5,"Channel gradient changed");
 check(take(surface.edit(ColorSurfaceKind::spectrum,{1,.5},0))==1&&source->value==1,"Channel edit failed");
 check(!surface.edit(ColorSurfaceKind::spectrum,{0,.5},0)&&!surface.frame(ColorSurfaceKind::wheel,7,2),"Stale edit or invalid kind accepted");
 source->invalidate=true;check(!surface.frame(ColorSurfaceKind::spectrum,7,2),"Changing source revision produced an inconsistent frame");}
