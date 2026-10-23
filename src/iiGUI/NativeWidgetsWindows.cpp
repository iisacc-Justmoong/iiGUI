#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#include <imm.h>
#include <shellapi.h>
#include "WidgetSurface.h"
#include "NativeCanvas.h"
#include "DesktopCommon.h"
#include <algorithm>

namespace ii::gui::detail {
namespace {
std::string utf8(std::wstring_view text){if(text.empty())return {};const int n=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,text.data(),int(text.size()),nullptr,0,nullptr,nullptr);if(n<=0)throw std::runtime_error("Invalid native UTF-16 input");std::string value(std::size_t(n),'\0');WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,text.data(),int(text.size()),value.data(),n,nullptr,nullptr);return value;}
WidgetKey key(WPARAM value){switch(value){case VK_TAB:return WidgetKey::tab;case VK_RETURN:return WidgetKey::enter;case VK_ESCAPE:return WidgetKey::escape;case VK_SPACE:return WidgetKey::space;case VK_LEFT:return WidgetKey::left;case VK_RIGHT:return WidgetKey::right;case VK_UP:return WidgetKey::up;case VK_DOWN:return WidgetKey::down;case VK_HOME:return WidgetKey::home;case VK_END:return WidgetKey::end;case VK_BACK:return WidgetKey::backspace;case VK_DELETE:return WidgetKey::delete_forward;default:return WidgetKey::character;}}
WidgetInput event(WidgetInputKind kind){return {.kind=kind,.shift=(GetKeyState(VK_SHIFT)&0x8000)!=0,.control=(GetKeyState(VK_CONTROL)&0x8000)!=0,.alt=(GetKeyState(VK_MENU)&0x8000)!=0,.meta=(GetKeyState(VK_LWIN)&0x8000)||(GetKeyState(VK_RWIN)&0x8000),.timestamp=double(DWORD(GetMessageTime()))/1000};}
}
struct WidgetSurface::Impl final {
    HWND parent;WidgetSurfaceCallbacks callbacks;NativeCanvas canvas;bool dirty=true,frame_paced=false;wchar_t surrogate=0;
    Impl(HWND window,WidgetSurfaceCallbacks value):parent(window),callbacks(std::move(value)),canvas(window,nullptr,Acceleration::prefer_hardware,callbacks.renderer){DragAcceptFiles(parent,TRUE);}
    ~Impl(){DragAcceptFiles(parent,FALSE);}
    double scale()const{const auto dpi=GetDpiForWindow(parent);return dpi?dpi/96.:1;}
    Size size()const{RECT r{};GetClientRect(parent,&r);return {(r.right-r.left)/scale(),(r.bottom-r.top)/scale()};}
    void flush(){if(!dirty)return;auto s=size();if(s.width<=0||s.height<=0)return;canvas.draw(callbacks.frame(s),scale(),s.height);dirty=false;}
    void send(WidgetInput input){callbacks.input(std::move(input));dirty=true;if(!frame_paced)InvalidateRect(parent,nullptr,FALSE);}
    std::optional<WidgetSnapshot> editor()const{for(auto& value:callbacks.accessibility())if(value.focused&&value.kind==WidgetKind::input_field)return value;return {};}
    void caret(){auto focused=editor();if(!focused)return;auto r=focused->bounds;HIMC context=ImmGetContext(parent);if(!context)return;COMPOSITIONFORM form{CFS_POINT,{LONG((r.x+8)*scale()),LONG((r.y+r.height)*scale())},{}};ImmSetCompositionWindow(context,&form);CANDIDATEFORM candidates{0,CFS_CANDIDATEPOS,form.ptCurrentPos,{}};ImmSetCandidateWindow(context,&candidates);ImmReleaseContext(parent,context);}
    bool process(const MSG& message){
        const auto code=message.message;const auto w=message.wParam;const auto l=message.lParam;const auto dpi=scale();
        if(code==WM_DROPFILES){auto drop=reinterpret_cast<HDROP>(w);struct Release final{HDROP value;~Release(){DragFinish(value);}} release{drop};auto content=std::make_shared<ClipboardContent>();POINT location{};DragQueryPoint(drop,&location);const UINT count=DragQueryFileW(drop,0xffffffff,nullptr,0);if(count>256)return true;for(UINT i=0;i<count;++i){const UINT length=DragQueryFileW(drop,i,nullptr,0);if(length>32768)return true;std::wstring path(std::size_t(length)+1,L'\0');DragQueryFileW(drop,i,path.data(),length+1);path.resize(length);content->files.push_back(utf8(path));}if(valid_clipboard(*content))send({.kind=WidgetInputKind::drop,.x=location.x/dpi,.y=location.y/dpi,.content=std::move(content)});return true;}
        if(code==WM_PAINT){PAINTSTRUCT paint{};BeginPaint(parent,&paint);dirty=true;flush();EndPaint(parent,&paint);return true;}if(code==WM_SIZE||code==WM_DPICHANGED){dirty=true;flush();return false;}
        if(code==WM_KILLFOCUS||code==WM_CAPTURECHANGED||code==WM_CANCELMODE){send(event(WidgetInputKind::focus_lost));return false;}
        if(code==WM_MOUSEMOVE||code==WM_LBUTTONDOWN||code==WM_RBUTTONDOWN||code==WM_MBUTTONDOWN||code==WM_XBUTTONDOWN||code==WM_LBUTTONUP||code==WM_RBUTTONUP||code==WM_MBUTTONUP||code==WM_XBUTTONUP){
            // Promoted pen messages are already delivered with pressure below.
            if((GetMessageExtraInfo()&0xffffff00)==0xff515700)return true;
            const bool down=code==WM_LBUTTONDOWN||code==WM_RBUTTONDOWN||code==WM_MBUTTONDOWN||code==WM_XBUTTONDOWN;
            const bool up=code==WM_LBUTTONUP||code==WM_RBUTTONUP||code==WM_MBUTTONUP||code==WM_XBUTTONUP;
            auto input=event(down?WidgetInputKind::pointer_down:up?WidgetInputKind::pointer_up:WidgetInputKind::pointer_move);input.x=GET_X_LPARAM(l)/dpi;input.y=GET_Y_LPARAM(l)/dpi;
            input.buttons=((w&MK_LBUTTON)?1:0)|((w&MK_RBUTTON)?2:0)|((w&MK_MBUTTON)?4:0)|((w&MK_XBUTTON1)?8:0)|((w&MK_XBUTTON2)?16:0);
            if(down){SetFocus(parent);SetCapture(parent);}send(std::move(input));if(up&&GetCapture()==parent)ReleaseCapture();caret();return true;
        }
        if(code==WM_POINTERDOWN||code==WM_POINTERUPDATE||code==WM_POINTERUP){POINTER_PEN_INFO pen{};const auto id=GET_POINTERID_WPARAM(w);if(!GetPointerPenInfo(id,&pen))return false;POINT point=pen.pointerInfo.ptPixelLocation;ScreenToClient(parent,&point);auto input=event(code==WM_POINTERDOWN?WidgetInputKind::pointer_down:code==WM_POINTERUP?WidgetInputKind::pointer_up:WidgetInputKind::pointer_move);input.x=point.x/dpi;input.y=point.y/dpi;input.device=(pen.penFlags&PEN_FLAG_ERASER)?PointerDevice::eraser:PointerDevice::pen;input.device_id=id;input.pressure=(pen.penMask&PEN_MASK_PRESSURE)?std::clamp(pen.pressure/1024.,0.,1.):1;input.tilt_x=pen.tiltX;input.tilt_y=pen.tiltY;input.rotation=pen.rotation;input.buttons=(pen.pointerInfo.pointerFlags&POINTER_FLAG_FIRSTBUTTON)?1:0;send(std::move(input));return true;}
        if(code==WM_MOUSEWHEEL||code==WM_MOUSEHWHEEL){POINT p{GET_X_LPARAM(l),GET_Y_LPARAM(l)};ScreenToClient(parent,&p);auto input=event(WidgetInputKind::scroll);input.x=p.x/dpi;input.y=p.y/dpi;if(code==WM_MOUSEWHEEL)input.scroll_y=GET_WHEEL_DELTA_WPARAM(w)*48./WHEEL_DELTA;else input.scroll_x=GET_WHEEL_DELTA_WPARAM(w)*48./WHEEL_DELTA;send(std::move(input));return true;}
        if(code==WM_KEYDOWN||code==WM_SYSKEYDOWN||code==WM_KEYUP||code==WM_SYSKEYUP){auto input=event((code==WM_KEYUP||code==WM_SYSKEYUP)?WidgetInputKind::key_up:WidgetInputKind::key_down);input.key=key(w);if(w>='A'&&w<='Z')input.text=std::string(1,char('a'+w-'A'));if(w==VK_OEM_PLUS)input.text="+";if(w==VK_OEM_MINUS)input.text="-";
            if(input.control&&input.kind==WidgetInputKind::key_down&&editor()){
                if(w=='A')input.key=WidgetKey::select_all;
                else if(w=='V'){auto content=read_clipboard();if(content)send({.kind=WidgetInputKind::text,.text=content->text});return true;}
                else if(w=='C'||w=='X'){auto field=editor();if(field&&!field->password){const int count=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,field->text.data(),int(field->text.size()),nullptr,0);std::wstring wide(std::size_t(count),L'\0');MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,field->text.data(),int(field->text.size()),wide.data(),count);const auto r=field->text_selection;if(r.location<=wide.size()&&r.length<=wide.size()-r.location)(void)write_clipboard({.text=utf8(std::wstring_view(wide).substr(r.location,r.length))});}if(w=='C')return true;input.key=WidgetKey::cut;}
            }send(std::move(input));caret();return true;}
        if(code==WM_CHAR){const auto c=wchar_t(w);if(c<32)return true;if(c>=0xd800&&c<=0xdbff){surrogate=c;return true;}std::wstring value;if(surrogate){if(c<0xdc00||c>0xdfff){surrogate=0;return true;}value={surrogate,c};surrogate=0;}else value={c};send({.kind=WidgetInputKind::text,.text=utf8(value)});caret();return true;}
        if(code==WM_IME_COMPOSITION){HIMC context=ImmGetContext(parent);if(!context)return false;const bool committed=(l&GCS_RESULTSTR)!=0;const DWORD flag=committed?GCS_RESULTSTR:GCS_COMPSTR;if(l&flag){const LONG length=ImmGetCompositionStringW(context,flag,nullptr,0);if(length>=0&&length<=131072){std::wstring value(std::size_t(length)/2,L'\0');ImmGetCompositionStringW(context,flag,value.data(),DWORD(length));auto input=event(committed?WidgetInputKind::text:WidgetInputKind::composition);input.text=utf8(value);if(!committed){const auto cursor=ImmGetCompositionStringW(context,GCS_CURSORPOS,nullptr,0);input.composition_selection=TextRange{std::size_t(std::clamp<LONG>(cursor,0,LONG(value.size()))),0};}send(std::move(input));}}ImmReleaseContext(parent,context);caret();return true;}
        if(code==WM_IME_ENDCOMPOSITION){send({.kind=WidgetInputKind::composition});return false;}
        return canvas.native_event(&message);
    }
};
WidgetSurface::WidgetSurface(void* p,WidgetSurfaceCallbacks callbacks):impl_(std::make_unique<Impl>(static_cast<HWND>(p),std::move(callbacks))){}
WidgetSurface::~WidgetSurface()=default;
void WidgetSurface::invalidate(){impl_->dirty=true;if(!impl_->frame_paced)InvalidateRect(impl_->parent,nullptr,FALSE);}
void WidgetSurface::accessibility_changed(){NotifyWinEvent(EVENT_OBJECT_REORDER,impl_->parent,OBJID_CLIENT,CHILDID_SELF);}
void WidgetSurface::accessibility_value_changed(std::string_view){NotifyWinEvent(EVENT_OBJECT_VALUECHANGE,impl_->parent,OBJID_CLIENT,CHILDID_SELF);}
void WidgetSurface::set_frame_paced(bool value){impl_->frame_paced=value;}
void WidgetSurface::flush(){impl_->flush();}
Size WidgetSurface::size()const{return impl_->size();}
double WidgetSurface::backing_scale()const{return impl_->scale();}
std::string WidgetSurface::resolve_font_family(std::string_view value)const{return value.empty()?"Segoe UI":std::string(value);}
GraphicsInfo WidgetSurface::info(){return impl_->canvas.info();}
PixelBuffer WidgetSurface::capture(){impl_->flush();return impl_->canvas.capture();}
bool WidgetSurface::native_event(const void* value){try{return impl_->process(*static_cast<const MSG*>(value));}catch(...){impl_->callbacks.failure(std::current_exception());return true;}}
}
