#include "AtSpiDbus.h"
#include <gio/gio.h>
#include <algorithm>
#include <clocale>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <unordered_map>
#include <utility>

#ifndef IIGUI_VERSION_STRING
#define IIGUI_VERSION_STRING ""
#endif

namespace ii::gui::detail {
namespace {
constexpr const char* subtree="/org/a11y/atspi/accessible";
constexpr const char* accessible="org.a11y.atspi.Accessible";
constexpr const char* application="org.a11y.atspi.Application";
constexpr const char* component="org.a11y.atspi.Component";
constexpr const char* action="org.a11y.atspi.Action";
constexpr const char* text_interface="org.a11y.atspi.Text";
constexpr const char* editable_text="org.a11y.atspi.EditableText";
constexpr const char* value_interface="org.a11y.atspi.Value";
constexpr const char* cache_interface="org.a11y.atspi.Cache";
constexpr const char* cache_path="/org/a11y/atspi/cache";
constexpr const char* null_path="/org/a11y/atspi/null";
constexpr const char* interfaces=R"XML(
<node>
 <interface name='org.a11y.atspi.Accessible'>
  <property name='version' type='u' access='read'/><property name='Name' type='s' access='read'/>
  <property name='Description' type='s' access='read'/><property name='Parent' type='(so)' access='read'/>
  <property name='ChildCount' type='i' access='read'/><property name='Locale' type='s' access='read'/>
  <property name='AccessibleId' type='s' access='read'/><property name='HelpText' type='s' access='read'/>
  <method name='GetChildAtIndex'><arg type='i' direction='in'/><arg type='(so)' direction='out'/></method>
  <method name='GetChildren'><arg type='a(so)' direction='out'/></method>
  <method name='GetIndexInParent'><arg type='i' direction='out'/></method>
  <method name='GetRelationSet'><arg type='a(ua(so))' direction='out'/></method>
  <method name='GetRole'><arg type='u' direction='out'/></method>
  <method name='GetRoleName'><arg type='s' direction='out'/></method>
  <method name='GetLocalizedRoleName'><arg type='s' direction='out'/></method>
  <method name='GetState'><arg type='au' direction='out'/></method>
  <method name='GetAttributes'><arg type='a{ss}' direction='out'/></method>
  <method name='GetApplication'><arg type='(so)' direction='out'/></method>
  <method name='GetInterfaces'><arg type='as' direction='out'/></method>
 </interface>
 <interface name='org.a11y.atspi.Application'>
  <property name='ToolkitName' type='s' access='read'/><property name='Version' type='s' access='read'/>
  <property name='ToolkitVersion' type='s' access='read'/><property name='AtspiVersion' type='s' access='read'/>
  <property name='InterfaceVersion' type='u' access='read'/><property name='Id' type='i' access='readwrite'/>
  <method name='GetLocale'><arg type='u' direction='in'/><arg type='s' direction='out'/></method>
  <method name='GetApplicationBusAddress'><arg type='s' direction='out'/></method>
 </interface>
 <interface name='org.a11y.atspi.Component'>
  <property name='version' type='u' access='read'/>
  <method name='Contains'><arg type='i' direction='in'/><arg type='i' direction='in'/><arg type='u' direction='in'/><arg type='b' direction='out'/></method>
  <method name='GetAccessibleAtPoint'><arg type='i' direction='in'/><arg type='i' direction='in'/><arg type='u' direction='in'/><arg type='(so)' direction='out'/></method>
  <method name='GetExtents'><arg type='u' direction='in'/><arg type='(iiii)' direction='out'/></method>
  <method name='GetPosition'><arg type='u' direction='in'/><arg type='i' direction='out'/><arg type='i' direction='out'/></method>
  <method name='GetSize'><arg type='i' direction='out'/><arg type='i' direction='out'/></method>
  <method name='GetLayer'><arg type='u' direction='out'/></method>
  <method name='GetMDIZOrder'><arg type='n' direction='out'/></method>
  <method name='GrabFocus'><arg type='b' direction='out'/></method>
 </interface>
 <interface name='org.a11y.atspi.Action'>
  <property name='version' type='u' access='read'/><property name='NActions' type='i' access='read'/>
  <method name='GetName'><arg type='i' direction='in'/><arg type='s' direction='out'/></method>
  <method name='GetLocalizedName'><arg type='i' direction='in'/><arg type='s' direction='out'/></method>
  <method name='GetDescription'><arg type='i' direction='in'/><arg type='s' direction='out'/></method>
  <method name='GetKeyBinding'><arg type='i' direction='in'/><arg type='s' direction='out'/></method>
  <method name='GetActions'><arg type='a(sss)' direction='out'/></method>
  <method name='DoAction'><arg type='i' direction='in'/><arg type='b' direction='out'/></method>
 </interface>
 <interface name='org.a11y.atspi.Text'>
  <property name='version' type='u' access='read'/><property name='CharacterCount' type='i' access='read'/><property name='CaretOffset' type='i' access='read'/>
  <method name='GetText'><arg type='i' direction='in'/><arg type='i' direction='in'/><arg type='s' direction='out'/></method>
  <method name='GetCharacterAtOffset'><arg type='i' direction='in'/><arg type='i' direction='out'/></method>
  <method name='GetStringAtOffset'><arg type='i' direction='in'/><arg type='u' direction='in'/><arg type='s' direction='out'/><arg type='i' direction='out'/><arg type='i' direction='out'/></method>
  <method name='GetNSelections'><arg type='i' direction='out'/></method>
  <method name='GetSelection'><arg type='i' direction='in'/><arg type='i' direction='out'/><arg type='i' direction='out'/></method>
  <method name='SetCaretOffset'><arg type='i' direction='in'/><arg type='b' direction='out'/></method>
  <method name='AddSelection'><arg type='i' direction='in'/><arg type='i' direction='in'/><arg type='b' direction='out'/></method>
  <method name='SetSelection'><arg type='i' direction='in'/><arg type='i' direction='in'/><arg type='i' direction='in'/><arg type='b' direction='out'/></method>
  <method name='RemoveSelection'><arg type='i' direction='in'/><arg type='b' direction='out'/></method>
 </interface>
 <interface name='org.a11y.atspi.EditableText'>
  <property name='version' type='u' access='read'/>
  <method name='SetTextContents'><arg type='s' direction='in'/><arg type='b' direction='out'/></method>
  <method name='InsertText'><arg type='i' direction='in'/><arg type='s' direction='in'/><arg type='i' direction='in'/><arg type='b' direction='out'/></method>
  <method name='DeleteText'><arg type='i' direction='in'/><arg type='i' direction='in'/><arg type='b' direction='out'/></method>
  <method name='CopyText'><arg type='i' direction='in'/><arg type='i' direction='in'/></method>
  <method name='CutText'><arg type='i' direction='in'/><arg type='i' direction='in'/><arg type='b' direction='out'/></method>
  <method name='PasteText'><arg type='i' direction='in'/><arg type='b' direction='out'/></method>
 </interface>
 <interface name='org.a11y.atspi.Value'>
  <property name='version' type='u' access='read'/><property name='MinimumValue' type='d' access='read'/>
  <property name='MaximumValue' type='d' access='read'/><property name='MinimumIncrement' type='d' access='read'/>
  <property name='CurrentValue' type='d' access='readwrite'/><property name='Text' type='s' access='read'/>
 </interface>
 <interface name='org.a11y.atspi.Cache'>
  <property name='version' type='u' access='read'/>
  <method name='GetItems'><arg type='a((so)(so)(so)iiassusau)' direction='out'/></method>
  <signal name='AddAccessible'><arg type='((so)(so)(so)iiassusau)'/></signal>
  <signal name='RemoveAccessible'><arg type='(so)'/></signal>
 </interface>
</node>)XML";

const char* role_name(AtSpiRole value)noexcept{
    switch(value){
        case AtSpiRole::application:return "application";case AtSpiRole::frame:return "frame";
        case AtSpiRole::check_box:return "check box";case AtSpiRole::column_header:return "column header";
        case AtSpiRole::image:return "image";case AtSpiRole::label:return "label";case AtSpiRole::menu:return "menu";
        case AtSpiRole::menu_item:return "menu item";case AtSpiRole::tab:return "page tab";case AtSpiRole::tab_group:return "page tab list";
        case AtSpiRole::panel:return "panel";case AtSpiRole::password:return "password text";case AtSpiRole::progress:return "progress bar";
        case AtSpiRole::button:return "push button";case AtSpiRole::radio_button:return "radio button";case AtSpiRole::scroll_area:return "scroll pane";
        case AtSpiRole::slider:return "slider";case AtSpiRole::stepper:return "spin button";case AtSpiRole::table:return "table";
        case AtSpiRole::cell:return "table cell";case AtSpiRole::text:return "text";case AtSpiRole::entry:return "entry";case AtSpiRole::link:return "link";
    }
    return "unknown";
}
bool focusable(const AtSpiObject& object)noexcept{
    return object.actionable||object.role==AtSpiRole::entry||object.role==AtSpiRole::password||
        object.role==AtSpiRole::text||object.role==AtSpiRole::slider||object.role==AtSpiRole::stepper;
}
std::uint32_t states(const AtSpiObject& object)noexcept{
    std::uint32_t result=(1u<<30); // visible
    if(object.bounds.width>0&&object.bounds.height>0)result|=(1u<<25);
    if(object.enabled)result|=(1u<<8)|(1u<<24);
    if(focusable(object))result|=(1u<<11);
    if(object.focused)result|=(1u<<12);
    if(object.selected)result|=(1u<<23);
    if(object.checked)result|=(1u<<4);
    if(!object.read_only&&(object.role==AtSpiRole::entry||object.role==AtSpiRole::password||object.role==AtSpiRole::text))result|=(1u<<7);
    return result;
}
const char* locale()noexcept{const auto* value=std::setlocale(LC_MESSAGES,nullptr);return value?value:"C";}
int integer(double value)noexcept{
    if(!std::isfinite(value))return 0;
    const auto low=double(std::numeric_limits<int>::min()),high=double(std::numeric_limits<int>::max());
    return static_cast<int>(std::round(std::clamp(value,low,high)));
}
void error(GDBusMethodInvocation* invocation,const char* message){
    g_dbus_method_invocation_return_error_literal(invocation,G_DBUS_ERROR,G_DBUS_ERROR_FAILED,message);
}
void fail(GError** value,const char* message){g_set_error_literal(value,G_DBUS_ERROR,G_DBUS_ERROR_FAILED,message);}
} // namespace

struct AtSpiDbusBridge::Impl final {
    static constexpr std::size_t text_cache_budget=64*1024*1024;
    AtSpiApplication& model;Action enqueue;TextCallbacks text_callbacks;ValueInput value_input;GDBusConnection* connection=nullptr;GDBusNodeInfo* info=nullptr;
    guint registration=0,cache_registration=0;std::string local_name,parent_bus,parent_path;gint application_id=0;
    std::unordered_map<std::string,AtSpiObject> published;
    std::unordered_map<std::string,AtSpiTextData> published_text;
    std::uint64_t attachment_epoch=0;bool reconciling=false,reconcile_again=false;
    Impl(AtSpiApplication& value,Action callback,TextCallbacks text,ValueInput input):model(value),enqueue(std::move(callback)),text_callbacks(std::move(text)),value_input(std::move(input)){
        GError* reason=nullptr;info=g_dbus_node_info_new_for_xml(interfaces,&reason);
        if(!info){std::string message=reason?reason->message:"Invalid AT-SPI interface";if(reason)g_error_free(reason);throw std::runtime_error(message);}
    }
    ~Impl(){detach();g_dbus_node_info_unref(info);}
    [[nodiscard]] bool has_text(const AtSpiObject& object)const noexcept{return bool(object.text_state)&&bool(text_callbacks.read);}
    [[nodiscard]] bool can_edit(const AtSpiObject& object)const noexcept{return has_text(object)&&!object.read_only&&bool(text_callbacks.input);}
    [[nodiscard]] std::optional<AtSpiTextData> read_text(const AtSpiObject& object){
        try{
            if(!has_text(object))return std::nullopt;auto before=model.text_target(object.path);if(!before)return std::nullopt;
            auto data=text_callbacks.read(object.path);if(!connection||!data||data->target!=*before||data->index.utf16_size()!=before->length||model.text_target(object.path)!=before)return std::nullopt;
            return data;
        }catch(...){return std::nullopt;}
    }
    [[nodiscard]] bool submit_text(const AtSpiObject& object,const AtSpiTextData& data,WidgetInputKind kind,
                                   gint first,gint last,std::string value={}){
        if(!text_callbacks.input||!data.target.focused||!data.target.enabled||first<0||last<first||
           (kind==WidgetInputKind::text&&data.target.read_only)||value.size()>65536||
           !g_utf8_validate(value.data(),static_cast<gssize>(value.size()),nullptr))return false;
        const auto start=data.index.utf16_at(static_cast<std::size_t>(first));
        const auto end=data.index.utf16_at(static_cast<std::size_t>(last));
        if(!start||!end)return false;
        WidgetInput input;input.kind=kind;input.replacement_range=TextRange{*start,*end-*start};
        input.expected_text_revision=data.target.revision;input.text=std::move(value);
        return text_callbacks.input(object.path,std::move(input));
    }
    void detach()noexcept{
        ++attachment_epoch;
        if(connection&&cache_registration)g_dbus_connection_unregister_object(connection,cache_registration);
        if(connection&&registration)g_dbus_connection_unregister_subtree(connection,registration);
        registration=0;cache_registration=0;if(connection)g_object_unref(std::exchange(connection,nullptr));
        local_name.clear();parent_bus.clear();parent_path.clear();published.clear();published_text.clear();application_id=0;
    }
    [[nodiscard]] std::unordered_map<std::string,AtSpiObject> snapshot()const{
        std::unordered_map<std::string,AtSpiObject> result;std::vector<std::string> pending{std::string(AtSpiApplication::root_path())};
        while(!pending.empty()){
            auto path=std::move(pending.back());pending.pop_back();if(result.contains(path))continue;
            auto object=model.object(path);if(!object)continue;
            for(const auto& child:object->children)pending.push_back(child);
            result.emplace(std::move(path),std::move(*object));
        }
        return result;
    }
    [[nodiscard]] static std::vector<std::string> ordered(const std::unordered_map<std::string,AtSpiObject>& values){
        std::vector<std::string> result,pending{std::string(AtSpiApplication::root_path())};
        while(!pending.empty()){
            auto path=std::move(pending.back());pending.pop_back();const auto found=values.find(path);if(found==values.end())continue;
            result.push_back(std::move(path));
            for(auto child=found->second.children.rbegin();child!=found->second.children.rend();++child)pending.push_back(*child);
        }
        return result;
    }
    void emit(const std::string& path,const char* signal,const char* detail,int first,GVariant* payload,int second=0)const noexcept{
        if(!connection)return;
        GVariantBuilder properties;g_variant_builder_init(&properties,G_VARIANT_TYPE("a{sv}"));
        GError* reason=nullptr;
        (void)g_dbus_connection_emit_signal(connection,nullptr,path.c_str(),"org.a11y.atspi.Event.Object",signal,
            g_variant_new("(siiv@a{sv})",detail,first,second,payload,g_variant_builder_end(&properties)),&reason);
        if(reason)g_error_free(reason);
    }
    void state_change(const std::string& path,const char* name,bool before,bool after)const noexcept{
        if(before!=after)emit(path,"StateChanged",name,after?1:0,g_variant_new_int32(0));
    }
    [[nodiscard]] GVariant* cache_item(const AtSpiObject& object,const std::unordered_map<std::string,AtSpiObject>& values)const{
        gint index=-1;const char* parent_name="";const char* parent_object=null_path;
        if(!object.parent.empty()){
            parent_name=local_name.c_str();parent_object=object.parent.c_str();
            const auto parent=values.find(object.parent);if(parent!=values.end()){
                const auto found=std::find(parent->second.children.begin(),parent->second.children.end(),object.path);
                if(found!=parent->second.children.end())index=static_cast<gint>(found-parent->second.children.begin());
            }
        }else if(!parent_bus.empty()&&!parent_path.empty()){parent_name=parent_bus.c_str();parent_object=parent_path.c_str();}
        GVariantBuilder supported;g_variant_builder_init(&supported,G_VARIANT_TYPE("as"));
        g_variant_builder_add(&supported,"s",accessible);g_variant_builder_add(&supported,"s",object.role==AtSpiRole::application?application:component);
        if(object.actionable)g_variant_builder_add(&supported,"s",action);
        if(has_text(object))g_variant_builder_add(&supported,"s",text_interface);if(can_edit(object))g_variant_builder_add(&supported,"s",editable_text);
        if(object.numeric)g_variant_builder_add(&supported,"s",value_interface);
        GVariantBuilder state;g_variant_builder_init(&state,G_VARIANT_TYPE("au"));g_variant_builder_add(&state,"u",states(object));g_variant_builder_add(&state,"u",has_text(object)?guint32(1u<<6):guint32(0));
        return g_variant_new("((so)(so)(so)ii@assus@au)",local_name.c_str(),object.path.c_str(),
            local_name.c_str(),AtSpiApplication::root_path().data(),parent_name,parent_object,index,gint(object.children.size()),
            g_variant_builder_end(&supported),object.name.c_str(),guint32(object.role),object.description.c_str(),g_variant_builder_end(&state));
    }
    void emit_cache(const char* signal,GVariant* value)const noexcept{
        if(!connection)return;GError* reason=nullptr;
        (void)g_dbus_connection_emit_signal(connection,nullptr,cache_path,cache_interface,signal,value,&reason);
        if(reason)g_error_free(reason);
    }
    [[nodiscard]] std::unordered_map<std::string,AtSpiTextData> text_snapshot(const std::unordered_map<std::string,AtSpiObject>& values){
        std::unordered_map<std::string,AtSpiTextData> result;std::size_t used=0;
        const auto paths=ordered(values);
        // Prefer the active editor when the total snapshot budget is exhausted.
        for(bool focused:{true,false})for(const auto& path:paths){
            if(!connection||reconcile_again)return result;
            const auto& object=values.at(path);
            if(!has_text(object)||object.focused!=focused||object.text_state->length>text_cache_budget-used)continue;
            const auto previous=published_text.find(path);std::optional<AtSpiTextData> data;
            if(previous!=published_text.end()&&previous->second.target==*object.text_state)data=previous->second;
            else data=read_text(object);
            if(!data||data->target!=*object.text_state)continue;
            const auto size=data->index.storage_bytes();if(size>text_cache_budget-used)continue;
            used+=size;result.emplace(path,std::move(*data));
        }
        return result;
    }
    void text_changes(const std::string& path,const AtSpiTextIndex& old,const AtSpiTextIndex& value)const{
        const auto before=old.text(),after=value.text();std::size_t prefix=0,suffix=0;
        while(prefix<before.size()&&prefix<after.size()&&before[prefix]==after[prefix])++prefix;
        // A UTF-8 leading byte can match while the rest of the scalar differs.
        while(prefix&&prefix<before.size()&&(static_cast<unsigned char>(before[prefix])&0xc0)==0x80)--prefix;
        while(suffix<before.size()-prefix&&suffix<after.size()-prefix&&before[before.size()-suffix-1]==after[after.size()-suffix-1])++suffix;
        while(suffix&&(static_cast<unsigned char>(before[before.size()-suffix])&0xc0)==0x80)--suffix;
        const auto removed=before.substr(prefix,before.size()-prefix-suffix);
        const auto inserted=after.substr(prefix,after.size()-prefix-suffix);
        const auto position=static_cast<int>(g_utf8_strlen(before.data(),static_cast<gssize>(prefix)));
        // Allocate both payloads before emitting either half of a replacement.
        const std::string removed_text(removed),inserted_text(inserted);
        if(!removed.empty())emit(path,"TextChanged","delete",position,g_variant_new_string(removed_text.c_str()),
            static_cast<int>(g_utf8_strlen(removed.data(),static_cast<gssize>(removed.size()))));
        if(!inserted.empty())emit(path,"TextChanged","insert",position,g_variant_new_string(inserted_text.c_str()),
            static_cast<int>(g_utf8_strlen(inserted.data(),static_cast<gssize>(inserted.size()))));
        if(old.caret()!=value.caret())emit(path,"TextCaretMoved","",static_cast<int>(value.caret()),g_variant_new_int32(0));
        const auto previous=old.selection(),current=value.selection();
        if(previous!=current&&(previous.length||current.length))emit(path,"TextSelectionChanged","",0,g_variant_new_int32(0));
    }
    void reconcile(bool notify){
        if(reconciling){reconcile_again=true;return;}
        reconciling=true;struct Reset final {bool& active;~Reset(){active=false;}} reset{reconciling};
        const auto epoch=attachment_epoch;
        for(int attempt=0;attempt<4&&connection&&attachment_epoch==epoch;++attempt){
        reconcile_again=false;auto next=snapshot();auto next_text=text_snapshot(next);
        if(!connection||attachment_epoch!=epoch)return;
        if(reconcile_again)continue;
        if(notify){
            const auto previous_order=ordered(published),next_order=ordered(next);
            for(auto it=previous_order.rbegin();it!=previous_order.rend();++it)if(!next.contains(*it))
                emit_cache("RemoveAccessible",g_variant_new("((so))",local_name.c_str(),it->c_str()));
            for(const auto& path:next_order)if(!published.contains(path))
                emit_cache("AddAccessible",g_variant_new("(@((so)(so)(so)iiassusau))",cache_item(next.at(path),next)));
            for(const auto& [path,old]:published){
                const auto current=next.find(path);if(current==next.end())continue;
                auto working=old.children;const auto& wanted=current->second.children;
                for(std::size_t index=0;index<wanted.size();++index){
                    if(index<working.size()&&working[index]==wanted[index])continue;
                    const auto found=std::find(working.begin()+std::min(index,working.size()),working.end(),wanted[index]);
                    if(found!=working.end()){
                        const auto position=std::size_t(found-working.begin());const auto moved=*found;
                        emit(path,"ChildrenChanged","remove",int(position),g_variant_new("(so)",local_name.c_str(),moved.c_str()));working.erase(found);
                    }
                    emit(path,"ChildrenChanged","add",int(index),g_variant_new("(so)",local_name.c_str(),wanted[index].c_str()));
                    working.insert(working.begin()+std::min(index,working.size()),wanted[index]);
                }
                while(working.size()>wanted.size()){
                    const auto position=working.size()-1;const auto removed=working.back();working.pop_back();
                    emit(path,"ChildrenChanged","remove",int(position),g_variant_new("(so)",local_name.c_str(),removed.c_str()));
                }
                const auto& value=current->second;
                if(has_text(old)!=has_text(value)||can_edit(old)!=can_edit(value)||bool(old.numeric)!=bool(value.numeric)){
                    emit_cache("RemoveAccessible",g_variant_new("((so))",local_name.c_str(),path.c_str()));
                    emit_cache("AddAccessible",g_variant_new("(@((so)(so)(so)iiassusau))",cache_item(value,next)));
                }
                state_change(path,"focused",old.focused,value.focused);state_change(path,"selected",old.selected,value.selected);
                state_change(path,"checked",old.checked,value.checked);state_change(path,"enabled",old.enabled,value.enabled);
                state_change(path,"sensitive",old.enabled,value.enabled);
                state_change(path,"editable",(states(old)&(1u<<7))!=0,(states(value)&(1u<<7))!=0);
                if(old.name!=value.name)emit(path,"PropertyChange","accessible-name",0,g_variant_new_string(value.name.c_str()));
                if(old.description!=value.description)emit(path,"PropertyChange","accessible-description",0,g_variant_new_string(value.description.c_str()));
                if(old.numeric&&value.numeric&&old.numeric->value!=value.numeric->value)
                    emit(path,"PropertyChange","accessible-value",0,g_variant_new_double(value.numeric->value));
                const auto before_text=published_text.find(path),after_text=next_text.find(path);
                if(before_text!=published_text.end()&&after_text!=next_text.end())text_changes(path,before_text->second.index,after_text->second.index);
                if(old.bounds!=value.bounds||old.screen_origin!=value.screen_origin||old.value!=value.value||old.numeric!=value.numeric||old.text_state!=value.text_state)
                    emit(path,"VisibleDataChanged","",0,g_variant_new_int32(0));
            }
        }
        published=std::move(next);published_text=std::move(next_text);
        if(!reconcile_again)return;
        }
    }
    std::string path(const gchar* node)const{return node?std::string(subtree)+"/"+node:std::string(subtree);}
    GVariant* reference(std::string_view name,std::string_view object)const{
        return g_variant_new("(so)",std::string(name).c_str(),std::string(object).c_str());
    }
    GVariant* parent(const AtSpiObject& object)const{
        if(!object.parent.empty())return reference(local_name,object.parent);
        if(!parent_bus.empty()&&!parent_path.empty())return reference(parent_bus,parent_path);
        return reference("",null_path);
    }
    GDBusInterfaceInfo* find(const char* name)const{return g_dbus_node_info_lookup_interface(info,name);}
    static gchar** enumerate(GDBusConnection*,const gchar*,const gchar*,gpointer){
        auto** result=g_new0(gchar*,2);result[0]=g_strdup("root");return result;
    }
    static GDBusInterfaceInfo** introspect(GDBusConnection*,const gchar*,const gchar*,const gchar* node,gpointer data){
        try{auto held=static_cast<std::weak_ptr<Impl>*>(data)->lock();if(!held||!held->connection)return nullptr;auto& self=*held;auto object=self.model.object(self.path(node));if(!object)return nullptr;
            auto** result=g_new0(GDBusInterfaceInfo*,8);std::size_t i=0;
            result[i++]=g_dbus_interface_info_ref(self.find(accessible));
            if(object->role==AtSpiRole::application)result[i++]=g_dbus_interface_info_ref(self.find(application));
            else result[i++]=g_dbus_interface_info_ref(self.find(component));
            if(object->actionable)result[i++]=g_dbus_interface_info_ref(self.find(action));
            if(self.has_text(*object))result[i++]=g_dbus_interface_info_ref(self.find(text_interface));
            if(self.can_edit(*object))result[i++]=g_dbus_interface_info_ref(self.find(editable_text));
            if(object->numeric)result[i++]=g_dbus_interface_info_ref(self.find(value_interface));
            return result;
        }catch(...){return nullptr;}
    }
    static const GDBusInterfaceVTable* interface_table(){static const GDBusInterfaceVTable table{method,get_property,set_property,{}};return &table;}
    static const GDBusInterfaceVTable* dispatch(GDBusConnection*,const gchar*,const gchar*,const gchar*,const gchar*,gpointer* out,gpointer data){
        *out=data;return interface_table();
    }
    static GVariant* get_property(GDBusConnection*,const gchar*,const gchar* path,const gchar* iface,const gchar* name,GError** reason,gpointer data){
        try{auto held=static_cast<std::weak_ptr<Impl>*>(data)->lock();if(!held||!held->connection){fail(reason,"Accessibility service is closed");return nullptr;}auto& self=*held;
            if(std::string_view(path)==cache_path&&std::string_view(iface)==cache_interface&&std::string_view(name)=="version")return g_variant_new_uint32(1);
            auto object=self.model.object(path);if(!object){fail(reason,"Accessible object is retired");return nullptr;}
            const std::string_view interface(iface),property(name);
            if(interface==accessible){
                if(property=="version")return g_variant_new_uint32(1);
                if(property=="Name")return g_variant_new_string(object->name.c_str());
                if(property=="Description")return g_variant_new_string(object->description.c_str());
                if(property=="Parent")return self.parent(*object);
                if(property=="ChildCount")return g_variant_new_int32(static_cast<gint32>(object->children.size()));
                if(property=="Locale")return g_variant_new_string(locale());
                if(property=="AccessibleId")return g_variant_new_string(object->path.c_str());
                if(property=="HelpText")return g_variant_new_string("");
            }else if(interface==application&&object->role==AtSpiRole::application){
                if(property=="ToolkitName")return g_variant_new_string("iiGUI");
                if(property=="Version"||property=="ToolkitVersion")return g_variant_new_string(IIGUI_VERSION_STRING);
                if(property=="AtspiVersion")return g_variant_new_string("2.1");
                if(property=="InterfaceVersion")return g_variant_new_uint32(1);
                if(property=="Id")return g_variant_new_int32(self.application_id);
            }else if(interface==component&&property=="version")return g_variant_new_uint32(1);
            else if(interface==action){if(property=="version")return g_variant_new_uint32(1);if(property=="NActions")return g_variant_new_int32(object->actionable?1:0);}
            else if(interface==text_interface&&self.has_text(*object)){
                if(property=="version")return g_variant_new_uint32(1);auto data=self.read_text(*object);if(!data){fail(reason,"Text snapshot is unavailable or changed");return nullptr;}
                if(property=="CharacterCount")return g_variant_new_int32(static_cast<gint32>(data->index.characters()));
                if(property=="CaretOffset")return g_variant_new_int32(static_cast<gint32>(data->index.caret()));
            }else if(interface==editable_text&&self.can_edit(*object)&&property=="version")return g_variant_new_uint32(1);
            else if(interface==value_interface&&object->numeric){
                if(property=="version")return g_variant_new_uint32(1);
                if(property=="MinimumValue")return g_variant_new_double(object->numeric->minimum);
                if(property=="MaximumValue")return g_variant_new_double(object->numeric->maximum);
                if(property=="CurrentValue")return g_variant_new_double(object->numeric->value);
                if(property=="Text")return g_variant_new_string("");
                if(property=="MinimumIncrement")return g_variant_new_double(0);
            }
            fail(reason,"Unsupported accessibility property");return nullptr;
        }catch(...){fail(reason,"Accessibility property failed");return nullptr;}
    }
    static gboolean set_property(GDBusConnection*,const gchar*,const gchar* path,const gchar* iface,const gchar* name,GVariant* value,GError** reason,gpointer data){
        try{auto held=static_cast<std::weak_ptr<Impl>*>(data)->lock();if(!held||!held->connection){fail(reason,"Accessibility service is closed");return FALSE;}auto& self=*held;
            if(std::string_view(path)==AtSpiApplication::root_path()&&std::string_view(iface)==application&&std::string_view(name)=="Id"&&g_variant_is_of_type(value,G_VARIANT_TYPE_INT32)){
                self.application_id=g_variant_get_int32(value);return TRUE;}
            if(std::string_view(iface)==value_interface&&std::string_view(name)=="CurrentValue"&&g_variant_is_of_type(value,G_VARIANT_TYPE_DOUBLE)){
                auto object=self.model.object(path);const auto requested=g_variant_get_double(value);
                if(object&&object->role==AtSpiRole::slider&&object->enabled&&object->numeric&&std::isfinite(requested)&&
                   requested>=object->numeric->minimum&&requested<=object->numeric->maximum&&self.value_input&&self.value_input(object->path,requested))return TRUE;
                fail(reason,"Accessible value cannot be set");return FALSE;
            }
            fail(reason,"Accessibility property is read-only");return FALSE;
        }catch(...){fail(reason,"Accessibility property update failed");return FALSE;}
    }
    [[nodiscard]] bool geometry(const AtSpiObject& object,guint coordinates,double& x,double& y)const{
        x=object.bounds.x;y=object.bounds.y;
        if(coordinates==0){if(!object.screen_origin)return false;x+=object.screen_origin->x;y+=object.screen_origin->y;return true;}
        if(coordinates==1)return true;
        if(coordinates==2){auto p=model.object(object.parent);if(p){x-=p->bounds.x;y-=p->bounds.y;}return true;}
        return false;
    }
    [[nodiscard]] std::optional<AtSpiObject> hit(const AtSpiObject& object,double x,double y)const{
        const auto contains=[&](const AtSpiObject& item){return x>=item.bounds.x&&y>=item.bounds.y&&x<item.bounds.x+item.bounds.width&&y<item.bounds.y+item.bounds.height;};
        if(!contains(object))return std::nullopt;auto current=object;
        for(;;){
            std::optional<AtSpiObject> selected;
            for(auto it=current.children.rbegin();it!=current.children.rend();++it)if(auto child=model.object(*it);child&&contains(*child)){selected=std::move(child);break;}
            if(!selected)return current;current=std::move(*selected);
        }
    }
    void method_impl(const char* raw_path,const char* raw_iface,const char* raw_method,GVariant* params,GDBusMethodInvocation* invocation){
        if(std::string_view(raw_path)==cache_path&&std::string_view(raw_iface)==cache_interface&&std::string_view(raw_method)=="GetItems"){
            GVariantBuilder values;g_variant_builder_init(&values,G_VARIANT_TYPE("a((so)(so)(so)iiassusau)"));
            for(const auto& path:ordered(published))g_variant_builder_add_value(&values,cache_item(published.at(path),published));
            g_dbus_method_invocation_return_value(invocation,g_variant_new("(a((so)(so)(so)iiassusau))",&values));return;
        }
        auto object=model.object(raw_path);if(!object){error(invocation,"Accessible object is retired");return;}
        const std::string_view iface(raw_iface),name(raw_method);
        if(iface==accessible){
            if(name=="GetRole"){g_dbus_method_invocation_return_value(invocation,g_variant_new("(u)",guint32(object->role)));return;}
            if(name=="GetRoleName"||name=="GetLocalizedRoleName"){g_dbus_method_invocation_return_value(invocation,g_variant_new("(s)",role_name(object->role)));return;}
            if(name=="GetState"){GVariantBuilder values;g_variant_builder_init(&values,G_VARIANT_TYPE("au"));g_variant_builder_add(&values,"u",states(*object));g_variant_builder_add(&values,"u",has_text(*object)?guint32(1u<<6):guint32(0));g_dbus_method_invocation_return_value(invocation,g_variant_new("(au)",&values));return;}
            if(name=="GetChildren"){GVariantBuilder values;g_variant_builder_init(&values,G_VARIANT_TYPE("a(so)"));for(const auto& child:object->children)g_variant_builder_add(&values,"(so)",local_name.c_str(),child.c_str());g_dbus_method_invocation_return_value(invocation,g_variant_new("(a(so))",&values));return;}
            if(name=="GetChildAtIndex"){gint index=0;g_variant_get(params,"(i)",&index);if(index<0||std::size_t(index)>=object->children.size()){error(invocation,"Child index is out of range");return;}
                g_dbus_method_invocation_return_value(invocation,g_variant_new("((so))",local_name.c_str(),object->children[std::size_t(index)].c_str()));return;}
            if(name=="GetIndexInParent"){gint index=-1;if(auto p=model.object(object->parent)){auto found=std::find(p->children.begin(),p->children.end(),object->path);if(found!=p->children.end())index=static_cast<gint>(found-p->children.begin());}g_dbus_method_invocation_return_value(invocation,g_variant_new("(i)",index));return;}
            if(name=="GetApplication"){g_dbus_method_invocation_return_value(invocation,g_variant_new("((so))",local_name.c_str(),AtSpiApplication::root_path().data()));return;}
            if(name=="GetInterfaces"){GVariantBuilder values;g_variant_builder_init(&values,G_VARIANT_TYPE("as"));g_variant_builder_add(&values,"s",accessible);g_variant_builder_add(&values,"s",object->role==AtSpiRole::application?application:component);if(object->actionable)g_variant_builder_add(&values,"s",action);if(has_text(*object))g_variant_builder_add(&values,"s",text_interface);if(can_edit(*object))g_variant_builder_add(&values,"s",editable_text);if(object->numeric)g_variant_builder_add(&values,"s",value_interface);g_dbus_method_invocation_return_value(invocation,g_variant_new("(as)",&values));return;}
            if(name=="GetAttributes"){GVariantBuilder values;g_variant_builder_init(&values,G_VARIANT_TYPE("a{ss}"));g_dbus_method_invocation_return_value(invocation,g_variant_new("(a{ss})",&values));return;}
            if(name=="GetRelationSet"){GVariantBuilder values;g_variant_builder_init(&values,G_VARIANT_TYPE("a(ua(so))"));g_dbus_method_invocation_return_value(invocation,g_variant_new("(a(ua(so)))",&values));return;}
        }else if(iface==application){
            if(name=="GetLocale"){g_dbus_method_invocation_return_value(invocation,g_variant_new("(s)",locale()));return;}
            if(name=="GetApplicationBusAddress"){g_dbus_method_invocation_return_value(invocation,g_variant_new("(s)",""));return;}
        }else if(iface==action){
            if(name=="GetActions"){GVariantBuilder values;g_variant_builder_init(&values,G_VARIANT_TYPE("a(sss)"));if(object->actionable)g_variant_builder_add(&values,"(sss)","click","","");g_dbus_method_invocation_return_value(invocation,g_variant_new("(a(sss))",&values));return;}
            gint index=0;g_variant_get(params,"(i)",&index);
            if(name=="DoAction"){const bool accepted=index==0&&model.action(object->path,WidgetInputKind::activate)&&enqueue&&enqueue(object->path,WidgetInputKind::activate);g_dbus_method_invocation_return_value(invocation,g_variant_new("(b)",accepted));return;}
            if(index!=0||!object->actionable){error(invocation,"Action index is out of range");return;}
            if(name=="GetName"||name=="GetLocalizedName"||name=="GetDescription"||name=="GetKeyBinding"){
                const char* value=(name=="GetName"||name=="GetLocalizedName")?"click":"";
                g_dbus_method_invocation_return_value(invocation,g_variant_new("(s)",value));return;}
        }else if(iface==text_interface&&has_text(*object)){
            auto data=read_text(*object);if(!data){error(invocation,"Text snapshot is unavailable or changed");return;}
            const auto& index=data->index;const auto count=static_cast<gint>(index.characters());
            if(name=="GetText"){
                gint first=0,last=0;g_variant_get(params,"(ii)",&first,&last);if(last==-1)last=count;
                const auto value=first>=0&&last>=first?index.slice(static_cast<std::size_t>(first),static_cast<std::size_t>(last)):std::nullopt;
                if(!value){error(invocation,"Text range is out of bounds");return;}
                g_dbus_method_invocation_return_value(invocation,g_variant_new("(s)",std::string(*value).c_str()));return;
            }
            if(name=="GetCharacterAtOffset"){
                gint offset=0;g_variant_get(params,"(i)",&offset);const auto value=offset>=0?index.character(static_cast<std::size_t>(offset)):std::nullopt;
                if(!value){error(invocation,"Character offset is out of bounds");return;}
                g_dbus_method_invocation_return_value(invocation,g_variant_new("(i)",static_cast<gint>(*value)));return;
            }
            if(name=="GetStringAtOffset"){
                gint offset=0;guint granularity=0;g_variant_get(params,"(iu)",&offset,&granularity);
                if(offset<0||offset>count){error(invocation,"Character offset is out of bounds");return;}
                std::optional<TextRange> range;
                if(granularity==0)range=TextRange{static_cast<std::size_t>(offset),offset<count?1u:0u};
                if(!range){error(invocation,"Text granularity is unavailable");return;}
                const auto value=index.slice(range->location,range->location+range->length);
                if(!value){error(invocation,"Text range is unavailable");return;}
                g_dbus_method_invocation_return_value(invocation,g_variant_new("(sii)",std::string(*value).c_str(),static_cast<gint>(range->location),static_cast<gint>(range->location+range->length)));return;
            }
            const auto selection=index.selection();
            if(name=="GetNSelections"){g_dbus_method_invocation_return_value(invocation,g_variant_new("(i)",selection.length?1:0));return;}
            if(name=="GetSelection"){
                gint number=0;g_variant_get(params,"(i)",&number);if(number!=0||!selection.length){error(invocation,"Text selection is unavailable");return;}
                g_dbus_method_invocation_return_value(invocation,g_variant_new("(ii)",static_cast<gint>(selection.location),static_cast<gint>(selection.location+selection.length)));return;
            }
            gint first=0,last=0,number=0;bool valid=true;
            if(name=="SetCaretOffset"){g_variant_get(params,"(i)",&first);last=first;}
            else if(name=="AddSelection"){g_variant_get(params,"(ii)",&first,&last);valid=!selection.length&&first!=last;}
            else if(name=="SetSelection"){g_variant_get(params,"(iii)",&number,&first,&last);valid=number==0&&selection.length;}
            else if(name=="RemoveSelection"){g_variant_get(params,"(i)",&number);first=last=static_cast<gint>(index.caret());valid=number==0&&selection.length;}
            else {error(invocation,"Unsupported text method");return;}
            const bool accepted=valid&&submit_text(*object,*data,WidgetInputKind::text_selection,first,last);
            g_dbus_method_invocation_return_value(invocation,g_variant_new("(b)",accepted));return;
        }else if(iface==editable_text&&can_edit(*object)){
            auto data=read_text(*object);if(!data){error(invocation,"Text snapshot is unavailable or changed");return;}
            gint first=0,last=0,length=0;const gchar* raw=nullptr;std::string value;bool valid=true;
            if(name=="SetTextContents"){g_variant_get(params,"(&s)",&raw);value=raw;last=static_cast<gint>(data->index.characters());}
            else if(name=="InsertText"){
                g_variant_get(params,"(i&si)",&first,&raw,&length);last=first;const std::string_view supplied(raw);
                valid=length>=-1;if(valid)value=std::string(supplied.substr(0,length<0?supplied.size():std::min(supplied.size(),static_cast<std::size_t>(length))));
            }else if(name=="DeleteText")g_variant_get(params,"(ii)",&first,&last);
            else if(name=="CutText"||name=="PasteText"){g_dbus_method_invocation_return_value(invocation,g_variant_new("(b)",FALSE));return;}
            else {error(invocation,"Clipboard operation is unavailable");return;}
            const bool accepted=valid&&submit_text(*object,*data,WidgetInputKind::text,first,last,std::move(value));
            g_dbus_method_invocation_return_value(invocation,g_variant_new("(b)",accepted));return;
        }else if(iface==component){
            if(name=="GrabFocus"){const bool accepted=focusable(*object)&&model.action(object->path,WidgetInputKind::focus)&&enqueue&&enqueue(object->path,WidgetInputKind::focus);g_dbus_method_invocation_return_value(invocation,g_variant_new("(b)",accepted));return;}
            if(name=="GetSize"){g_dbus_method_invocation_return_value(invocation,g_variant_new("(ii)",integer(object->bounds.width),integer(object->bounds.height)));return;}
            if(name=="GetLayer"){g_dbus_method_invocation_return_value(invocation,g_variant_new("(u)",object->role==AtSpiRole::frame?7u:3u));return;}
            if(name=="GetMDIZOrder"){g_dbus_method_invocation_return_value(invocation,g_variant_new("(n)",gint16(-1)));return;}
            guint coordinates=0;gint point_x=0,point_y=0;
            if(name=="GetExtents"||name=="GetPosition")g_variant_get(params,"(u)",&coordinates);
            else if(name=="Contains"||name=="GetAccessibleAtPoint")g_variant_get(params,"(iiu)",&point_x,&point_y,&coordinates);
            else {error(invocation,"Unsupported accessibility component method");return;}
            double x=0,y=0;if(!geometry(*object,coordinates,x,y)){error(invocation,"Requested coordinate system is unavailable");return;}
            if(name=="GetExtents"){g_dbus_method_invocation_return_value(invocation,g_variant_new("((iiii))",integer(x),integer(y),integer(object->bounds.width),integer(object->bounds.height)));return;}
            if(name=="GetPosition"){g_dbus_method_invocation_return_value(invocation,g_variant_new("(ii)",integer(x),integer(y)));return;}
            const bool inside=point_x>=x&&point_y>=y&&point_x<x+object->bounds.width&&point_y<y+object->bounds.height;
            if(name=="Contains"){g_dbus_method_invocation_return_value(invocation,g_variant_new("(b)",inside));return;}
            auto selected=inside?hit(*object,object->bounds.x+point_x-x,object->bounds.y+point_y-y):std::nullopt;
            g_dbus_method_invocation_return_value(invocation,g_variant_new("((so))",selected?local_name.c_str():"",selected?selected->path.c_str():null_path));return;
        }
        error(invocation,"Unsupported accessibility method");
    }
    static void method(GDBusConnection*,const gchar*,const gchar* path,const gchar* iface,const gchar* name,GVariant* params,GDBusMethodInvocation* invocation,gpointer data){
        try{auto held=static_cast<std::weak_ptr<Impl>*>(data)->lock();if(!held||!held->connection){error(invocation,"Accessibility service is closed");return;}held->method_impl(path,iface,name,params,invocation);}catch(...){error(invocation,"Accessibility method failed");}
    }
    static void release(gpointer data){delete static_cast<std::weak_ptr<Impl>*>(data);}
};

AtSpiDbusBridge::AtSpiDbusBridge(AtSpiApplication& model,Action action,TextCallbacks text,ValueInput value):impl_(std::make_shared<Impl>(model,std::move(action),std::move(text),std::move(value))){}
AtSpiDbusBridge::~AtSpiDbusBridge(){if(impl_)impl_->detach();}
bool AtSpiDbusBridge::attach(GDBusConnection* connection,std::string local_name){
    auto held=impl_;if(!connection||local_name.empty()||held->connection||held->reconciling)return false;
    static const GDBusSubtreeVTable table{Impl::enumerate,Impl::introspect,Impl::dispatch,{}};
    auto subtree_data=std::make_unique<std::weak_ptr<Impl>>(held);
    GError* reason=nullptr;const auto id=g_dbus_connection_register_subtree(connection,subtree,&table,G_DBUS_SUBTREE_FLAGS_DISPATCH_TO_UNENUMERATED_NODES,subtree_data.release(),Impl::release,&reason);
    if(!id){if(reason)g_error_free(reason);return false;}
    held->connection=G_DBUS_CONNECTION(g_object_ref(connection));const auto epoch=++held->attachment_epoch;held->registration=id;held->local_name=std::move(local_name);
    auto cache_data=std::make_unique<std::weak_ptr<Impl>>(held);
    held->cache_registration=g_dbus_connection_register_object(connection,cache_path,held->find(cache_interface),Impl::interface_table(),cache_data.release(),Impl::release,&reason);
    if(!held->cache_registration){if(reason)g_error_free(reason);held->detach();return false;}
    try{held->reconcile(false);}catch(...){held->detach();return false;}
    return held->connection&&held->attachment_epoch==epoch;
}
void AtSpiDbusBridge::registry_parent(std::string bus,std::string path){
    if(bus.empty()||!g_variant_is_object_path(path.c_str()))return;impl_->parent_bus=std::move(bus);impl_->parent_path=std::move(path);
}
void AtSpiDbusBridge::changed()noexcept{try{auto held=impl_;if(held->connection)held->reconcile(true);}catch(...){}}
void AtSpiDbusBridge::detach()noexcept{impl_->detach();}
} // namespace ii::gui::detail
