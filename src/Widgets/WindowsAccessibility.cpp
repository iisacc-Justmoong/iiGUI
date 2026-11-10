#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <oleacc.h>
#include "WindowsAccessibility.h"
#include "WindowsAccessibilityQueue.h"
#include "WindowsAccessibilityObjectIds.h"
#include "WindowsAccessibilityTree.h"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <limits>
#include <iterator>
#include <mutex>
#include <new>
#include <thread>
#include <unordered_map>

namespace ii::gui::detail {
namespace {
constexpr UINT action_message=WM_APP+0x14a;
std::atomic<std::uintptr_t> next_cookie{1};
std::uintptr_t cookie(){
    auto current=next_cookie.load();
    while(current){const auto next=current==std::numeric_limits<std::uintptr_t>::max()?0:current+1;
        if(next_cookie.compare_exchange_weak(current,next))return current;}
    return 0;
}
struct Published final {
    explicit Published(std::vector<WidgetSnapshot> nodes):tree(nodes){}
    Win32AccessibilityTree tree;
    POINT origin{};RECT client{};double scale=1;
    std::wstring title;
    std::unordered_map<std::uint32_t,std::string> objects;
    std::unordered_map<std::string,std::uint32_t> object_ids;
};
template<class Function> HRESULT guarded(Function&& function) noexcept {
    try{return function();}catch(const std::bad_alloc&){return E_OUTOFMEMORY;}catch(...){return E_FAIL;}
}
HRESULT bstr(std::string_view value,BSTR* result){
    if(!result)return E_POINTER;*result=nullptr;
    if(value.size()>std::size_t(std::numeric_limits<int>::max()))return E_INVALIDARG;
    if(value.empty()){*result=SysAllocStringLen(nullptr,0);return *result?S_OK:E_OUTOFMEMORY;}
    const auto length=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,value.data(),int(value.size()),nullptr,0);
    if(length<=0)return E_FAIL;auto text=SysAllocStringLen(nullptr,UINT(length));if(!text)return E_OUTOFMEMORY;
    if(MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,value.data(),int(value.size()),text,length)!=length){SysFreeString(text);return E_FAIL;}
    *result=text;return S_OK;
}
std::optional<std::string> utf8_value(BSTR value){
    const auto length=value?SysStringLen(value):0;
    if(length>65536)return {};
    std::string result;result.reserve(length);
    for(UINT i=0;i<length;++i){
        std::uint32_t scalar=std::uint32_t(value[i]);
        if(!scalar||scalar>0xffff)return {};
        if(scalar>=0xd800&&scalar<=0xdbff){
            if(++i==length)return {};const auto low=std::uint32_t(value[i]);
            if(low<0xdc00||low>0xdfff)return {};scalar=0x10000+((scalar-0xd800)<<10)+(low-0xdc00);
        }else if(scalar>=0xdc00&&scalar<=0xdfff)return {};
        if(scalar<0x80)result.push_back(char(scalar));
        else if(scalar<0x800){result.push_back(char(0xc0|(scalar>>6)));result.push_back(char(0x80|(scalar&63)));}
        else if(scalar<0x10000){result.push_back(char(0xe0|(scalar>>12)));result.push_back(char(0x80|((scalar>>6)&63)));result.push_back(char(0x80|(scalar&63)));}
        else{result.push_back(char(0xf0|(scalar>>18)));result.push_back(char(0x80|((scalar>>12)&63)));result.push_back(char(0x80|((scalar>>6)&63)));result.push_back(char(0x80|(scalar&63)));}
        if(result.size()>65536)return {};
    }
    return result;
}
LONG bounded(double value){
    if(!std::isfinite(value))return 0;
    return LONG(std::clamp(value,double(std::numeric_limits<LONG>::min()),double(std::numeric_limits<LONG>::max())));
}
long native_role(Win32AccessibilityRole role){
    using Role=Win32AccessibilityRole;
    switch(role){
        case Role::group:return ROLE_SYSTEM_GROUPING;case Role::text:return ROLE_SYSTEM_STATICTEXT;
        case Role::button:return ROLE_SYSTEM_PUSHBUTTON;case Role::link:return ROLE_SYSTEM_LINK;
        case Role::check_box:return ROLE_SYSTEM_CHECKBUTTON;case Role::radio_button:return ROLE_SYSTEM_RADIOBUTTON;
        case Role::tab:return ROLE_SYSTEM_PAGETAB;case Role::tab_group:return ROLE_SYSTEM_PAGETABLIST;
        case Role::input:return ROLE_SYSTEM_TEXT;case Role::slider:return ROLE_SYSTEM_SLIDER;
        case Role::stepper:return ROLE_SYSTEM_SPINBUTTON;case Role::progress:return ROLE_SYSTEM_PROGRESSBAR;
        case Role::menu:return ROLE_SYSTEM_MENUPOPUP;case Role::menu_item:return ROLE_SYSTEM_MENUITEM;
        case Role::table:return ROLE_SYSTEM_TABLE;case Role::cell:return ROLE_SYSTEM_CELL;
        case Role::column_header:return ROLE_SYSTEM_COLUMNHEADER;case Role::image:return ROLE_SYSTEM_GRAPHIC;
        case Role::scroll_area:return ROLE_SYSTEM_PANE;
    }
    return ROLE_SYSTEM_CLIENT;
}
}

struct WindowsAccessibilityState final {
    HWND window;WidgetSurfaceCallbacks callbacks;const std::uintptr_t identity=cookie();
    const std::thread::id owner=std::this_thread::get_id();
    std::atomic<bool> live{true};std::mutex published_mutex;std::shared_ptr<const Published> published;
    Win32AccessibilityQueue actions;Win32AccessibilityObjectIds object_ids;bool updating=false,again=false;
    WindowsAccessibilityState(HWND value,const WidgetSurfaceCallbacks& c):window(value),callbacks(c){}
    std::shared_ptr<const Published> load(){std::lock_guard lock(published_mutex);return published;}
    void store(std::shared_ptr<const Published> value){std::lock_guard lock(published_mutex);published=std::move(value);}
    void refresh(){
        if(!live.load())return;if(updating){again=true;return;}updating=true;
        struct Reset{bool& flag;~Reset(){flag=false;}}reset{updating};
        for(int attempt=0;attempt<2;++attempt){
            again=false;auto previous=load();auto accessibility=callbacks.accessibility;
            auto nodes=accessibility?accessibility():std::vector<WidgetSnapshot>{};
            if(!live.load())return;auto content=callbacks.text_content;auto lookup=callbacks.lookup;
            // Read custom text providers only on the owner thread. Never query
            // passwords or export a result whose revision changed during read.
            for(auto& n:nodes)if(n.text_input&&!n.password&&n.text_revision&&n.text_length<=65536&&content&&lookup){
                auto text=content(n.id,{0,n.text_length});if(!text)continue;
                auto after=lookup(n.id);
                if(after.attached&&after.text_revision==n.text_revision&&after.text_length==n.text_length&&text->range==TextRange{0,n.text_length})n.text=std::move(text->text);
            }
            if(!live.load())return;
            auto value=std::make_shared<Published>(std::move(nodes));
            std::vector<std::string> identities;identities.reserve(value->tree.size());
            for(std::size_t i=0;i<value->tree.size();++i)identities.push_back(value->tree.node(i).snapshot.id);
            value->object_ids=object_ids.update(identities);
            for(const auto& [id,object]:value->object_ids)value->objects.emplace(object,id);
            GetClientRect(window,&value->client);ClientToScreen(window,&value->origin);
            const auto dpi=GetDpiForWindow(window);value->scale=dpi?dpi/96.:1;
            const auto length=std::max(0,GetWindowTextLengthW(window));value->title.resize(std::size_t(length)+1);
            const auto copied=GetWindowTextW(window,value->title.data(),length+1);value->title.resize(std::size_t(std::max(0,copied)));
            std::string old_focus,new_focus;
            if(previous)if(auto focus=previous->tree.focus())old_focus=previous->tree.node(*focus).snapshot.id;
            if(auto focus=value->tree.focus())new_focus=value->tree.node(*focus).snapshot.id;
            auto event=value->object_ids.find(new_focus);const auto focused_object=event==value->object_ids.end()?0:event->second;
            store(std::move(value));
            if(live.load()&&focused_object&&new_focus!=old_focus)NotifyWinEvent(EVENT_OBJECT_FOCUS,window,LONG(focused_object),CHILDID_SELF);
            if(!again||!live.load())break;
        }
    }
    bool enqueue(std::string target,std::uint32_t object_id,WidgetInputKind kind){
        if(!live.load()||!identity)return false;
        auto token=actions.admit({std::move(target),kind,object_id});if(!token)return false;
        if(PostMessageW(window,action_message,WPARAM(identity),LPARAM(*token)))return true;
        (void)actions.take(*token);return false;
    }
    HRESULT replace_text(Win32AccessibilityQueue::Request& request){
        if(!live.load())return CO_E_OBJNOTCONNECTED;
        refresh();if(!live.load())return CO_E_OBJNOTCONNECTED;
        auto current=load();if(!current)return CO_E_OBJNOTCONNECTED;
        const auto object=current->object_ids.find(request.target);
        if(object==current->object_ids.end()||object->second!=request.object_id)return CO_E_OBJNOTCONNECTED;
        auto index=current->tree.find(request.target);if(!index)return CO_E_OBJNOTCONNECTED;
        const auto& node=current->tree.node(*index);const auto& snapshot=node.snapshot;
        if(node.role!=Win32AccessibilityRole::input||!snapshot.text_input||!snapshot.text_revision)return DISP_E_MEMBERNOTFOUND;
        if(!snapshot.enabled||snapshot.read_only||snapshot.password)return E_ACCESSDENIED;
        if(!request.revision||*snapshot.text_revision!=*request.revision)return E_FAIL;
        auto replace=callbacks.accessibility_text;if(!replace)return DISP_E_MEMBERNOTFOUND;
        auto result=replace(request.target,*request.revision,std::move(request.text));
        if(result)return S_OK;
        if(result.error().code==core::ErrorCode::closed||result.error().code==core::ErrorCode::not_found)return CO_E_OBJNOTCONNECTED;
        if(result.error().code==core::ErrorCode::invalid_argument||result.error().code==core::ErrorCode::out_of_range)return E_INVALIDARG;
        if(result.error().code==core::ErrorCode::unsupported)return E_ACCESSDENIED;
        return E_FAIL;
    }
    HRESULT set_text(std::string target,std::uint32_t object_id,TextInputRevision revision,std::string text){
        if(!live.load()||!identity)return CO_E_OBJNOTCONNECTED;
        Win32AccessibilityQueue::Request request{std::move(target),WidgetInputKind::text,object_id,std::move(text),std::move(revision),{}};
        if(std::this_thread::get_id()==owner)return guarded([&]{return replace_text(request);});
        auto completion=std::make_shared<Win32AccessibilityQueue::Completion>();request.completion=completion;
        auto token=actions.admit(std::move(request));if(!token)return CO_E_OBJNOTCONNECTED;
        if(!PostMessageW(window,action_message,WPARAM(identity),LPARAM(*token))){(void)actions.take(*token);completion->cancel();return E_FAIL;}
        auto result=completion->wait();
        if(!result){(void)actions.take(*token);return live.load()?E_FAIL:CO_E_OBJNOTCONNECTED;}
        return HRESULT(*result);
    }
    bool input(const MSG& event){
        if(event.message!=action_message||std::uintptr_t(event.wParam)!=identity)return false;
        auto request=actions.take(std::uint32_t(event.lParam));if(!request)return true;
        if(!live.load()){if(request->completion)request->completion->cancel();return true;}
        if(request->kind==WidgetInputKind::text){
            auto completion=request->completion;if(!completion||!completion->begin())return true;
            const auto result=guarded([&]{return replace_text(*request);});completion->finish(result);return true;
        }
        const auto current_target=[&]{
            if(!live.load())return false;auto current=load();if(!current)return false;
            const auto object=current->object_ids.find(request->target);
            if(object==current->object_ids.end()||object->second!=request->object_id)return false;
            auto index=current->tree.find(request->target);if(!index)return false;
            const auto& node=current->tree.node(*index);
            return node.snapshot.enabled&&(request->kind!=WidgetInputKind::activate||node.actionable);
        };
        refresh();if(!current_target())return true;
        if(request->kind==WidgetInputKind::focus){SetFocus(window);refresh();if(!current_target())return true;}
        if(!live.load())return true;auto send=callbacks.input;
        if(send){WidgetInput input;input.kind=request->kind;input.target=std::move(request->target);send(std::move(input));}return true;
    }
    void retire(){live.store(false);actions.close();store({});callbacks={};}
};

namespace {
constexpr std::size_t root_index=std::numeric_limits<std::size_t>::max();
class Accessible final : public IAccessible {
    std::atomic<ULONG> refs_{1};std::shared_ptr<WindowsAccessibilityState> state_;std::string id_;std::uint32_t object_id_=0;
    using Tree=Win32AccessibilityTree;
    std::shared_ptr<const Published> snapshot()const{
        if(!state_->live.load())return {};auto value=state_->load();
        if(!value)return {};
        if(!id_.empty()){
            const auto object=value->object_ids.find(id_);
            if(object==value->object_ids.end()||object->second!=object_id_||!value->tree.find(id_))return {};
        }
        return value;
    }
    std::optional<std::size_t> self(const Tree& tree)const{return id_.empty()?std::optional<std::size_t>{root_index}:tree.find(id_);}
    static const std::vector<std::size_t>& children(const Tree& tree,std::size_t id){return id==root_index?tree.roots():tree.node(id).children;}
    std::optional<std::size_t> resolve(const Tree& tree,VARIANT child)const{
        const auto owner=self(tree);if(!owner||child.vt!=VT_I4||child.lVal<0)return {};
        if(child.lVal==CHILDID_SELF)return owner;
        const auto& entries=children(tree,*owner);const auto index=std::size_t(child.lVal-1);
        return index<entries.size()?std::optional<std::size_t>{entries[index]}:std::nullopt;
    }
    bool descendant(const Tree& tree,std::size_t child,std::size_t parent)const{
        if(parent==root_index)return true;
        for(auto cursor=std::optional<std::size_t>{child};cursor;cursor=tree.node(*cursor).parent)if(*cursor==parent)return true;
        return false;
    }
    HRESULT dispatch(const Published& current,std::size_t id,VARIANT* result){
        if(!result)return E_POINTER;VariantInit(result);
        if(id==root_index){AddRef();result->pdispVal=this;}
        else{const auto& name=current.tree.node(id).snapshot.id;const auto object=current.object_ids.find(name);
            if(object==current.object_ids.end())return CO_E_OBJNOTCONNECTED;
            result->pdispVal=new Accessible(state_,name,object->second);}
        result->vt=VT_DISPATCH;return S_OK;
    }
    template<class Function> HRESULT with(VARIANT child,Function&& function){
        auto current=snapshot();if(!current)return CO_E_OBJNOTCONNECTED;
        auto index=resolve(current->tree,child);if(!index)return E_INVALIDARG;
        return function(*current,*index);
    }
public:
    Accessible(std::shared_ptr<WindowsAccessibilityState> state,std::string id={},std::uint32_t object_id=0):state_(std::move(state)),id_(std::move(id)),object_id_(object_id){}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid,void** result) override{
        if(!result)return E_POINTER;*result=nullptr;
        if(IsEqualIID(iid,__uuidof(IUnknown))||IsEqualIID(iid,__uuidof(IDispatch))||IsEqualIID(iid,__uuidof(IAccessible))){*result=static_cast<IAccessible*>(this);AddRef();return S_OK;}return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override{return ++refs_;}
    ULONG STDMETHODCALLTYPE Release() override{const auto left=--refs_;if(!left)delete this;return left;}
    HRESULT STDMETHODCALLTYPE GetTypeInfoCount(UINT* count) override{if(!count)return E_POINTER;*count=0;return S_OK;}
    HRESULT STDMETHODCALLTYPE GetTypeInfo(UINT,LCID,ITypeInfo** out) override{if(out)*out=nullptr;return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE GetIDsOfNames(REFIID,LPOLESTR*,UINT,LCID,DISPID*) override{return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE Invoke(DISPID,REFIID,LCID,WORD,DISPPARAMS*,VARIANT* result,EXCEPINFO*,UINT*) override{if(result)VariantInit(result);return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE get_accParent(IDispatch** result) override{return guarded([&]()->HRESULT{
        if(!result)return E_POINTER;*result=nullptr;auto current=snapshot();if(!current)return CO_E_OBJNOTCONNECTED;
        if(id_.empty()){
            const auto status=AccessibleObjectFromWindow(state_->window,OBJID_WINDOW,__uuidof(IDispatch),reinterpret_cast<void**>(result));
            if(!state_->live.load()){if(*result)(*result)->Release();*result=nullptr;return CO_E_OBJNOTCONNECTED;}
            return status;
        }
        auto index=current->tree.find(id_);const auto parent=current->tree.node(*index).parent;
        if(!parent){*result=new Accessible(state_);return S_OK;}
        const auto& parent_id=current->tree.node(*parent).snapshot.id;const auto object=current->object_ids.find(parent_id);
        if(object==current->object_ids.end())return CO_E_OBJNOTCONNECTED;
        *result=new Accessible(state_,parent_id,object->second);return S_OK;
    });}
    HRESULT STDMETHODCALLTYPE get_accChildCount(LONG* result) override{return guarded([&]()->HRESULT{
        if(!result)return E_POINTER;*result=0;auto current=snapshot();if(!current)return CO_E_OBJNOTCONNECTED;
        const auto index=self(current->tree);*result=LONG(children(current->tree,*index).size());return S_OK;
    });}
    HRESULT STDMETHODCALLTYPE get_accChild(VARIANT child,IDispatch** result) override{return guarded([&]()->HRESULT{
        if(!result)return E_POINTER;*result=nullptr;if(child.vt!=VT_I4||child.lVal==CHILDID_SELF)return E_INVALIDARG;
        return with(child,[&](const Published& current,std::size_t index)->HRESULT{
            const auto& id=current.tree.node(index).snapshot.id;const auto object=current.object_ids.find(id);
            if(object==current.object_ids.end())return CO_E_OBJNOTCONNECTED;
            *result=new Accessible(state_,id,object->second);return S_OK;});
    });}
    HRESULT STDMETHODCALLTYPE get_accName(VARIANT child,BSTR* result) override{return guarded([&]()->HRESULT{
        if(!result)return E_POINTER;*result=nullptr;return with(child,[&](const Published& current,std::size_t index)->HRESULT{
            if(index!=root_index)return bstr(current.tree.node(index).name,result);
            *result=SysAllocStringLen(current.title.data(),UINT(current.title.size()));return *result?S_OK:E_OUTOFMEMORY;});
    });}
    HRESULT STDMETHODCALLTYPE get_accValue(VARIANT child,BSTR* result) override{return guarded([&]()->HRESULT{
        if(!result)return E_POINTER;*result=nullptr;return with(child,[&](const Published& current,std::size_t index)->HRESULT{
            if(index==root_index)return S_FALSE;const auto& n=current.tree.node(index);if(n.snapshot.password)return E_ACCESSDENIED;
            if(n.value)return bstr(*n.value,result);
            if(n.role==Win32AccessibilityRole::progress&&n.snapshot.accessibility_semantics&&!n.snapshot.accessibility_semantics->range)return S_FALSE;
            if(n.role==Win32AccessibilityRole::slider||n.role==Win32AccessibilityRole::progress)return bstr(std::to_string(n.snapshot.value),result);
            return S_FALSE;});
    });}
    HRESULT STDMETHODCALLTYPE get_accDescription(VARIANT child,BSTR* result) override{return guarded([&]()->HRESULT{
        if(!result)return E_POINTER;*result=nullptr;return with(child,[&](const Published& current,std::size_t index)->HRESULT{
            if(index==root_index||current.tree.node(index).snapshot.accessibility_description.empty())return S_FALSE;
            return bstr(current.tree.node(index).snapshot.accessibility_description,result);});
    });}
    HRESULT STDMETHODCALLTYPE get_accRole(VARIANT child,VARIANT* result) override{return guarded([&]()->HRESULT{
        if(!result)return E_POINTER;VariantInit(result);return with(child,[&](const Published& current,std::size_t index)->HRESULT{
            result->vt=VT_I4;result->lVal=index==root_index?ROLE_SYSTEM_CLIENT:native_role(current.tree.node(index).role);return S_OK;});
    });}
    HRESULT STDMETHODCALLTYPE get_accState(VARIANT child,VARIANT* result) override{return guarded([&]()->HRESULT{
        if(!result)return E_POINTER;VariantInit(result);return with(child,[&](const Published& current,std::size_t index)->HRESULT{
            result->vt=VT_I4;result->lVal=0;if(index==root_index)return S_OK;
            const auto& n=current.tree.node(index);if(!n.snapshot.enabled)result->lVal|=STATE_SYSTEM_UNAVAILABLE;
            if(n.snapshot.focused)result->lVal|=STATE_SYSTEM_FOCUSED;
            if(n.actionable||n.role==Win32AccessibilityRole::input||n.role==Win32AccessibilityRole::slider)result->lVal|=STATE_SYSTEM_FOCUSABLE;
            if(n.role==Win32AccessibilityRole::tab||n.role==Win32AccessibilityRole::cell||n.role==Win32AccessibilityRole::radio_button)result->lVal|=STATE_SYSTEM_SELECTABLE;
            if(n.selected)result->lVal|=STATE_SYSTEM_SELECTED;
            if(n.snapshot.checked)result->lVal|=STATE_SYSTEM_CHECKED;
            if(n.snapshot.read_only)result->lVal|=STATE_SYSTEM_READONLY;
            if(n.snapshot.password)result->lVal|=STATE_SYSTEM_PROTECTED;
            if(n.bounds.width<=0||n.bounds.height<=0)result->lVal|=STATE_SYSTEM_OFFSCREEN;
            return S_OK;});
    });}
    HRESULT STDMETHODCALLTYPE get_accHelp(VARIANT,BSTR* result) override{if(result)*result=nullptr;return result?S_FALSE:E_POINTER;}
    HRESULT STDMETHODCALLTYPE get_accHelpTopic(BSTR* file,VARIANT,LONG* topic) override{if(file)*file=nullptr;if(topic)*topic=-1;return file&&topic?S_FALSE:E_POINTER;}
    HRESULT STDMETHODCALLTYPE get_accKeyboardShortcut(VARIANT,BSTR* result) override{if(result)*result=nullptr;return result?S_FALSE:E_POINTER;}
    HRESULT STDMETHODCALLTYPE get_accFocus(VARIANT* result) override{return guarded([&]()->HRESULT{
        if(!result)return E_POINTER;VariantInit(result);auto current=snapshot();if(!current)return CO_E_OBJNOTCONNECTED;
        const auto owner=self(current->tree),focus=current->tree.focus();if(!focus||!descendant(current->tree,*focus,*owner))return S_FALSE;
        if(*focus==*owner){result->vt=VT_I4;result->lVal=CHILDID_SELF;return S_OK;}return dispatch(*current,*focus,result);
    });}
    HRESULT STDMETHODCALLTYPE get_accSelection(VARIANT* result) override{return guarded([&]()->HRESULT{
        if(!result)return E_POINTER;VariantInit(result);auto current=snapshot();if(!current)return CO_E_OBJNOTCONNECTED;
        auto owner=self(current->tree);std::optional<std::size_t> selected;
        for(auto id:children(current->tree,*owner))if(current->tree.node(id).selected){if(selected)return S_FALSE;selected=id;}
        return selected?dispatch(*current,*selected,result):S_FALSE;
    });}
    HRESULT STDMETHODCALLTYPE get_accDefaultAction(VARIANT child,BSTR* result) override{return guarded([&]()->HRESULT{
        if(!result)return E_POINTER;*result=nullptr;return with(child,[&](const Published& current,std::size_t index)->HRESULT{
            return index!=root_index&&current.tree.node(index).actionable?bstr("Press",result):S_FALSE;});
    });}
    HRESULT STDMETHODCALLTYPE accSelect(LONG flags,VARIANT child) override{return guarded([&]()->HRESULT{
        if(flags!=SELFLAG_TAKEFOCUS)return E_NOTIMPL;
        return with(child,[&](const Published& current,std::size_t index)->HRESULT{
            if(index==root_index||!current.tree.node(index).snapshot.enabled)return E_INVALIDARG;
            const auto& id=current.tree.node(index).snapshot.id;const auto object=current.object_ids.find(id);
            if(object==current.object_ids.end())return CO_E_OBJNOTCONNECTED;
            return state_->enqueue(id,object->second,WidgetInputKind::focus)?S_OK:CO_E_OBJNOTCONNECTED;});
    });}
    HRESULT STDMETHODCALLTYPE accLocation(LONG* x,LONG* y,LONG* width,LONG* height,VARIANT child) override{return guarded([&]()->HRESULT{
        if(!x||!y||!width||!height)return E_POINTER;*x=*y=*width=*height=0;
        return with(child,[&](const Published& current,std::size_t index)->HRESULT{
            if(index==root_index){*x=current.origin.x;*y=current.origin.y;*width=current.client.right-current.client.left;*height=current.client.bottom-current.client.top;return S_OK;}
            const auto r=current.tree.node(index).bounds;*x=bounded(current.origin.x+r.x*current.scale);*y=bounded(current.origin.y+r.y*current.scale);
            *width=bounded(r.width*current.scale);*height=bounded(r.height*current.scale);return S_OK;});
    });}
    HRESULT STDMETHODCALLTYPE accNavigate(LONG direction,VARIANT start,VARIANT* result) override{return guarded([&]()->HRESULT{
        if(!result)return E_POINTER;VariantInit(result);return with(start,[&](const Published& current,std::size_t index)->HRESULT{
            const auto& tree=current.tree;std::optional<std::size_t> target;
            if(direction==NAVDIR_FIRSTCHILD||direction==NAVDIR_LASTCHILD){const auto& items=children(tree,index);if(!items.empty())target=direction==NAVDIR_FIRSTCHILD?items.front():items.back();}
            else if((direction==NAVDIR_NEXT||direction==NAVDIR_PREVIOUS)&&index!=root_index){const auto parent=tree.node(index).parent;const auto& items=children(tree,parent.value_or(root_index));
                auto found=std::find(items.begin(),items.end(),index);if(found!=items.end()){
                    if(direction==NAVDIR_NEXT&&std::next(found)!=items.end())target=*std::next(found);
                    if(direction==NAVDIR_PREVIOUS&&found!=items.begin())target=*std::prev(found);}}
            return target?dispatch(current,*target,result):S_FALSE;});
    });}
    HRESULT STDMETHODCALLTYPE accHitTest(LONG x,LONG y,VARIANT* result) override{return guarded([&]()->HRESULT{
        if(!result)return E_POINTER;VariantInit(result);auto current=snapshot();if(!current)return CO_E_OBJNOTCONNECTED;
        const auto local_x=double(x)-current->origin.x,local_y=double(y)-current->origin.y;
        if(local_x<0||local_y<0||local_x>=current->client.right-current->client.left||local_y>=current->client.bottom-current->client.top)return S_FALSE;
        const auto owner=self(current->tree);const auto hit=current->tree.hit(local_x/current->scale,local_y/current->scale);
        if(!hit&&*owner==root_index){result->vt=VT_I4;result->lVal=CHILDID_SELF;return S_OK;}
        if(!hit||!descendant(current->tree,*hit,*owner))return S_FALSE;
        if(*hit==*owner){result->vt=VT_I4;result->lVal=CHILDID_SELF;return S_OK;}return dispatch(*current,*hit,result);
    });}
    HRESULT STDMETHODCALLTYPE accDoDefaultAction(VARIANT child) override{return guarded([&]()->HRESULT{
        return with(child,[&](const Published& current,std::size_t index)->HRESULT{
            if(index==root_index||!current.tree.node(index).actionable)return E_INVALIDARG;
            const auto& id=current.tree.node(index).snapshot.id;const auto object=current.object_ids.find(id);
            if(object==current.object_ids.end())return CO_E_OBJNOTCONNECTED;
            return state_->enqueue(id,object->second,WidgetInputKind::activate)?S_OK:CO_E_OBJNOTCONNECTED;});
    });}
    HRESULT STDMETHODCALLTYPE put_accName(VARIANT,BSTR) override{return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE put_accValue(VARIANT child,BSTR value) override{return guarded([&]()->HRESULT{
        return with(child,[&](const Published& current,std::size_t index)->HRESULT{
            if(index==root_index)return DISP_E_MEMBERNOTFOUND;
            const auto& node=current.tree.node(index);const auto& snapshot=node.snapshot;
            if(node.role!=Win32AccessibilityRole::input||!snapshot.text_input||!snapshot.text_revision)return DISP_E_MEMBERNOTFOUND;
            if(!snapshot.enabled||snapshot.read_only||snapshot.password)return E_ACCESSDENIED;
            auto text=utf8_value(value);if(!text)return E_INVALIDARG;
            const auto object=current.object_ids.find(snapshot.id);if(object==current.object_ids.end())return CO_E_OBJNOTCONNECTED;
            return state_->set_text(snapshot.id,object->second,*snapshot.text_revision,std::move(*text));
        });
    });}
};
}

WindowsAccessibility::WindowsAccessibility(void* window,const WidgetSurfaceCallbacks& callbacks){
    const auto result=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED|COINIT_DISABLE_OLE1DDE);
    uninitialize_=SUCCEEDED(result);
    if(!uninitialize_&&result!=RPC_E_CHANGED_MODE)return;
    try{state_=std::make_shared<WindowsAccessibilityState>(static_cast<HWND>(window),callbacks);}
    catch(...){if(uninitialize_)CoUninitialize();uninitialize_=false;throw;}
}
WindowsAccessibility::~WindowsAccessibility(){retire();if(uninitialize_)CoUninitialize();}
void WindowsAccessibility::retire() noexcept{try{auto state=state_;if(state)state->retire();}catch(...){}}
void WindowsAccessibility::refresh() noexcept{try{auto state=state_;if(state)state->refresh();}catch(...){}}
void WindowsAccessibility::structure_changed() noexcept{
    try{auto state=state_;if(!state)return;state->refresh();if(state->live.load())NotifyWinEvent(EVENT_OBJECT_REORDER,state->window,OBJID_CLIENT,CHILDID_SELF);}catch(...){}
}
void WindowsAccessibility::value_changed(std::string_view id) noexcept{
    try{auto state=state_;if(!state)return;state->refresh();if(!state->live.load())return;
        auto current=state->load();if(!current)return;auto object=current->object_ids.find(std::string(id));
        NotifyWinEvent(EVENT_OBJECT_VALUECHANGE,state->window,object==current->object_ids.end()?OBJID_CLIENT:LONG(object->second),CHILDID_SELF);
    }catch(...){}
}
bool WindowsAccessibility::input(const void* raw) noexcept{try{auto state=state_;return state&&state->input(*static_cast<const MSG*>(raw));}catch(...){return true;}}
std::optional<std::intptr_t> WindowsAccessibility::result(const void* raw) noexcept{
    try{const auto& event=*static_cast<const MSG*>(raw);if(event.message!=WM_GETOBJECT)return {};
        const auto object=DWORD(event.lParam);if(object!=DWORD(OBJID_CLIENT)&&(!object||object>DWORD(std::numeric_limits<LONG>::max())))return {};
        auto state=state_;if(!state||!state->live.load())return std::intptr_t{0};state->refresh();
        if(!state->live.load())return std::intptr_t{0};auto current=state->load();if(!current)return std::intptr_t{0};
        std::string id;if(object!=DWORD(OBJID_CLIENT)){auto found=current->objects.find(object);if(found==current->objects.end())return {};id=found->second;}
        auto provider=new Accessible(state,std::move(id),object==DWORD(OBJID_CLIENT)?0:object);const auto result=LresultFromObject(__uuidof(IAccessible),event.wParam,provider);provider->Release();return std::intptr_t(result);
    }catch(...){return std::intptr_t{0};}
}
} // namespace ii::gui::detail
