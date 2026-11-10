#include <windows.h>
#include <oleacc.h>
#include "../src/Widgets/WindowsAccessibility.h"
#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <cstring>
#include <functional>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace {
std::mutex fake_mutex;std::condition_variable posted_changed;std::vector<MSG> posted;struct Event{DWORD type;LONG object;};std::vector<Event> events;
std::mutex bstr_mutex;std::unordered_map<BSTR,UINT> bstr_lengths;
std::function<void()> notification,parent_callback,focus_callback;HRESULT initialize_result=S_OK;unsigned uninitialized=0;
void check(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
VARIANT child(LONG id){VARIANT result{};result.vt=VT_I4;result.lVal=id;return result;}
ii::gui::WidgetSnapshot widget(std::string id,ii::gui::WidgetKind kind,ii::gui::Rect rect){
    ii::gui::WidgetSnapshot value{};value.id=std::move(id);value.kind=kind;value.attached=true;value.bounds=value.visual_bounds=rect;return value;
}
}
HRESULT CoInitializeEx(void*,DWORD){return initialize_result;}void CoUninitialize(){++uninitialized;}
BSTR SysAllocStringLen(const wchar_t* value,UINT size){auto out=new(std::nothrow) wchar_t[size+1];if(!out)return nullptr;if(value)std::copy_n(value,size,out);out[size]=0;{std::lock_guard lock(bstr_mutex);bstr_lengths[out]=size;}return out;}
UINT SysStringLen(BSTR value){if(!value)return 0;std::lock_guard lock(bstr_mutex);return bstr_lengths.at(value);}
void SysFreeString(BSTR value){if(!value)return;{std::lock_guard lock(bstr_mutex);bstr_lengths.erase(value);}delete[] value;}
int MultiByteToWideChar(UINT,DWORD,const char* text,int length,wchar_t* out,int capacity){
    if(!out)return length;if(capacity<length)return 0;for(int i=0;i<length;++i)out[i]=static_cast<unsigned char>(text[i]);return length;
}
BOOL GetClientRect(HWND,RECT* out){*out={0,0,600,400};return 1;}BOOL ClientToScreen(HWND,POINT* out){*out={100,200};return 1;}
UINT GetDpiForWindow(HWND){return 192;}int GetWindowTextLengthW(HWND){return 8;}
int GetWindowTextW(HWND,wchar_t* out,int size){const wchar_t* name=L"Document";if(size<9)return 0;std::copy_n(name,9,out);return 8;}
BOOL PostMessageW(HWND window,UINT type,WPARAM w,LPARAM l){std::lock_guard lock(fake_mutex);posted.push_back({window,type,w,l,0,{}});posted_changed.notify_all();return 1;}
HWND SetFocus(HWND value){auto hook=focus_callback;if(hook)hook();return value;}
void NotifyWinEvent(DWORD type,HWND,LONG object,LONG){events.push_back({type,object});auto hook=notification;if(hook)hook();}
LRESULT LresultFromObject(REFIID,WPARAM,IUnknown* object){object->AddRef();return reinterpret_cast<LRESULT>(object);}
HRESULT AccessibleObjectFromWindow(HWND,DWORD,REFIID,void** result){*result=nullptr;auto hook=parent_callback;if(hook)hook();return E_NOTIMPL;}

void reused_widget_identity(){
    using namespace ii::gui;using namespace ii::gui::detail;
    posted.clear();events.clear();auto button=widget("same-id",WidgetKind::button,{0,0,40,20});button.text="Original";button.focused=true;
    std::vector<WidgetSnapshot> nodes{button};unsigned actions=0;WidgetSurfaceCallbacks callbacks;
    callbacks.accessibility=[&]{return nodes;};callbacks.input=[&](WidgetInput input){if(input.target=="same-id")++actions;};
    auto bridge=std::make_unique<WindowsAccessibility>(reinterpret_cast<void*>(1),callbacks);bridge->refresh();
    MSG request{reinterpret_cast<void*>(1),WM_GETOBJECT,0,OBJID_CLIENT,0,{}};auto response=bridge->result(&request);
    check(response&&*response,"Cannot obtain identity test root");auto* root=reinterpret_cast<IAccessible*>(*response);
    IDispatch* old_dispatch=nullptr;check(root->get_accChild(child(1),&old_dispatch)==S_OK&&old_dispatch,"Cannot obtain original peer");
    VARIANT focus{},hit{},navigation{};
    check(root->get_accFocus(&focus)==S_OK&&focus.vt==VT_DISPATCH&&focus.pdispVal,"Cannot obtain focused peer");
    check(root->accHitTest(110,210,&hit)==S_OK&&hit.vt==VT_DISPATCH&&hit.pdispVal,"Cannot obtain hit-test peer");
    check(root->accNavigate(NAVDIR_FIRSTCHILD,child(0),&navigation)==S_OK&&navigation.vt==VT_DISPATCH&&navigation.pdispVal,"Cannot obtain navigated peer");
    auto* old=static_cast<IAccessible*>(old_dispatch);std::atomic<HRESULT> submitted{E_FAIL};
    std::atomic<HRESULT> selected{E_FAIL};std::thread worker([&]{submitted=old->accDoDefaultAction(child(0));selected=old->accSelect(SELFLAG_TAKEFOCUS,child(0));});worker.join();
    check(submitted==S_OK&&selected==S_OK&&posted.size()>=2,"Cannot queue original actions");const auto pending=posted[posted.size()-2],pending_focus=posted.back();
    nodes.clear();bridge->refresh();button.text="Replacement";nodes.push_back(button);bridge->refresh();
    check(bridge->input(&pending)&&actions==0,"A queued action reached a new widget with the same ID");
    check(bridge->input(&pending_focus)&&actions==0,"A queued focus reached a new widget with the same ID");
    BSTR name=nullptr;check(old->get_accName(child(0),&name)==CO_E_OBJNOTCONNECTED&&!name,"An old peer resolved a new widget with the same ID");
    check(old->accDoDefaultAction(child(0))==CO_E_OBJNOTCONNECTED,"A retired peer accepted a new action");
    for(auto* peer:{focus.pdispVal,hit.pdispVal,navigation.pdispVal}){
        name=nullptr;check(static_cast<IAccessible*>(peer)->get_accName(child(0),&name)==CO_E_OBJNOTCONNECTED&&!name,"A derived peer resolved a replacement widget");peer->Release();}
    IDispatch* fresh_dispatch=nullptr;check(root->get_accChild(child(1),&fresh_dispatch)==S_OK&&fresh_dispatch,"Cannot obtain replacement peer");
    auto* fresh=static_cast<IAccessible*>(fresh_dispatch);check(fresh->get_accName(child(0),&name)==S_OK&&std::wstring(name)==L"Replacement","Replacement peer did not resolve the new widget");SysFreeString(name);
    std::thread fresh_worker([&]{submitted=fresh->accDoDefaultAction(child(0));});fresh_worker.join();
    check(submitted==S_OK&&bridge->input(&posted.back())&&actions==1,"A current peer could not activate the replacement");
    std::thread focus_worker([&]{selected=fresh->accSelect(SELFLAG_TAKEFOCUS,child(0));});focus_worker.join();
    check(selected==S_OK,"Cannot queue a live focus request");const auto reentrant_focus=posted.back();
    focus_callback=[&]{nodes.clear();bridge->refresh();nodes.push_back(button);bridge->refresh();};
    check(bridge->input(&reentrant_focus)&&actions==1,"Focus reentry delivered input to a newer widget");focus_callback={};
    IDispatch* latest_dispatch=nullptr;check(root->get_accChild(child(1),&latest_dispatch)==S_OK&&latest_dispatch,"Cannot obtain newest peer");
    auto* latest=static_cast<IAccessible*>(latest_dispatch);
    std::thread retiring_worker([&]{selected=latest->accSelect(SELFLAG_TAKEFOCUS,child(0));});retiring_worker.join();
    check(selected==S_OK,"Cannot queue focus before retirement");const auto retiring_focus=posted.back();auto* raw=bridge.get();
    focus_callback=[&]{bridge.reset();};check(raw->input(&retiring_focus)&&!bridge&&actions==1,"Focus reentry survived bridge retirement");focus_callback={};
    name=nullptr;check(latest->get_accName(child(0),&name)==CO_E_OBJNOTCONNECTED&&!name,"Retired bridge kept its newest peer live");
    latest->Release();fresh->Release();old->Release();root->Release();
}

int main(){
    using namespace ii;using namespace ii::gui;using namespace ii::gui::detail;
    const auto owner=std::this_thread::get_id();auto root_node=widget("layout",WidgetKind::vertical_stack,{0,0,300,200});
    auto button=widget("button",WidgetKind::button,{10,20,30,10});button.parent_id="layout";button.text="Save";button.focused=true;
    auto field=widget("field",WidgetKind::input_field,{0,50,100,20});field.parent_id="layout";field.text_input=true;field.text_length=3;
    field.text_revision=TextInputRevision{.identity=std::make_shared<int>(1),.owner=1,.value=1,.focus=1};
    auto password=widget("password",WidgetKind::input_field,{0,80,100,20});password.parent_id="layout";password.text_input=true;password.password=true;
    password.text="secret";password.text_length=6;password.text_revision=field.text_revision;password.placeholder="Password";
    std::vector<WidgetSnapshot> nodes{root_node,button,field,password};unsigned reads=0,actions=0,edits=0;std::string field_text="abc";
    WidgetSurfaceCallbacks callbacks;
    callbacks.accessibility=[&]{check(std::this_thread::get_id()==owner,"Snapshot query ran off the owner thread");return nodes;};
    callbacks.lookup=[&](std::string_view id){check(std::this_thread::get_id()==owner,"Lookup ran off the owner thread");
        auto found=std::ranges::find_if(nodes,[&](const auto& n){return n.id==id;});return found==nodes.end()?WidgetSnapshot{}:*found;};
    callbacks.text_content=[&](std::string_view id,TextRange range)->core::Result<TextContent>{
        check(std::this_thread::get_id()==owner&&id=="field","Password or background text was read");++reads;return TextContent{range,field_text};};
    callbacks.input=[&](WidgetInput event){check(std::this_thread::get_id()==owner&&event.target=="replacement","An action was delivered on the wrong thread or to a retired target");++actions;};
    callbacks.accessibility_text=[&](std::string_view id,const TextInputRevision& revision,std::string text)->core::Result<void>{
        check(std::this_thread::get_id()==owner&&id=="field","Accessibility text ran off owner or reached the wrong target");
        auto found=std::ranges::find_if(nodes,[&](const auto& node){return node.id==id;});
        check(found!=nodes.end()&&found->text_revision==revision,"Accessibility text used a stale revision");
        field_text=std::move(text);found->text_length=field_text=="한😀"?3:field_text.size();++found->text_revision->owner;++edits;return {};
    };
    auto bridge=std::make_unique<WindowsAccessibility>(reinterpret_cast<void*>(1),callbacks);bridge->refresh();
    check(reads==1&&!events.empty()&&events.back().type==EVENT_OBJECT_FOCUS&&events.back().object>0,"Focus event was not published");
    const auto focus_object=events.back().object;
    MSG request{reinterpret_cast<void*>(1),WM_GETOBJECT,0,OBJID_CLIENT,0,{}};auto response=bridge->result(&request);
    check(response&&*response,"WM_GETOBJECT did not return a root provider");auto* accessible=reinterpret_cast<IAccessible*>(*response);
    LONG count=0;check(accessible->get_accChildCount(&count)==S_OK&&count==3,"Wrong root child count");
    IDispatch* button_dispatch=nullptr;check(accessible->get_accChild(child(1),&button_dispatch)==S_OK&&button_dispatch,"Missing button peer");
    auto* button_peer=static_cast<IAccessible*>(button_dispatch);BSTR name=nullptr;
    check(button_peer->get_accName(child(0),&name)==S_OK&&std::wstring(name)==L"Save","Button name is incorrect");SysFreeString(name);
    VARIANT focus{};check(accessible->get_accFocus(&focus)==S_OK&&focus.vt==VT_DISPATCH&&focus.pdispVal,"Focused element is missing");focus.pdispVal->Release();
    VARIANT hit{};check(accessible->accHitTest(121,241,&hit)==S_OK&&hit.vt==VT_DISPATCH&&hit.pdispVal,"Hit testing did not find the button");hit.pdispVal->Release();
    name=nullptr;check(accessible->get_accName(child(99),&name)==E_INVALIDARG&&!name,"An invalid child ID was accepted");
    LONG x=0,y=0,w=0,h=0;check(button_peer->accLocation(&x,&y,&w,&h,child(0))==S_OK&&x==120&&y==240&&w==60&&h==20,"DPI-adjusted screen coordinates are incorrect");
    IDispatch* field_dispatch=nullptr;check(accessible->get_accChild(child(2),&field_dispatch)==S_OK,"Missing field peer");
    auto* field_peer=static_cast<IAccessible*>(field_dispatch);BSTR value=nullptr;
    check(field_peer->get_accValue(child(0),&value)==S_OK&&std::wstring(value)==L"abc","Custom text provider was not published");SysFreeString(value);
    check(field_peer->put_accValue(child(99),nullptr)==E_INVALIDARG,"Text value setter accepted an invalid child");
    const wchar_t unicode[]{wchar_t(0xd55c),wchar_t(0xd83d),wchar_t(0xde00)};
    auto input=SysAllocStringLen(unicode,3);check(input,"Cannot allocate test value");
    check(field_peer->put_accValue(child(0),input)==S_OK&&field_text=="한😀"&&edits==1,"UTF-16 accessibility value did not reach the editor");SysFreeString(input);bridge->refresh();
    const wchar_t nul[]{L'A',0,L'B'};input=SysAllocStringLen(nul,3);
    check(field_peer->put_accValue(child(0),input)==E_INVALIDARG&&edits==1,"Embedded NUL changed the editor");SysFreeString(input);
    const wchar_t broken[]{wchar_t(0xd800)};input=SysAllocStringLen(broken,1);
    check(field_peer->put_accValue(child(0),input)==E_INVALIDARG&&edits==1,"Unpaired surrogate changed the editor");SysFreeString(input);
    std::vector<wchar_t> oversized(65537,L'x');input=SysAllocStringLen(oversized.data(),UINT(oversized.size()));
    check(field_peer->put_accValue(child(0),input)==E_INVALIDARG&&edits==1,"Oversized text changed the editor");SysFreeString(input);
    nodes[2].read_only=true;bridge->refresh();
    check(field_peer->put_accValue(child(0),nullptr)==E_ACCESSDENIED&&edits==1,"Read-only editor accepted replacement");
    nodes[2].read_only=false;bridge->refresh();
    check(button_peer->put_accValue(child(0),nullptr)==DISP_E_MEMBERNOTFOUND,"A button accepted text replacement");
    auto queue_value=[&](std::wstring text,std::atomic<HRESULT>& status){
        std::size_t count;{std::lock_guard lock(fake_mutex);count=posted.size();}
        auto thread=std::thread([&,text=std::move(text)]{auto value=SysAllocStringLen(text.data(),UINT(text.size()));status=field_peer->put_accValue(child(0),value);SysFreeString(value);});
        MSG message{};{std::unique_lock lock(fake_mutex);check(posted_changed.wait_for(lock,std::chrono::seconds(3),[&]{return posted.size()>count;}),"Value request was not posted");message=posted.back();}
        return std::pair{std::move(thread),message};
    };
    std::atomic<HRESULT> value_status{E_FAIL};auto [value_worker,value_message]=queue_value(L"queued",value_status);
    check(edits==1&&bridge->input(&value_message),"Queued value was not delivered");value_worker.join();
    check(value_status==S_OK&&field_text=="queued"&&edits==2,"Queued value did not return the applied result");bridge->refresh();
    auto [stale_worker,stale_message]=queue_value(L"stale",value_status);
    ++nodes[2].text_revision->owner;bridge->refresh();check(bridge->input(&stale_message),"Stale value message was not consumed");stale_worker.join();
    check(value_status==E_FAIL&&field_text=="queued"&&edits==2,"A stale value replaced newer text");
    auto [timeout_worker,timeout_message]=queue_value(L"timed out",value_status);timeout_worker.join();
    check(value_status==E_FAIL&&bridge->input(&timeout_message)&&edits==2,"A timed-out value was applied later");
    IDispatch* secret_dispatch=nullptr;check(accessible->get_accChild(child(3),&secret_dispatch)==S_OK,"Missing password peer");
    auto* secret_peer=static_cast<IAccessible*>(secret_dispatch);value=nullptr;
    check(secret_peer->get_accValue(child(0),&value)==E_ACCESSDENIED&&!value,"Password value was exposed");
    check(secret_peer->put_accValue(child(0),nullptr)==E_ACCESSDENIED,"Password value accepted accessibility replacement");
    VARIANT flags{};check(secret_peer->get_accState(child(0),&flags)==S_OK&&(flags.lVal&STATE_SYSTEM_PROTECTED),"Password protection state is missing");
    MSG focus_request{reinterpret_cast<void*>(1),WM_GETOBJECT,0,focus_object,0,{}};auto focused=bridge->result(&focus_request);
    check(focused&&*focused,"Focus event did not resolve to its native object");reinterpret_cast<IAccessible*>(*focused)->Release();
    std::atomic<HRESULT> submitted{E_FAIL};std::thread worker([&]{submitted=button_peer->accDoDefaultAction(child(0));});worker.join();
    check(submitted==S_OK&&!posted.empty()&&actions==0,"Cross-thread action was not queued");const auto stale=posted.back();
    nodes.erase(nodes.begin()+1);auto replacement=button;replacement.id="replacement";replacement.text="Open";nodes.insert(nodes.begin()+1,replacement);
    bridge->refresh();check(bridge->input(&stale)&&actions==0,"A stale action reached the replacement");
    name=nullptr;check(button_peer->get_accName(child(0),&name)==CO_E_OBJNOTCONNECTED&&!name,"Retired peer remained connected");
    focused=bridge->result(&focus_request);check(!focused||!*focused,"Retired event ID addressed a replacement");
    IDispatch* replacement_dispatch=nullptr;check(accessible->get_accChild(child(1),&replacement_dispatch)==S_OK,"Missing replacement peer");
    auto* replacement_peer=static_cast<IAccessible*>(replacement_dispatch);
    std::thread live_worker([&]{submitted=replacement_peer->accDoDefaultAction(child(0));});live_worker.join();
    check(submitted==S_OK&&bridge->input(&posted.back())&&actions==1,"Live action was not dispatched on the owner thread");
    bridge->value_changed("field");check(events.back().type==EVENT_OBJECT_VALUECHANGE&&events.back().object>0,"Value event did not identify its peer");
    auto [replaced_worker,replaced_message]=queue_value(L"old peer",value_status);
    auto replaced_field=nodes[2];nodes.erase(nodes.begin()+2);bridge->refresh();
    nodes.insert(nodes.begin()+2,replaced_field);bridge->refresh();
    check(bridge->input(&replaced_message),"Replaced field message was not consumed");replaced_worker.join();
    check(value_status==CO_E_OBJNOTCONNECTED&&edits==2,"A retired value request reached the replacement with the same ID");
    field_peer->Release();field_dispatch=nullptr;
    check(accessible->get_accChild(child(2),&field_dispatch)==S_OK&&field_dispatch,"Missing replacement field peer");
    field_peer=static_cast<IAccessible*>(field_dispatch);
    auto [retired_worker,retired_message]=queue_value(L"retired",value_status);
    bridge->retire();retired_worker.join();
    check(value_status==CO_E_OBJNOTCONNECTED&&bridge->input(&retired_message)&&edits==2,"A retired value request remained active");
    name=nullptr;check(replacement_peer->get_accName(child(0),&name)==CO_E_OBJNOTCONNECTED,"A retired surface remained accessible");
    const auto before=events.size();bridge->structure_changed();check(events.size()==before,"A retired surface emitted an event");bridge.reset();
    replacement_peer->Release();secret_peer->Release();field_peer->Release();button_peer->Release();accessible->Release();
    check(uninitialized==1,"COM apartment was not balanced");
    initialize_result=RPC_E_CHANGED_MODE;auto existing=std::make_unique<WindowsAccessibility>(reinterpret_cast<void*>(1),callbacks);existing->refresh();
    auto parent_response=existing->result(&request);check(parent_response&&*parent_response,"Cannot create a root in an existing COM apartment");
    auto* root_peer=reinterpret_cast<IAccessible*>(*parent_response);parent_callback=[&]{existing.reset();};IDispatch* parent=nullptr;
    check(root_peer->get_accParent(&parent)==CO_E_OBJNOTCONNECTED&&!parent,"Parent lookup survived reentrant surface retirement");
    parent_callback={};root_peer->Release();
    check(uninitialized==1,"An existing COM apartment was uninitialized");
    initialize_result=S_OK;nodes[1].focused=true;auto reentrant=std::make_unique<WindowsAccessibility>(reinterpret_cast<void*>(1),callbacks);
    notification=[&]{reentrant.reset();};reentrant->refresh();notification={};check(!reentrant,"Reentrant surface retirement did not finish");
    reused_widget_identity();
}
