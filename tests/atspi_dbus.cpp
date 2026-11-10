#include "../src/Widgets/AtSpiApplication.h"
#include "../src/Widgets/AtSpiDbus.h"
#include "../src/Widgets/AtSpiSession.h"
#include "../src/Widgets/WidgetState.h"
#include <gio/gio.h>
#include <sys/socket.h>
#include <algorithm>
#include <chrono>
#include <atomic>
#include <memory>
#include <limits>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>

using namespace ii::gui;
using namespace ii::gui::detail;
namespace {
void check(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
struct Variant final {
    GVariant* value=nullptr;
    explicit Variant(GVariant* p=nullptr):value(p){}
    ~Variant(){if(value)g_variant_unref(value);}
    Variant(const Variant&)=delete;Variant& operator=(const Variant&)=delete;
    Variant(Variant&& other)noexcept:value(std::exchange(other.value,nullptr)){}
};
struct Pending final {GVariant* result=nullptr;GError* error=nullptr;bool done=false;};
void complete(GObject* object,GAsyncResult* result,gpointer data){
    auto& pending=*static_cast<Pending*>(data);
    pending.result=g_dbus_connection_call_finish(G_DBUS_CONNECTION(object),result,&pending.error);pending.done=true;
}
void wait(GMainContext* context,const bool& done,const std::function<void()>& extra={}){
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
    while(!done&&std::chrono::steady_clock::now()<deadline){
        if(extra)extra();while(g_main_context_pending(context))g_main_context_iteration(context,FALSE);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    check(done,"D-Bus test timed out");
}
struct Connected final {GDBusConnection* connection=nullptr;GError* error=nullptr;bool done=false;};
void connected(GObject*,GAsyncResult* result,gpointer data){
    auto& state=*static_cast<Connected*>(data);state.connection=g_dbus_connection_new_finish(result,&state.error);state.done=true;
}
struct Peer final {
    GMainContext* context=g_main_context_new();GIOStream* streams[2]{};GDBusConnection* server=nullptr;GDBusConnection* client=nullptr;std::function<void()> extra;
    Peer(){
        g_main_context_push_thread_default(context);int fds[2]{};check(socketpair(AF_UNIX,SOCK_STREAM,0,fds)==0,"Cannot create test transport");
        for(int i=0;i<2;++i){GError* error=nullptr;auto* socket=g_socket_new_from_fd(fds[i],&error);check(socket&&!error,"Cannot adopt socket");
            streams[i]=G_IO_STREAM(g_socket_connection_factory_create_connection(socket));g_object_unref(socket);}
        auto* guid=g_dbus_generate_guid();Connected left,right;
        g_dbus_connection_new(streams[0],guid,GDBusConnectionFlags(G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_SERVER|G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_ALLOW_ANONYMOUS),nullptr,nullptr,connected,&left);
        g_dbus_connection_new(streams[1],nullptr,G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT,nullptr,nullptr,connected,&right);g_free(guid);
        wait(context,left.done);wait(context,right.done);check(left.connection&&right.connection&&!left.error&&!right.error,"Cannot authenticate test peers");
        server=left.connection;client=right.connection;g_dbus_connection_set_exit_on_close(server,FALSE);g_dbus_connection_set_exit_on_close(client,FALSE);
    }
    ~Peer(){if(client){g_dbus_connection_close_sync(client,nullptr,nullptr);g_object_unref(client);}if(server){g_dbus_connection_close_sync(server,nullptr,nullptr);g_object_unref(server);}
        for(auto* stream:streams)if(stream)g_object_unref(stream);g_main_context_pop_thread_default(context);g_main_context_unref(context);}
    Variant call(const std::string& path,const char* interface,const char* method,GVariant* params=nullptr,bool succeeds=true){
        Pending pending;g_dbus_connection_call(client,nullptr,path.c_str(),interface,method,params,nullptr,G_DBUS_CALL_FLAGS_NONE,3000,nullptr,complete,&pending);
        wait(context,pending.done,extra);const bool ok=pending.result!=nullptr;
        if(pending.error)g_error_free(pending.error);check(ok==succeeds,"Unexpected D-Bus reply status");return Variant(pending.result);
    }
    Variant property(const std::string& path,const char* iface,const char* name){
        auto response=call(path,"org.freedesktop.DBus.Properties","Get",g_variant_new("(ss)",iface,name));
        GVariant* value=nullptr;g_variant_get(response.value,"(v)",&value);return Variant(value);
    }
};
WidgetSnapshot node(std::string id,std::string parent,WidgetKind kind){WidgetSnapshot result{};result.id=std::move(id);result.parent_id=std::move(parent);result.kind=kind;result.attached=true;result.bounds=result.visual_bounds={5,8,60,20};return result;}
struct Events final {int focused=0,blurred=0,added=0,cached=0,removed=0;bool renamed=false,done=false,removed_done=false,editable_done=false,value_changed=false;double value=0;std::vector<std::string> added_paths,removed_paths,editable_paths;};
void signal(GDBusConnection*,const gchar*,const gchar* path,const gchar*,const gchar* name,GVariant* values,gpointer data){
    auto& events=*static_cast<Events*>(data);const gchar* detail=nullptr;gint first=0,second=0;GVariant* payload=nullptr;GVariant* properties=nullptr;
    g_variant_get(values,"(&sii@v@a{sv})",&detail,&first,&second,&payload,&properties);
    if(std::string_view(name)=="StateChanged"&&std::string_view(detail)=="focused"){if(first)++events.focused;else ++events.blurred;}
    if(std::string_view(name)=="StateChanged"&&std::string_view(detail)=="editable"&&!first){events.editable_paths.emplace_back(path);events.editable_done=events.editable_paths.size()>=2;}
    if(std::string_view(name)=="PropertyChange"&&std::string_view(detail)=="accessible-value"){
        auto* number=g_variant_get_variant(payload);if(g_variant_is_of_type(number,G_VARIANT_TYPE_DOUBLE)){events.value=g_variant_get_double(number);events.value_changed=true;}g_variant_unref(number);
    }
    if(std::string_view(name)=="ChildrenChanged"&&std::string_view(detail)=="add")++events.added;
    if(std::string_view(name)=="PropertyChange"&&std::string_view(detail)=="accessible-name")events.renamed=true;
    g_variant_unref(payload);g_variant_unref(properties);events.done=events.focused>0&&events.blurred>0&&events.added>0&&events.cached>=2;
}
void value_protocol(){
    Peer peer;AtSpiApplication app("Editor");auto window=app.open_window("Document",{100,100});check(bool(window),"Cannot open value window");
    auto slider=node("slider","",WidgetKind::action_surface);slider.accessibility_semantics=AccessibilitySemantics{.role=AccessibilityRole::slider,.range=AccessibilityRange{-5,5,2}};
    auto progress=node("progress","",WidgetKind::action_surface);progress.accessibility_semantics=AccessibilitySemantics{.role=AccessibilityRole::progress,.range=AccessibilityRange{0,1,.25}};
    auto surface=app.stage(*window,{slider,progress});check(surface&&app.commit(*surface),"Cannot publish value surface");
    std::string requested;double delivered=0;int calls=0;
    AtSpiDbusBridge bridge(app,{}, {},[&](std::string path,double value){requested=std::move(path);delivered=value;++calls;return true;});
    check(bridge.attach(peer.server,":1.144"),"Cannot attach value bridge");const auto frame=AtSpiApplication::window_path(*window);
    const auto path=app.object(frame)->children.at(0),bar=app.object(frame)->children.at(1);
    auto minimum=peer.property(path,"org.a11y.atspi.Value","MinimumValue"),maximum=peer.property(path,"org.a11y.atspi.Value","MaximumValue"),current=peer.property(path,"org.a11y.atspi.Value","CurrentValue");
    check(g_variant_get_double(minimum.value)==-5&&g_variant_get_double(maximum.value)==5&&g_variant_get_double(current.value)==2,"Numeric range was not exposed");
    auto read=peer.property(bar,"org.a11y.atspi.Value","CurrentValue");check(g_variant_get_double(read.value)==.25,"Progress value was not exposed");
    auto increment=peer.property(path,"org.a11y.atspi.Value","MinimumIncrement");check(g_variant_get_double(increment.value)==0,"Unknown minimum increment was not reported as unknown");
    peer.call(path,"org.freedesktop.DBus.Properties","Set",g_variant_new("(ssv)","org.a11y.atspi.Value","CurrentValue",g_variant_new_double(3.5)));
    check(calls==1&&requested==path&&delivered==3.5,"Value request did not reach its owner");
    peer.call(path,"org.freedesktop.DBus.Properties","Set",g_variant_new("(ssv)","org.a11y.atspi.Value","CurrentValue",g_variant_new_double(6)),false);
    peer.call(path,"org.freedesktop.DBus.Properties","Set",g_variant_new("(ssv)","org.a11y.atspi.Value","CurrentValue",g_variant_new_double(std::numeric_limits<double>::quiet_NaN())),false);
    peer.call(bar,"org.freedesktop.DBus.Properties","Set",g_variant_new("(ssv)","org.a11y.atspi.Value","CurrentValue",g_variant_new_double(.5)),false);
    check(calls==1,"Invalid or read-only value reached its owner");
    Events events;const auto subscription=g_dbus_connection_signal_subscribe(peer.client,nullptr,"org.a11y.atspi.Event.Object",nullptr,nullptr,nullptr,G_DBUS_SIGNAL_FLAGS_NONE,signal,&events,nullptr);
    slider.accessibility_semantics->range->value=4;check(app.update(*surface,{slider,progress}),"Cannot update numeric range");bridge.changed();wait(peer.context,events.value_changed);
    check(events.value==4,"Changed numeric value was not published");g_dbus_connection_signal_unsubscribe(peer.client,subscription);bridge.detach();
}
void cache_signal(GDBusConnection*,const gchar*,const gchar*,const gchar*,const gchar* name,GVariant* values,gpointer data){
    auto& events=*static_cast<Events*>(data);const gchar* bus=nullptr;const gchar* path=nullptr;
    if(std::string_view(name)=="AddAccessible"){
        auto* item=g_variant_get_child_value(values,0);g_variant_get_child(item,0,"(&s&o)",&bus,&path);events.added_paths.emplace_back(path);g_variant_unref(item);++events.cached;
    }
    if(std::string_view(name)=="RemoveAccessible"){g_variant_get(values,"((&s&o))",&bus,&path);events.removed_paths.emplace_back(path);++events.removed;}
    events.done=events.focused>0&&events.blurred>0&&events.added>0&&events.cached>=2;events.removed_done=events.removed>=2;
}
void protocol(){
    Peer peer;AtSpiApplication app("Editor");auto window=app.open_window("Canvas",{320,200});check(window.has_value(),"Cannot open window");
    auto layout=node("layout","",WidgetKind::vertical_stack),button=node("save","layout",WidgetKind::button);button.text="Save";button.focused=true;
    auto secret=node("secret","layout",WidgetKind::input_field);secret.text_input=true;secret.password=true;secret.text="private";secret.placeholder="Password";
    auto surface=app.stage(*window,{layout,button,secret});check(surface&&app.commit(*surface),"Cannot publish surface");
    std::string requested;WidgetInputKind request_kind=WidgetInputKind::pointer_move;
    AtSpiDbusBridge bridge(app,[&](std::string path,WidgetInputKind kind){requested=std::move(path);request_kind=kind;return true;});
    check(bridge.attach(peer.server,":1.144"),"Cannot register accessibility objects");
    AtSpiDbusBridge duplicate(app,{});check(!duplicate.attach(peer.server,":1.145"),"A conflicting subtree registration was accepted");
    bridge.registry_parent(":1.5","/org/a11y/atspi/accessible/root");
    const std::string root(AtSpiApplication::root_path()),frame=app.window_path(*window);
    auto role=peer.call(root,"org.a11y.atspi.Accessible","GetRole");guint32 number=0;g_variant_get(role.value,"(u)",&number);check(number==75,"Wrong application role");
    auto toolkit=peer.property(root,"org.a11y.atspi.Application","ToolkitVersion");check(std::string(g_variant_get_string(toolkit.value,nullptr))==IIGUI_EXPECTED_VERSION,"Toolkit version did not follow the build version");
    auto children=peer.call(root,"org.a11y.atspi.Accessible","GetChildren");GVariant* array=nullptr;g_variant_get(children.value,"(@a(so))",&array);Variant children_value(array);
    check(g_variant_n_children(array)==1,"Wrong application child count");const gchar* bus=nullptr;const gchar* child=nullptr;g_variant_get_child(array,0,"(&s&o)",&bus,&child);
    check(std::string(bus)==":1.144"&&std::string(child)==frame,"Wrong application child reference");
    auto bulk=peer.call("/org/a11y/atspi/cache","org.a11y.atspi.Cache","GetItems");GVariant* items=nullptr;
    g_variant_get(bulk.value,"(@a((so)(so)(so)iiassusau))",&items);Variant item_values(items);check(g_variant_n_children(items)==4,"The accessibility cache lost objects");
    auto title=peer.property(frame,"org.a11y.atspi.Accessible","Name");check(std::string(g_variant_get_string(title.value,nullptr))=="Canvas","Wrong window title");
    const auto button_path=app.object(frame)->children.at(0),secret_path=app.object(frame)->children.at(1);
    auto name=peer.property(button_path,"org.a11y.atspi.Accessible","Name");check(std::string(g_variant_get_string(name.value,nullptr))=="Save","Wrong accessible name");
    auto states=peer.call(button_path,"org.a11y.atspi.Accessible","GetState");GVariant* state_array=nullptr;g_variant_get(states.value,"(@au)",&state_array);Variant state_value(state_array);
    gsize count=0;const auto* bits=static_cast<const guint32*>(g_variant_get_fixed_array(state_array,&count,sizeof(guint32)));
    check(count==2&&(bits[0]&(1u<<12))&&(bits[0]&(1u<<8)),"Focused and enabled states were not encoded as bitfields");
    auto secret_role=peer.call(secret_path,"org.a11y.atspi.Accessible","GetRole");g_variant_get(secret_role.value,"(u)",&number);check(number==40,"Password role was lost");
    auto action=peer.call(button_path,"org.a11y.atspi.Action","DoAction",g_variant_new("(i)",0));gboolean accepted=FALSE;g_variant_get(action.value,"(b)",&accepted);
    check(accepted&&requested==button_path&&request_kind==WidgetInputKind::activate,"Action was not admitted for owner-thread delivery");
    Events events;const auto subscription=g_dbus_connection_signal_subscribe(peer.client,nullptr,"org.a11y.atspi.Event.Object",nullptr,nullptr,nullptr,G_DBUS_SIGNAL_FLAGS_NONE,signal,&events,nullptr);
    const auto cache_subscription=g_dbus_connection_signal_subscribe(peer.client,nullptr,"org.a11y.atspi.Cache",nullptr,nullptr,nullptr,G_DBUS_SIGNAL_FLAGS_NONE,cache_signal,&events,nullptr);
    button.focused=false;secret.focused=true;auto extra=node("extra","layout",WidgetKind::action_surface);extra.text="More";auto inner=node("inner","extra",WidgetKind::button);inner.text="Inner";
    check(app.update(*surface,{layout,button,secret,extra,inner}),"Cannot update signal content");bridge.changed();wait(peer.context,events.done);
    const auto extra_path=app.object(frame)->children.at(2),inner_path=app.object(extra_path)->children.at(0);
    check(events.added_paths==std::vector<std::string>({extra_path,inner_path}),"Cache additions were not parent-first");
    auto hit=peer.call(frame,"org.a11y.atspi.Component","GetAccessibleAtPoint",g_variant_new("(iiu)",10,10,1));
    const gchar* hit_bus=nullptr;const gchar* hit_path=nullptr;g_variant_get(hit.value,"((&s&o))",&hit_bus,&hit_path);
    check(std::string(hit_bus)==":1.144"&&std::string(hit_path)==inner_path,"Hit testing did not select the topmost nested object");
    check(app.update(*surface,{layout,button,secret}),"Cannot remove signal content");bridge.changed();wait(peer.context,events.removed_done);
    check(events.removed_paths==std::vector<std::string>({inner_path,extra_path}),"Cache removals were not child-first");
    g_dbus_connection_signal_unsubscribe(peer.client,subscription);g_dbus_connection_signal_unsubscribe(peer.client,cache_subscription);
    peer.call(button_path,"org.a11y.atspi.Component","GetExtents",g_variant_new("(u)",0),false);
    auto extents=peer.call(button_path,"org.a11y.atspi.Component","GetExtents",g_variant_new("(u)",1));gint x=0,y=0,w=0,h=0;g_variant_get(extents.value,"((iiii))",&x,&y,&w,&h);
    check(x==5&&y==8&&w==60&&h==20,"Window-local coordinates were lost");
    peer.call(root,"org.freedesktop.DBus.Properties","Set",g_variant_new("(ssv)","org.a11y.atspi.Application","Id",g_variant_new_int32(42)));
    auto id=peer.property(root,"org.a11y.atspi.Application","Id");check(g_variant_get_int32(id.value)==42,"Registry ID was not retained");
    check(app.retire(*surface),"Cannot retire surface");peer.call(button_path,"org.a11y.atspi.Accessible","GetRole",nullptr,false);
    Pending in_flight;g_dbus_connection_call(peer.client,nullptr,root.c_str(),"org.a11y.atspi.Accessible","GetRole",nullptr,nullptr,G_DBUS_CALL_FLAGS_NONE,3000,nullptr,complete,&in_flight);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));bridge.detach();wait(peer.context,in_flight.done);
    check(!in_flight.result&&in_flight.error,"An in-flight request survived bridge retirement");g_error_free(in_flight.error);
    peer.call(root,"org.a11y.atspi.Accessible","GetRole",nullptr,false);
}
void text_protocol(){
    Peer peer;AtSpiApplication app("Editor");auto window=app.open_window("Document",{100,100});check(bool(window),"Cannot open text window");
    auto identity=std::make_shared<int>(0);auto editor=node("editor","",WidgetKind::input_field);editor.text_input=true;editor.focused=true;
    editor.text_length=4;editor.text_selection={1,2};editor.text_revision=TextInputRevision{identity,1,1,1};editor.placeholder="Editor";
    auto password=node("password","",WidgetKind::input_field);password.text_input=true;password.password=true;password.text="secret";password.text_revision=TextInputRevision{identity,2,1,1};
    auto surface=app.stage(*window,{editor,password});check(surface&&app.commit(*surface),"Cannot publish text surface");
    WidgetInput delivered;std::string delivery_path;int edits=0;
    AtSpiDbusBridge bridge(app,{},AtSpiDbusBridge::TextCallbacks{
        .read=[&](std::string_view path)->std::optional<AtSpiTextData>{auto target=app.text_target(path);if(!target)return std::nullopt;auto index=AtSpiTextIndex::create("A😀B",target->selection,target->reversed);if(!index)return std::nullopt;return AtSpiTextData{*target,std::move(*index)};},
        .input=[&](std::string path,WidgetInput input){delivery_path=std::move(path);delivered=std::move(input);++edits;return true;}});
    check(bridge.attach(peer.server,":1.144"),"Cannot attach text bridge");const auto frame=AtSpiApplication::window_path(*window);
    const auto path=app.object(frame)->children.at(0),secret=app.object(frame)->children.at(1);
    auto count=peer.property(path,"org.a11y.atspi.Text","CharacterCount"),caret=peer.property(path,"org.a11y.atspi.Text","CaretOffset");
    check(g_variant_get_int32(count.value)==3&&g_variant_get_int32(caret.value)==2,"Text properties used UTF-16 offsets");
    auto text=peer.call(path,"org.a11y.atspi.Text","GetText",g_variant_new("(ii)",1,2));const gchar* value=nullptr;g_variant_get(text.value,"(&s)",&value);check(std::string(value)=="😀","GetText split a scalar");
    auto scalar=peer.call(path,"org.a11y.atspi.Text","GetCharacterAtOffset",g_variant_new("(i)",1));gint code=0;g_variant_get(scalar.value,"(i)",&code);check(code==0x1f600,"Wrong Unicode scalar");
    auto character=peer.call(path,"org.a11y.atspi.Text","GetStringAtOffset",g_variant_new("(iu)",1,0));gint first=0,last=0;g_variant_get(character.value,"(&sii)",&value,&first,&last);
    check(std::string(value)=="😀"&&first==1&&last==2,"Character granularity used byte offsets");
    peer.call(path,"org.a11y.atspi.Text","GetStringAtOffset",g_variant_new("(iu)",1,3),false);
    peer.call(path,"org.a11y.atspi.Text","GetStringAtOffset",g_variant_new("(iu)",1,1),false);
    auto selection=peer.call(path,"org.a11y.atspi.Text","GetSelection",g_variant_new("(i)",0));gint start=0,end=0;g_variant_get(selection.value,"(ii)",&start,&end);check(start==1&&end==2,"Wrong text selection offsets");
    auto selected=peer.call(path,"org.a11y.atspi.Text","SetSelection",g_variant_new("(iii)",0,2,3));gboolean accepted=FALSE;g_variant_get(selected.value,"(b)",&accepted);
    check(accepted&&edits==1&&delivery_path==path&&delivered.kind==WidgetInputKind::text_selection&&delivered.replacement_range==TextRange{3,1}&&delivered.expected_text_revision==editor.text_revision,"Selection was not converted and revision guarded");
    auto inserted=peer.call(path,"org.a11y.atspi.EditableText","InsertText",g_variant_new("(isi)",2,"한",3));g_variant_get(inserted.value,"(b)",&accepted);
    check(accepted&&edits==2&&delivered.kind==WidgetInputKind::text&&delivered.text=="한"&&delivered.replacement_range==TextRange{3,0},"Text insertion lost its UTF-16 target");
    auto partial=peer.call(path,"org.a11y.atspi.EditableText","InsertText",g_variant_new("(isi)",1,"한",1));g_variant_get(partial.value,"(b)",&accepted);
    check(!accepted&&edits==2,"A partial UTF-8 sequence was accepted");
    auto removed=peer.call(path,"org.a11y.atspi.EditableText","DeleteText",g_variant_new("(ii)",1,2));g_variant_get(removed.value,"(b)",&accepted);
    check(accepted&&edits==3&&delivered.text.empty()&&delivered.replacement_range==TextRange{1,2},"Delete split a surrogate pair");
    auto copy=peer.call(path,"org.a11y.atspi.EditableText","CutText",g_variant_new("(ii)",1,2));g_variant_get(copy.value,"(b)",&accepted);
    check(!accepted&&edits==3,"Unavailable clipboard operation changed text");
    peer.call(path,"org.a11y.atspi.Text","GetText",g_variant_new("(ii)",4,5),false);
    peer.call(secret,"org.a11y.atspi.Text","GetText",g_variant_new("(ii)",0,1),false);
    Events events;const auto subscription=g_dbus_connection_signal_subscribe(peer.client,nullptr,"org.a11y.atspi.Event.Object",nullptr,nullptr,nullptr,G_DBUS_SIGNAL_FLAGS_NONE,signal,&events,nullptr);
    editor.read_only=true;password.read_only=true;check(app.update(*surface,{editor,password}),"Cannot make editors read-only");bridge.changed();
    wait(peer.context,events.editable_done);
    check(std::find(events.editable_paths.begin(),events.editable_paths.end(),path)!=events.editable_paths.end()&&std::find(events.editable_paths.begin(),events.editable_paths.end(),secret)!=events.editable_paths.end(),"Read-only transitions did not match published editable states");
    g_dbus_connection_signal_unsubscribe(peer.client,subscription);
    peer.call(path,"org.a11y.atspi.EditableText","SetTextContents",g_variant_new("(s)","X"),false);
    auto readonly=peer.call(path,"org.a11y.atspi.Text","GetText",g_variant_new("(ii)",0,-1));g_variant_get(readonly.value,"(&s)",&value);check(std::string(value)=="A😀B","Read-only text could not be read");bridge.detach();
}
struct TextEvent final {
    std::string path,name,detail,text;
    int first=0,second=0;
};
struct TextEvents final {std::vector<TextEvent> values;std::size_t expected=0;bool done=false;};
void text_signal(GDBusConnection*,const gchar*,const gchar* path,const gchar*,const gchar* name,GVariant* values,gpointer data){
    if(std::string_view(name)!="TextChanged"&&std::string_view(name)!="TextCaretMoved"&&std::string_view(name)!="TextSelectionChanged")return;
    auto& events=*static_cast<TextEvents*>(data);const gchar* detail=nullptr;gint first=0,second=0;GVariant* payload=nullptr;GVariant* properties=nullptr;
    g_variant_get(values,"(&sii@v@a{sv})",&detail,&first,&second,&payload,&properties);
    auto* value=g_variant_get_variant(payload);std::string text;
    if(g_variant_is_of_type(value,G_VARIANT_TYPE_STRING))text=g_variant_get_string(value,nullptr);
    events.values.push_back({path,name,detail,std::move(text),first,second});
    g_variant_unref(value);g_variant_unref(payload);g_variant_unref(properties);events.done=events.values.size()>=events.expected;
}
void text_notifications(){
    Peer peer;AtSpiApplication app("Editor");auto window=app.open_window("Document",{100,100});
    auto identity=std::make_shared<int>(0);auto editor=node("editor","",WidgetKind::input_field);
    editor.text_input=true;editor.focused=true;editor.text_length=4;editor.text_selection={1,2};editor.text_revision=TextInputRevision{identity,1,1,1};
    auto secret=node("secret","",WidgetKind::input_field);secret.text_input=true;secret.password=true;secret.text_revision=TextInputRevision{identity,2,1,1};
    auto surface=app.stage(*window,{editor,secret});check(surface&&app.commit(*surface),"Cannot publish notification editors");
    std::string content="A😀B";bool unavailable=false,throws=false,reenter=false;int reads=0;AtSpiDbusBridge* active_bridge=nullptr;
    AtSpiDbusBridge bridge(app,{},AtSpiDbusBridge::TextCallbacks{.read=[&](std::string_view path)->std::optional<AtSpiTextData>{
        ++reads;if(throws)throw std::runtime_error("Provider unavailable");if(unavailable)return std::nullopt;auto target=app.text_target(path);if(!target)return std::nullopt;
        const auto captured=content;
        if(reenter){reenter=false;content="A😁水!B";editor.text_length=6;++editor.text_revision->value;
            check(app.update(*surface,{editor,secret}),"Cannot update editor during a provider read");active_bridge->changed();}
        auto index=AtSpiTextIndex::create(captured,target->selection,target->reversed);if(!index)return std::nullopt;
        return AtSpiTextData{*target,std::move(*index)};
    }});
    active_bridge=&bridge;
    check(bridge.attach(peer.server,":1.144"),"Cannot attach text notification bridge");
    const auto frame=AtSpiApplication::window_path(*window),path=app.object(frame)->children.at(0),secret_path=app.object(frame)->children.at(1);
    TextEvents events;const auto subscription=g_dbus_connection_signal_subscribe(peer.client,nullptr,"org.a11y.atspi.Event.Object",nullptr,nullptr,nullptr,G_DBUS_SIGNAL_FLAGS_NONE,text_signal,&events,nullptr);
    content="A😁한B";editor.text_length=5;editor.text_selection={4,0};++editor.text_revision->value;
    check(app.update(*surface,{editor,secret}),"Cannot replace accessible text");events.expected=4;bridge.changed();wait(peer.context,events.done);
    check(events.values.size()==4,"Text replacement emitted an unexpected event count");
    check(events.values[0].path==path&&events.values[0].name=="TextChanged"&&events.values[0].detail=="delete"&&events.values[0].first==1&&events.values[0].second==1&&events.values[0].text=="😀","Deleted text used invalid Unicode offsets or payload");
    check(events.values[1].name=="TextChanged"&&events.values[1].detail=="insert"&&events.values[1].first==1&&events.values[1].second==2&&events.values[1].text=="😁한","Inserted text used invalid Unicode offsets or payload");
    check(events.values[2].name=="TextCaretMoved"&&events.values[2].first==3&&events.values[3].name=="TextSelectionChanged","Caret and selection changes were not emitted");
    events.values.clear();events.done=false;events.expected=2;editor.text_selection={1,2};editor.text_selection_reversed=true;++editor.text_revision->value;
    check(app.update(*surface,{editor,secret}),"Cannot reverse selection");bridge.changed();wait(peer.context,events.done);
    check(events.values.size()==2&&events.values[0].name=="TextCaretMoved"&&events.values[0].first==1&&events.values[1].name=="TextSelectionChanged","Reversed selection used the wrong caret");
    events.values.clear();events.done=false;events.expected=1;secret.text="private";++secret.text_revision->value;
    check(app.update(*surface,{editor,secret}),"Cannot update password");bridge.changed();
    peer.call(path,"org.a11y.atspi.Accessible","GetRole");check(events.values.empty(),"Password or unchanged editor emitted text notifications");
    unavailable=true;content="A😁水B";++editor.text_revision->value;
    check(app.update(*surface,{editor,secret}),"Cannot update unreadable editor");bridge.changed();
    peer.call(path,"org.a11y.atspi.Accessible","GetRole");check(events.values.empty(),"Unavailable text produced a fabricated change");
    unavailable=false;bridge.changed();peer.call(path,"org.a11y.atspi.Accessible","GetRole");
    check(events.values.empty(),"A recovered reader announced a change without a reliable baseline");
    throws=true;content="A😁木B";++editor.text_revision->value;check(app.update(*surface,{editor,secret}),"Cannot update throwing provider");
    bridge.changed();peer.call(path,"org.a11y.atspi.Accessible","GetRole");check(events.values.empty(),"A throwing reader produced a text event");
    throws=false;content="A😁水B";++editor.text_revision->value;check(app.update(*surface,{editor,secret}),"Cannot restore throwing provider");
    bridge.changed();peer.call(path,"org.a11y.atspi.Accessible","GetRole");check(events.values.empty(),"A throwing reader preserved a stale baseline");
    content="A😁水?B";editor.text_length=6;++editor.text_revision->value;reenter=true;
    check(app.update(*surface,{editor,secret}),"Cannot queue a reentrant edit");events.expected=1;bridge.changed();wait(peer.context,events.done);
    check(events.values.size()==1&&events.values[0].name=="TextChanged"&&events.values[0].detail=="insert"&&events.values[0].first==3&&events.values[0].second==1&&events.values[0].text=="!","Reentrant provider published a stale edit");
    events.values.clear();events.done=false;events.expected=1;editor.password=true;content="TopSecret";editor.text_length=9;++editor.text_revision->value;
    check(app.update(*surface,{editor,secret}),"Cannot protect editor");bridge.changed();peer.call(path,"org.a11y.atspi.Accessible","GetRole");
    check(events.values.empty(),"Changing to a password revealed cached text");
    editor.password=false;editor.text_selection={0,0};editor.text_selection_reversed=false;content="X©";editor.text_length=2;++editor.text_revision->value;
    check(app.update(*surface,{editor,secret}),"Cannot restore editor");bridge.changed();peer.call(path,"org.a11y.atspi.Accessible","GetRole");
    check(events.values.empty(),"Restored editor reused a protected text baseline");
    content="Xé";++editor.text_revision->value;check(app.update(*surface,{editor,secret}),"Cannot replace a shared UTF-8 suffix");
    events.expected=2;bridge.changed();wait(peer.context,events.done);
    check(events.values.size()==2&&events.values[0].detail=="delete"&&events.values[0].first==1&&events.values[0].second==1&&events.values[0].text=="©"&&
          events.values[1].detail=="insert"&&events.values[1].first==1&&events.values[1].second==1&&events.values[1].text=="é","A common UTF-8 continuation byte split a scalar");
    events.values.clear();events.done=false;editor.text_length=TextIndex::maximum_text_bytes+1;++editor.text_revision->value;
    check(app.update(*surface,{editor,secret}),"Cannot publish an oversized editor");const auto before_reads=reads;bridge.changed();peer.call(path,"org.a11y.atspi.Accessible","GetRole");
    check(reads==before_reads&&events.values.empty(),"An over-budget editor was read or produced a fabricated change");
    check(secret_path!=path,"Password shared the editor path");g_dbus_connection_signal_unsubscribe(peer.client,subscription);bridge.detach();
}
void text_attach_retirement(){
    Peer peer;AtSpiApplication app("Editor");auto window=app.open_window("Document",{100,100});
    auto editor=node("editor","",WidgetKind::input_field);editor.text_input=true;editor.text_length=1;
    editor.text_revision=TextInputRevision{std::make_shared<int>(0),1,1,1};auto surface=app.stage(*window,{editor});
    check(surface&&app.commit(*surface),"Cannot publish retirement editor");
    bool retire=true;AtSpiDbusBridge* active=nullptr;
    AtSpiDbusBridge bridge(app,{},AtSpiDbusBridge::TextCallbacks{.read=[&](std::string_view path)->std::optional<AtSpiTextData>{
        auto target=app.text_target(path);if(!target)return std::nullopt;
        if(retire){retire=false;active->detach();}
        auto index=AtSpiTextIndex::create("A",target->selection,target->reversed);if(!index)return std::nullopt;
        return AtSpiTextData{*target,std::move(*index)};
    }});active=&bridge;
    check(!bridge.attach(peer.server,":1.144"),"Retirement during initial text read reported a live attachment");
    check(bridge.attach(peer.server,":1.144"),"A retired text attachment could not be reattached");
    auto role=peer.call(AtSpiApplication::window_path(*window),"org.a11y.atspi.Accessible","GetRole");
    guint32 value=0;g_variant_get(role.value,"(u)",&value);check(value==23,"Reattached text bridge lost its window");bridge.detach();
}
void text_attach_owner_destruction(){
    Peer peer;AtSpiApplication app("Editor");auto window=app.open_window("Document",{100,100});
    auto editor=node("editor","",WidgetKind::input_field);editor.text_input=true;editor.text_length=1;
    editor.text_revision=TextInputRevision{std::make_shared<int>(0),1,1,1};auto surface=app.stage(*window,{editor});
    check(surface&&app.commit(*surface),"Cannot publish destruction editor");
    std::unique_ptr<AtSpiDbusBridge> bridge;
    bridge=std::make_unique<AtSpiDbusBridge>(app,AtSpiDbusBridge::Action{},AtSpiDbusBridge::TextCallbacks{
        .read=[&](std::string_view path)->std::optional<AtSpiTextData>{
            auto target=app.text_target(path);if(!target)return std::nullopt;bridge.reset();
            auto index=AtSpiTextIndex::create("A",target->selection,target->reversed);if(!index)return std::nullopt;
            return AtSpiTextData{*target,std::move(*index)};
        }});
    auto* raw=bridge.get();check(!raw->attach(peer.server,":1.144")&&!bridge,"A destroyed bridge reported a live attachment");
    AtSpiDbusBridge replacement(app,{});check(replacement.attach(peer.server,":1.144"),"Destroyed bridge left an accessibility registration");replacement.detach();
}
void session_lifetime(){
    Peer peer;std::mutex mutex;std::vector<std::function<void()>> tasks;int calls=0;
    auto post=[&](std::function<void()> task){std::lock_guard lock(mutex);tasks.push_back(std::move(task));return true;};
    auto owner=AtSpiCoordinator::create("Editor",post);auto window=owner->open_window("Canvas",{100,100});
    auto button=node("button","",WidgetKind::button);button.text="Save";
    auto identity=std::make_shared<int>(0);auto editor=node("editor","",WidgetKind::input_field);editor.text_input=true;editor.focused=true;
    editor.text_length=3;editor.text_selection={0,0};editor.text_revision=TextInputRevision{identity,1,1,1};WidgetInput delivered;double delivered_value=-1;
    auto slider=node("slider","",WidgetKind::action_surface);slider.accessibility_semantics=AccessibilitySemantics{.role=AccessibilityRole::slider,.range=AccessibilityRange{0,10,2}};
    bool value_published=false;Widget actual_slider;WidgetFrameHooks hooks;
    hooks.accessibility_value_changed=[&]{if(!value_published)return;auto value=actual_slider.snapshot();check(bool(value),"Cannot inspect live slider");
        slider.accessibility_semantics=value->accessibility_semantics;check(owner->update(*window,{button,editor,slider}),"Cannot refresh live slider value");};
    auto widget_state=std::make_shared<WidgetState>([](ii::runtime::Task task){task();},WidgetAppearance{},std::move(hooks));
    auto actual=widget_state->action_surface({},"slider",{},{});check(bool(actual),"Cannot create live slider");actual_slider=*actual;
    check(bool(actual_slider.set_accessibility_semantics(slider.accessibility_semantics)),"Cannot set live slider range");
    check(bool(actual_slider.set_accessibility_value_handler([&](double value)->ii::core::Result<void>{
        delivered_value=value;auto snapshot=actual_slider.snapshot();if(!snapshot)return std::unexpected(snapshot.error());
        auto semantics=*snapshot->accessibility_semantics;semantics.range->value=value;return actual_slider.set_accessibility_semantics(semantics);
    })),"Cannot set live slider handler");
    check(window&&owner->publish(*window,{button,editor,slider},[&](WidgetInput input){++calls;delivered=std::move(input);},
        [](const AtSpiTextTarget& target){return AtSpiTextIndex::create("A😀",target.selection,target.reversed);},
        {.can_set=[&](std::string_view id,double value){auto result=widget_state->can_set_accessibility_value(id,value);return result&&*result;},
         .set=[&](std::string_view id,double value){check(bool(widget_state->set_accessibility_value(id,value)),"Live slider rejected an accessible value");}}),"Cannot publish session content");
    value_published=true;
    auto server=std::shared_ptr<GDBusConnection>(G_DBUS_CONNECTION(g_object_ref(peer.server)),[](GDBusConnection* value){g_object_unref(value);});
    auto attempts=std::make_shared<std::atomic_int>(0);
    auto session=AtSpiSession::create(owner,post,
        [server,attempts](GCancellable*)->std::optional<AtSpiConnection>{if(attempts->fetch_add(1)==0)return std::nullopt;return AtSpiConnection{server,":1.144"};},
        [](const AtSpiConnection&,GCancellable*)->std::optional<AtSpiReference>{return AtSpiReference{":1.5","/org/a11y/atspi/accessible/root"};});
    check(bool(session),"Cannot start session");
    peer.extra=[&]{std::vector<std::function<void()>> ready;{std::lock_guard lock(mutex);ready.swap(tasks);}for(auto& task:ready)task();session->pump();};
    bool ready=false;const auto limit=std::chrono::steady_clock::now()+std::chrono::seconds(5);
    while(!ready&&std::chrono::steady_clock::now()<limit){peer.extra();while(g_main_context_pending(peer.context))g_main_context_iteration(peer.context,FALSE);ready=session->connected();std::this_thread::sleep_for(std::chrono::milliseconds(1));}
    check(ready&&attempts->load()>=2,"Session did not retry, attach and register");const auto frame=AtSpiApplication::window_path(*window),path=owner->model().object(frame)->children.at(0);
    Events events;const auto subscription=g_dbus_connection_signal_subscribe(peer.client,nullptr,"org.a11y.atspi.Event.Object",nullptr,nullptr,nullptr,G_DBUS_SIGNAL_FLAGS_NONE,signal,&events,nullptr);
    button.text="Updated";check(owner->update(*window,{button,editor,slider}),"Cannot update session content");wait(peer.context,events.renamed,peer.extra);
    g_dbus_connection_signal_unsubscribe(peer.client,subscription);
    auto reply=peer.call(path,"org.a11y.atspi.Action","DoAction",g_variant_new("(i)",0));gboolean accepted=FALSE;g_variant_get(reply.value,"(b)",&accepted);
    check(accepted,"A session action was rejected");peer.extra();check(calls==1,"A session action was not delivered");
    const auto text_path=owner->model().object(frame)->children.at(1);
    auto contents=peer.call(text_path,"org.a11y.atspi.Text","GetText",g_variant_new("(ii)",0,-1));const gchar* text=nullptr;g_variant_get(contents.value,"(&s)",&text);
    check(std::string(text)=="A😀","Session did not connect the text reader");
    auto caret=peer.call(text_path,"org.a11y.atspi.Text","SetCaretOffset",g_variant_new("(i)",2));g_variant_get(caret.value,"(b)",&accepted);
    peer.extra();check(accepted&&calls==2&&delivered.kind==WidgetInputKind::text_selection&&delivered.replacement_range==TextRange{3,0}&&delivered.expected_text_revision==editor.text_revision,"Session did not deliver a revision-guarded caret");
    const auto value_path=owner->model().object(frame)->children.at(2);
    peer.call(value_path,"org.freedesktop.DBus.Properties","Set",g_variant_new("(ssv)","org.a11y.atspi.Value","CurrentValue",g_variant_new_double(7)));
    peer.extra();auto updated_value=peer.property(value_path,"org.a11y.atspi.Value","CurrentValue");
    check(delivered_value==7&&g_variant_get_double(updated_value.value)==7,"Session did not deliver and publish a live accessible numeric value");
    Pending in_flight;g_dbus_connection_call(peer.client,nullptr,path.c_str(),"org.a11y.atspi.Accessible","GetRole",nullptr,nullptr,G_DBUS_CALL_FLAGS_NONE,3000,nullptr,complete,&in_flight);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));session->stop();wait(peer.context,in_flight.done,peer.extra);
    check(!in_flight.result&&in_flight.error,"A queued request survived session shutdown");g_error_free(in_flight.error);
    peer.call(path,"org.a11y.atspi.Accessible","GetRole",nullptr,false);value_published=false;widget_state->close();peer.extra={};
}
void cancel_connect(){
    std::atomic_bool started=false,finished=false;auto post=[](std::function<void()>){return false;};auto owner=AtSpiCoordinator::create("Editor",post);
    auto session=AtSpiSession::create(owner,post,[&](GCancellable* cancel)->std::optional<AtSpiConnection>{
        started.store(true);while(!g_cancellable_is_cancelled(cancel))std::this_thread::sleep_for(std::chrono::milliseconds(1));finished.store(true);return std::nullopt;
    },[](const AtSpiConnection&,GCancellable*)->std::optional<AtSpiReference>{return std::nullopt;});
    const auto limit=std::chrono::steady_clock::now()+std::chrono::seconds(5);while(!started.load()&&std::chrono::steady_clock::now()<limit)std::this_thread::yield();
    check(started.load(),"Connection worker did not start");session->stop();check(finished.load(),"Closing did not cancel and join the connection worker");
}
void cache_registration_rollback(){
    Peer peer;GError* error=nullptr;
    auto* info=g_dbus_node_info_new_for_xml("<node><interface name='org.a11y.atspi.Cache'><method name='GetItems'/></interface></node>",&error);
    check(info&&!error,"Cannot create conflicting cache interface");const GDBusInterfaceVTable table{};
    const auto blocker=g_dbus_connection_register_object(peer.server,"/org/a11y/atspi/cache",info->interfaces[0],&table,nullptr,nullptr,&error);
    check(blocker&&!error,"Cannot register conflicting cache");AtSpiApplication app("Editor");AtSpiDbusBridge bridge(app,{});
    check(!bridge.attach(peer.server,":1.144"),"A conflicting cache registration was accepted");
    check(g_dbus_connection_unregister_object(peer.server,blocker),"Cannot retire conflicting cache");
    check(bridge.attach(peer.server,":1.144"),"A failed cache registration did not roll back the subtree");
    auto role=peer.call(std::string(AtSpiApplication::root_path()),"org.a11y.atspi.Accessible","GetRole");guint32 value=0;g_variant_get(role.value,"(u)",&value);
    check(value==75,"The bridge did not recover after registration failure");bridge.detach();g_dbus_node_info_unref(info);
}
}
int main(){protocol();text_protocol();text_notifications();text_attach_retirement();text_attach_owner_destruction();value_protocol();session_lifetime();cancel_connect();cache_registration_rollback();}
