#include "FontDatabase.h"
#include "WidgetSurface.h"
#include "NativeCanvas.h"
#include "DesktopCommon.h"
#include "NativeDesktop_p.hpp"
#include "XInputPenState.h"
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <cctype>
#include <X11/keysym.h>
#include <X11/extensions/XInput2.h>
#include <algorithm>
#include <clocale>
#include <cstring>
#include <map>

namespace ii::gui::detail {
namespace {
WidgetKey key(KeySym symbol){switch(symbol){case XK_Tab:case XK_ISO_Left_Tab:return WidgetKey::tab;case XK_Return:case XK_KP_Enter:return WidgetKey::enter;case XK_Escape:return WidgetKey::escape;case XK_space:return WidgetKey::space;case XK_Left:return WidgetKey::left;case XK_Right:return WidgetKey::right;case XK_Up:return WidgetKey::up;case XK_Down:return WidgetKey::down;case XK_Home:return WidgetKey::home;case XK_End:return WidgetKey::end;case XK_BackSpace:return WidgetKey::backspace;case XK_Delete:return WidgetKey::delete_forward;case XK_Page_Up:return WidgetKey::page_up;case XK_Page_Down:return WidgetKey::page_down;case XK_Insert:return WidgetKey::insert;default:return WidgetKey::character;}}
WidgetInput input(WidgetInputKind kind,unsigned modifiers,double time){return {.kind=kind,.shift=(modifiers&ShiftMask)!=0,.control=(modifiers&ControlMask)!=0,.alt=(modifiers&Mod1Mask)!=0,.meta=(modifiers&Mod4Mask)!=0,.timestamp=time/1000};}
}
struct WidgetSurface::Impl final {
    Display* display;Window parent;WidgetSurfaceCallbacks callbacks;std::function<double()> scale;NativeCanvas canvas;
    Window drag_source=0;double drag_x=0,drag_y=0;bool drag_supported=false;
    bool dirty=true,frame_paced=false,xi=false;int xi_opcode=0;XIM method=nullptr;XIC context=nullptr;
    struct Axis final {
        int pressure=-1,tilt_x=-1,tilt_y=-1,rotation=-1;
        double pressure_min=0,pressure_max=1;
        bool pen=false,eraser=false;
        XInputPenState sensors;
    };
    std::map<int,Axis> axes;
    Impl(const X11WidgetParent& native,WidgetSurfaceCallbacks value):display(static_cast<Display*>(native.display)),parent(native.window),callbacks(std::move(value)),scale(native.scale),canvas(reinterpret_cast<void*>(parent),display,Acceleration::prefer_hardware,callbacks.renderer,callbacks.presentations){
        XWindowAttributes attributes{};XGetWindowAttributes(display,parent,&attributes);
        XSelectInput(display,parent,attributes.your_event_mask|ExposureMask|KeyPressMask|KeyReleaseMask|FocusChangeMask|ButtonPressMask|ButtonReleaseMask|PointerMotionMask|PropertyChangeMask);
        int event=0,error=0;if(XQueryExtension(display,"XInputExtension",&xi_opcode,&event,&error)){int major=2,minor=2;if(XIQueryVersion(display,&major,&minor)==Success){unsigned char mask[XIMaskLen(XI_TouchEnd)]{};for(int code:{XI_ButtonPress,XI_ButtonRelease,XI_Motion,XI_TouchBegin,XI_TouchUpdate,XI_TouchEnd})XISetMask(mask,code);XIEventMask selection{XIAllMasterDevices,int(sizeof(mask)),mask};xi=XISelectEvents(display,parent,&selection,1)==Success;}}
        std::setlocale(LC_CTYPE,"");XSetLocaleModifiers("");method=XOpenIM(display,nullptr,nullptr,nullptr);
        if(method){context=XCreateIC(method,XNInputStyle,XIMPreeditPosition|XIMStatusNothing,XNClientWindow,parent,XNFocusWindow,parent,nullptr);if(!context)context=XCreateIC(method,XNInputStyle,XIMPreeditNothing|XIMStatusNothing,XNClientWindow,parent,XNFocusWindow,parent,nullptr);}
        const unsigned long version=5;XChangeProperty(display,parent,XInternAtom(display,"XdndAware",False),XA_ATOM,32,PropModeReplace,reinterpret_cast<const unsigned char*>(&version),1);
    }
    ~Impl(){if(context)XDestroyIC(context);if(method)XCloseIM(method);}
    double dpi()const{return scale?scale():1;}
    Size size()const{XWindowAttributes value{};if(!XGetWindowAttributes(display,parent,&value))return {};return {value.width/dpi(),value.height/dpi()};}
    void flush(){if(!dirty)return;auto extent=size();if(extent.width<=0||extent.height<=0)return;canvas.draw(callbacks.frame(extent),dpi(),extent.height);dirty=false;}
    void invalidate(){dirty=true;if(!frame_paced){XEvent event{};event.xexpose.type=Expose;event.xexpose.display=display;event.xexpose.window=parent;XSendEvent(display,parent,False,ExposureMask,&event);XFlush(display);}}
    void send(WidgetInput value){callbacks.input(std::move(value));invalidate();caret();}
    std::optional<WidgetSnapshot> editor()const{for(auto& value:callbacks.accessibility())if(value.focused&&value.text_input)return value;return {};}
    void caret(){if(!context)return;auto value=editor();if(!value)return;auto r=value->caret_bounds;XPoint point{short(std::clamp(r.x*dpi(),-32768.,32767.)),short(std::clamp((r.y+r.height)*dpi(),-32768.,32767.))};auto list=XVaCreateNestedList(0,XNSpotLocation,&point,nullptr);if(list){XSetICValues(context,XNPreeditAttributes,list,nullptr);XFree(list);}}
    Axis& device(int id) {
        if(auto found=axes.find(id);found!=axes.end()) return found->second;
        Axis axis;
        XInputPenUpdate initial;
        int count=0;
        auto* devices=XIQueryDevice(display,id,&count);
        if(devices&&count) {
            std::string name=devices[0].name?devices[0].name:"";
            std::transform(name.begin(),name.end(),name.begin(),[](unsigned char c){return char(std::tolower(c));});
            axis.eraser=name.find("eraser")!=name.npos;
            for(int i=0;i<devices[0].num_classes;++i) {
                auto* base=devices[0].classes[i];
                if(base->type!=XIValuatorClass) continue;
                auto* info=reinterpret_cast<XIValuatorClassInfo*>(base);
                char* label=info->label?XGetAtomName(display,info->label):nullptr;
                const std::string_view text=label?label:"";
                if(text=="Abs Pressure") {
                    axis.pressure=info->number;axis.pressure_min=info->min;axis.pressure_max=info->max;axis.pen=true;
                    if(std::isfinite(info->value) && std::isfinite(info->min) && std::isfinite(info->max) && info->max>info->min)
                        initial.pressure=std::clamp((info->value-info->min)/(info->max-info->min),0.,1.);
                } else if(text=="Abs Tilt X") {
                    axis.tilt_x=info->number;
                    if(std::isfinite(info->value)) initial.tilt_x=std::clamp(info->value,-90.,90.);
                } else if(text=="Abs Tilt Y") {
                    axis.tilt_y=info->number;
                    if(std::isfinite(info->value)) initial.tilt_y=std::clamp(info->value,-90.,90.);
                } else if(text=="Abs Rotation") {
                    axis.rotation=info->number;
                    if(std::isfinite(info->value)) initial.rotation=info->value;
                }
                if(label) XFree(label);
            }
        }
        if(devices) XIFreeDeviceInfo(devices);
        (void)axis.sensors.merge(initial);
        return axes.emplace(id,std::move(axis)).first->second;
    }
    bool extended(XGenericEventCookie& cookie){if(!xi||cookie.extension!=xi_opcode||!XGetEventData(display,&cookie))return false;struct Release final{Display* display;XGenericEventCookie* cookie;~Release(){XFreeEventData(display,cookie);}}release{display,&cookie};const auto type=cookie.evtype;
        if(type!=XI_ButtonPress&&type!=XI_ButtonRelease&&type!=XI_Motion&&type!=XI_TouchBegin&&type!=XI_TouchUpdate&&type!=XI_TouchEnd)return false;auto* native=static_cast<XIDeviceEvent*>(cookie.data);if(native->event!=parent)return false;
        const bool touch=type==XI_TouchBegin||type==XI_TouchUpdate||type==XI_TouchEnd,down=type==XI_ButtonPress||type==XI_TouchBegin,up=type==XI_ButtonRelease||type==XI_TouchEnd;
        if(!touch&&native->detail>=4&&native->detail<=7){if(down){auto value=input(WidgetInputKind::scroll,native->mods.effective,native->time);value.x=native->event_x/dpi();value.y=native->event_y/dpi();if(native->detail<6)value.scroll_y=native->detail==4?48:-48;else value.scroll_x=native->detail==6?48:-48;send(std::move(value));}return true;}
        auto value=input(down?WidgetInputKind::pointer_down:up?WidgetInputKind::pointer_up:WidgetInputKind::pointer_move,native->mods.effective,native->time);value.x=native->event_x/dpi();value.y=native->event_y/dpi();value.device_id=native->sourceid;
        auto& axis=device(native->sourceid);
        value.device=touch?PointerDevice::touch:axis.eraser?PointerDevice::eraser:axis.pen?PointerDevice::pen:PointerDevice::mouse;
        const bool pen=!touch&&(axis.pen||axis.eraser);
        XInputPenUpdate update;
        const double* values=native->valuators.values;
        for(int number=0;number<native->valuators.mask_len*8;++number) {
            if(!XIMaskIsSet(native->valuators.mask,number)) continue;
            const auto v=*values++;
            if(number==axis.pressure&&axis.pressure_max>axis.pressure_min) {
                const auto pressure=std::isfinite(v)?std::clamp((v-axis.pressure_min)/(axis.pressure_max-axis.pressure_min),0.,1.):v;
                if(pen) update.pressure=pressure;else value.pressure=pressure;
            } else if(number==axis.tilt_x) {
                const auto tilt=std::isfinite(v)?std::clamp(v,-90.,90.):v;
                if(pen) update.tilt_x=tilt;else value.tilt_x=tilt;
            } else if(number==axis.tilt_y) {
                const auto tilt=std::isfinite(v)?std::clamp(v,-90.,90.):v;
                if(pen) update.tilt_y=tilt;else value.tilt_y=tilt;
            } else if(number==axis.rotation) {
                if(pen) update.rotation=v;else value.rotation=v;
            }
        }
        if(pen) {
            // Preserve pointer down/up delivery even when a sensor update is invalid.
            (void)axis.sensors.merge(update);
            axis.sensors.fill(value);
        }
        for(int button=1;button<=3;++button)if(XIMaskIsSet(native->buttons.mask,button))value.buttons|=button==1?1:button==3?2:4;if(down)value.buttons|=touch||native->detail==1?1:native->detail==3?2:4;
        if(down){XSetInputFocus(display,parent,RevertToParent,CurrentTime);XGrabPointer(display,parent,False,PointerMotionMask|ButtonReleaseMask,GrabModeAsync,GrabModeAsync,None,None,CurrentTime);}send(std::move(value));if(up)XUngrabPointer(display,CurrentTime);return true;
    }
    void drag_reply(const char* name,long accepted){if(!drag_source)return;XEvent reply{};reply.xclient.type=ClientMessage;reply.xclient.display=display;reply.xclient.window=drag_source;reply.xclient.message_type=XInternAtom(display,name,False);reply.xclient.format=32;reply.xclient.data.l[0]=long(parent);reply.xclient.data.l[1]=accepted;if(accepted)reply.xclient.data.l[name==std::string_view("XdndStatus")?4:2]=long(XInternAtom(display,"XdndActionCopy",False));XSendEvent(display,drag_source,False,NoEventMask,&reply);XFlush(display);}
    bool drag(XClientMessageEvent& message){const auto atom=[&](const char* name){return XInternAtom(display,name,False);};
        if(message.message_type==atom("XdndEnter")){drag_source=Window(message.data.l[0]);drag_supported=false;std::vector<Atom> types;if(message.data.l[1]&1){Atom actual=0;int format=0;unsigned long count=0,left=0;unsigned char* data=nullptr;if(XGetWindowProperty(display,drag_source,atom("XdndTypeList"),0,256,False,XA_ATOM,&actual,&format,&count,&left,&data)==Success&&actual==XA_ATOM&&format==32&&count<=256&&!left&&data)types.assign(reinterpret_cast<Atom*>(data),reinterpret_cast<Atom*>(data)+count);if(data)XFree(data);}else for(int i=2;i<=4;++i)if(message.data.l[i])types.push_back(Atom(message.data.l[i]));for(auto type:types)for(const auto* mime:{"text/uri-list","image/png","image/tiff","image/jpeg","image/bmp"})if(type==atom(mime))drag_supported=true;return true;}
        if(message.message_type==atom("XdndPosition")){int x=0,y=0;Window child=0;const long xy=message.data.l[2];XTranslateCoordinates(display,DefaultRootWindow(display),parent,short((xy>>16)&0xffff),short(xy&0xffff),&x,&y,&child);drag_x=x/dpi();drag_y=y/dpi();drag_reply("XdndStatus",drag_supported?1:0);return true;}
        if(message.message_type==atom("XdndLeave")){drag_source=0;drag_supported=false;return true;}
        if(message.message_type==atom("XdndDrop")){bool accepted=false;if(drag_supported){auto content=read_x11_selection(display,parent,atom("XdndSelection"));if(content){send({.kind=WidgetInputKind::drop,.x=drag_x,.y=drag_y,.content=std::make_shared<ClipboardContent>(std::move(*content))});accepted=true;}}drag_reply("XdndFinished",accepted?1:0);drag_source=0;drag_supported=false;return true;}return false;
    }
    bool process(XEvent event){
        if(event.type==GenericEvent)return extended(event.xcookie);if(event.xany.window!=parent)return canvas.native_event(&event);
        if(context&&XFilterEvent(&event,parent))return true;
        switch(event.type){
        case ClientMessage:return drag(event.xclient);
        case Expose:dirty=true;flush();return true;
        case ConfigureNotify:dirty=true;return false;
        case FocusIn:if(context)XSetICFocus(context);return false;
        case FocusOut:if(context)XUnsetICFocus(context);send(input(WidgetInputKind::focus_lost,0,event.xfocus.serial));return false;
        case MotionNotify:{if(xi)return true;auto value=input(WidgetInputKind::pointer_move,event.xmotion.state,event.xmotion.time);value.x=event.xmotion.x/dpi();value.y=event.xmotion.y/dpi();value.buttons=((event.xmotion.state&Button1Mask)?1:0)|((event.xmotion.state&Button3Mask)?2:0)|((event.xmotion.state&Button2Mask)?4:0);send(std::move(value));return true;}
        case ButtonPress:case ButtonRelease:{if(xi)return true;const bool down=event.type==ButtonPress;const auto button=event.xbutton.button;auto value=input(button>=4&&button<=7?WidgetInputKind::scroll:down?WidgetInputKind::pointer_down:WidgetInputKind::pointer_up,event.xbutton.state,event.xbutton.time);value.x=event.xbutton.x/dpi();value.y=event.xbutton.y/dpi();if(button>=4&&button<=7){if(!down)return true;if(button<6)value.scroll_y=button==4?48:-48;else value.scroll_x=button==6?48:-48;}else{value.buttons=down?(button==1?1:button==3?2:4):0;if(down){XSetInputFocus(display,parent,RevertToParent,CurrentTime);XGrabPointer(display,parent,False,PointerMotionMask|ButtonReleaseMask,GrabModeAsync,GrabModeAsync,None,None,CurrentTime);}else XUngrabPointer(display,CurrentTime);}send(std::move(value));return true;}
        case KeyPress:case KeyRelease:{KeySym symbol=0;Status status=0;std::vector<char> bytes(128);int count=0;if(context&&event.type==KeyPress){count=Xutf8LookupString(context,&event.xkey,bytes.data(),int(bytes.size()),&symbol,&status);if(status==XBufferOverflow&&(count<=0||count>65536))return true;if(status==XBufferOverflow&&count<=65536){bytes.resize(std::size_t(count)+1);count=Xutf8LookupString(context,&event.xkey,bytes.data(),int(bytes.size()),&symbol,&status);}}else count=XLookupString(&event.xkey,bytes.data(),int(bytes.size()),&symbol,nullptr);
            auto value=input(event.type==KeyPress?WidgetInputKind::key_down:WidgetInputKind::key_up,event.xkey.state,event.xkey.time);value.key=key(symbol);if(symbol>=XK_A&&symbol<=XK_Z)value.text=std::string(1,char('a'+symbol-XK_A));else if(symbol>=XK_a&&symbol<=XK_z)value.text=std::string(1,char(symbol));else if(symbol==XK_plus||symbol==XK_equal)value.text="+";else if(symbol==XK_minus)value.text="-";
            if(event.type==KeyPress&&value.control&&editor()&&value.text=="a")value.key=WidgetKey::select_all;
            if(event.type==KeyPress&&value.control&&editor()&&(value.text=="c"||value.text=="x")){auto field=editor();if(field&&field->kind!=WidgetKind::input_field){value.key=value.text=="c"?WidgetKey::copy:WidgetKey::cut;send(std::move(value));return true;}if(field&&!field->password){const auto selection=field->text_selection;std::size_t units=0,begin=field->text.size(),end=begin;for(std::size_t i=0;i<field->text.size();){if(units==selection.location)begin=i;if(units==selection.location+selection.length){end=i;break;}const auto byte=static_cast<unsigned char>(field->text[i]);const std::size_t length=byte<0x80?1:byte<0xe0?2:byte<0xf0?3:4;units+=length==4?2:1;i+=length;if(i==field->text.size())end=i;}if(begin<=end)(void)write_clipboard({.text=field->text.substr(begin,end-begin)});}if(value.text=="c")return true;value.key=WidgetKey::cut;}
            if(event.type==KeyPress&&value.control&&editor()&&value.text=="v"){auto clipboard=read_clipboard();if(clipboard){if(editor()->kind==WidgetKind::input_field)send({.kind=WidgetInputKind::text,.text=clipboard->text});else send({.kind=WidgetInputKind::text,.content=std::make_shared<const ClipboardContent>(std::move(*clipboard))});}return true;}
            send(std::move(value));if(event.type==KeyPress&&!(event.xkey.state&(ControlMask|Mod1Mask|Mod4Mask))&&count>0&&static_cast<unsigned char>(bytes[0])>=32)send({.kind=WidgetInputKind::text,.text=std::string(bytes.data(),std::size_t(count))});return true;}
        default:return false;
        }
    }
};
WidgetSurface::WidgetSurface(void* parent,WidgetSurfaceCallbacks callbacks):impl_(std::make_unique<Impl>(*static_cast<X11WidgetParent*>(parent),std::move(callbacks))){}
WidgetSurface::~WidgetSurface()=default;void WidgetSurface::invalidate(){impl_->invalidate();}
void WidgetSurface::accessibility_changed(){}void WidgetSurface::accessibility_value_changed(std::string_view){}
void WidgetSurface::set_frame_paced(bool value){impl_->frame_paced=value;}void WidgetSurface::flush(){impl_->flush();}
Size WidgetSurface::size()const{return impl_->size();}double WidgetSurface::backing_scale()const{return impl_->dpi();}
std::string WidgetSurface::resolve_font_family(std::string_view value)const{return FontDatabase::application()->resolve_family(value);}
GraphicsInfo WidgetSurface::info(){return impl_->canvas.info();}PixelBuffer WidgetSurface::capture(){impl_->flush();return impl_->canvas.capture();}
bool WidgetSurface::native_event(const void* value){try{return impl_->process(*static_cast<const XEvent*>(value));}catch(...){impl_->callbacks.failure(std::current_exception());return true;}}
}
