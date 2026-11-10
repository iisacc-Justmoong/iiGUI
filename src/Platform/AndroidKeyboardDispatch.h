#pragma once
#include "AndroidKeyboard.h"
#include <memory>
#include <optional>
#include <string>

namespace ii::gui::detail {
// The host owns focus, text and the clipboard. Its send() returns false if a
// callback retired the surface, so no subsequent event targets stale content.
class AndroidKeyboardDispatcher final {
    AndroidKeyboardState state_;
    std::optional<std::string> focus_;
    bool observed_focus_=false,paste_down_=false;
    static bool same_focus(const std::optional<WidgetSnapshot>& a,const std::optional<WidgetSnapshot>& b){
        if(bool(a)!=bool(b))return false;
        return !a||(a->id==b->id&&a->kind==b->kind&&a->text_input==b->text_input);
    }
    static bool editor(const std::optional<WidgetSnapshot>& value){
        return value&&value->text_input&&value->focused&&value->enabled&&value->visible&&!value->input_blocked;
    }
    static char shortcut(std::uint32_t value)noexcept{
        if(value>='A'&&value<='Z')value+=('a'-'A');
        return value<128?char(value):'\0';
    }
    template<class Host>
    bool paste(Host& host,const WidgetSnapshot& field){
        if(field.read_only)return true;
        auto value=host.read_clipboard();if(!value||value->text.empty())return true;
        if(!same_focus(std::optional{field},host.focus()))return true;
        WidgetInput text;text.kind=WidgetInputKind::text;
        if(field.kind==WidgetKind::input_field)text.text=value->text;
        else text.content=std::make_shared<const ClipboardContent>(std::move(*value));
        (void)host.send(std::move(text));return true;
    }
    template<class Host>
    bool legacy_copy(Host& host,const WidgetSnapshot& field,bool cut,WidgetInput input){
        if(field.password||field.text_selection.length==0)return true;
        auto content=host.read(field.id,field.text_selection);
        if(!content||content->range!=field.text_selection)return true;
        if(!host.write_clipboard(ClipboardContent{.text=std::move(content->text)}))return true;
        auto current=host.focus();
        if(cut&&!field.read_only&&same_focus(std::optional{field},current)&&current->text_selection==field.text_selection&&
           current->text==field.text){input.key=WidgetKey::cut;(void)host.send(std::move(input));}
        return true;
    }
public:
    void reset()noexcept{state_.reset();focus_.reset();observed_focus_=false;paste_down_=false;}
    template<class Host,class Combine>
    bool input(AndroidKeyboardFrame frame,Host& host,Combine&& combine){
        auto focused=host.focus();const auto id=focused?std::optional{focused->id}:std::nullopt;
        if(observed_focus_&&focus_!=id)state_.reset();
        focus_=id;observed_focus_=true;
        const bool has_editor=editor(focused);
        if(frame.paste){
            if(!frame.down){const bool handled=paste_down_;paste_down_=false;return handled;}
            if(!has_editor)return false;
            paste_down_=true;state_.reset();return paste(host,*focused);
        }
        if(frame.down&&!frame.canceled&&has_editor){
            const auto letter=shortcut(frame.shortcut);
            const bool command=(frame.control||frame.meta)&&!frame.alt;
            const bool copy=frame.key==WidgetKey::copy||(command&&letter=='c');
            const bool cut=frame.key==WidgetKey::cut||(command&&letter=='x');
            if(command&&letter=='v'){state_.reset();return paste(host,*focused);}
            if(command&&letter=='a')frame.key=WidgetKey::select_all;
            if(copy||cut){
                state_.reset();frame.key=cut?WidgetKey::cut:WidgetKey::copy;
                auto translated=state_.translate(frame,combine);
                if(translated.events.empty())return true;
                if(focused->kind==WidgetKind::input_field)return legacy_copy(host,*focused,cut,std::move(translated.events[0]));
                (void)host.send(std::move(translated.events[0]));return true;
            }
        }
        auto translated=state_.translate(frame,std::forward<Combine>(combine));
        if(!translated.handled)return false;
        if(has_editor&&focused->kind==WidgetKind::input_field&&frame.down&&frame.key==WidgetKey::space){
            if(translated.events.size()==2&&translated.events[1].text==" ")translated.events.pop_back();
            else if(!translated.events.empty())translated.events[0].key=WidgetKey::character;
        }
        for(auto& event:translated.events){
            if(event.kind==WidgetInputKind::text&&!same_focus(focused,host.focus())){state_.reset();break;}
            if(!host.send(std::move(event)))break;
        }
        return true;
    }
};
} // namespace ii::gui::detail
