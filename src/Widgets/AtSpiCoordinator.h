#pragma once
#include "AtSpiApplication.h"
#include <cmath>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace ii::gui::detail {
// App-owned, owner-thread-only mediation between native windows, the immutable
// accessibility projection and an asynchronous transport. Posting never owns
// the application and requests are re-resolved immediately before dispatch.
class AtSpiCoordinator final : public std::enable_shared_from_this<AtSpiCoordinator> {
public:
    using Post=std::function<bool(std::function<void()>)>;
    using Input=std::function<void(WidgetInput)>;
    using TextReader=std::function<std::optional<AtSpiTextIndex>(const AtSpiTextTarget&)>;
    struct ValueCallbacks final {
        std::function<bool(std::string_view,double)> can_set;
        std::function<void(std::string_view,double)> set;
    };
    using Changed=std::function<void(std::string)>;
private:
    struct Window final {std::optional<AtSpiSurfaceToken> surface;Input input;TextReader read_text;ValueCallbacks value;};
    const std::thread::id owner_=std::this_thread::get_id();
    AtSpiApplication model_;
    Post post_;
    Changed changed_;
    std::unordered_map<std::uint64_t,Window> windows_;
    explicit AtSpiCoordinator(std::string name,Post post):model_(std::move(name)),post_(std::move(post)){}
    [[nodiscard]] bool on_owner()const noexcept{return owner_==std::this_thread::get_id();}
    void changed(std::string path)noexcept{try{if(changed_)changed_(std::move(path));}catch(...){}}
public:
    [[nodiscard]] static std::shared_ptr<AtSpiCoordinator> create(std::string name,Post post){
        return std::shared_ptr<AtSpiCoordinator>(new AtSpiCoordinator(std::move(name),std::move(post)));
    }
    AtSpiCoordinator(const AtSpiCoordinator&)=delete;
    AtSpiCoordinator& operator=(const AtSpiCoordinator&)=delete;
    [[nodiscard]] AtSpiApplication& model()noexcept{return model_;}
    void on_changed(Changed value){if(on_owner())changed_=std::move(value);}
    [[nodiscard]] std::optional<std::uint64_t> open_window(std::string title,Size size,std::optional<WidgetPoint> origin=std::nullopt){
        if(!on_owner())return std::nullopt;auto id=model_.open_window(std::move(title),size,origin);if(!id)return std::nullopt;
        try{windows_.emplace(*id,Window{});}catch(...){(void)model_.close_window(*id);throw;}
        changed(std::string(AtSpiApplication::root_path()));return id;
    }
    [[nodiscard]] bool geometry(std::uint64_t id,std::string title,Size size,std::optional<WidgetPoint> origin=std::nullopt){
        if(!on_owner()||!model_.update_window(id,std::move(title),size,origin))return false;
        changed(AtSpiApplication::window_path(id));return true;
    }
    [[nodiscard]] bool close_window(std::uint64_t id){
        if(!on_owner()||!model_.close_window(id))return false;windows_.erase(id);
        changed(std::string(AtSpiApplication::root_path()));return true;
    }
    [[nodiscard]] bool publish(std::uint64_t id,const std::vector<WidgetSnapshot>& snapshots,Input input,TextReader read_text={},ValueCallbacks value={}){
        if(!on_owner())return false;const auto found=windows_.find(id);if(found==windows_.end()||!input)return false;
        auto token=model_.stage(id,snapshots);if(!token)return false;
        if(!model_.commit(*token)){(void)model_.retire(*token);return false;}
        found->second.surface=*token;found->second.input=std::move(input);found->second.read_text=std::move(read_text);found->second.value=std::move(value);changed(AtSpiApplication::window_path(id));return true;
    }
    [[nodiscard]] bool update(std::uint64_t id,const std::vector<WidgetSnapshot>& snapshots){
        if(!on_owner())return false;const auto found=windows_.find(id);if(found==windows_.end()||!found->second.surface)return false;
        if(!model_.update(*found->second.surface,snapshots))return false;
        changed(AtSpiApplication::window_path(id));return true;
    }
    [[nodiscard]] bool unpublish(std::uint64_t id){
        if(!on_owner())return false;const auto found=windows_.find(id);if(found==windows_.end()||!found->second.surface)return false;
        if(!model_.retire(*found->second.surface))return false;
        found->second.surface.reset();found->second.input={};found->second.read_text={};found->second.value={};changed(AtSpiApplication::window_path(id));return true;
    }
    [[nodiscard]] std::optional<AtSpiTextData> text(std::string_view path){
        if(!on_owner())return std::nullopt;auto id=model_.window_of(path);auto target=model_.text_target(path);if(!id||!target)return std::nullopt;
        const auto found=windows_.find(*id);if(found==windows_.end()||!found->second.read_text)return std::nullopt;
        try{auto read=found->second.read_text;auto index=read(*target);if(!index||model_.text_target(path)!=target)return std::nullopt;
            return AtSpiTextData{std::move(*target),std::move(*index)};
        }catch(...){return std::nullopt;}
    }
    [[nodiscard]] bool request_text(std::string path,WidgetInput input){
        if(!on_owner()||!post_||(input.kind!=WidgetInputKind::text&&input.kind!=WidgetInputKind::text_selection)||
           !input.replacement_range||!input.expected_text_revision)return false;
        auto id=model_.window_of(path);auto target=model_.text_target(path);
        if(!id||!target||!target->focused||!target->enabled||target->revision!=*input.expected_text_revision||
           (input.kind==WidgetInputKind::text&&target->read_only)||input.replacement_range->location>target->length||
           input.replacement_range->length>target->length-input.replacement_range->location)return false;
        if(input.kind==WidgetInputKind::text&&(input.text.size()>65536||input.text.find('\0')!=std::string::npos||!TextIndex::create(input.text)))return false;
        const auto found=windows_.find(*id);if(found==windows_.end()||!found->second.input)return false;
        try{auto weak=weak_from_this();return post_([weak,path=std::move(path),input=std::move(input)]()mutable{
            auto self=weak.lock();if(!self)return;auto id=self->model_.window_of(path);auto target=self->model_.text_target(path);
            if(!id||!target||!target->focused||!target->enabled||target->revision!=*input.expected_text_revision||
               (input.kind==WidgetInputKind::text&&target->read_only))return;
            const auto found=self->windows_.find(*id);if(found==self->windows_.end())return;
            auto deliver=found->second.input;if(!deliver)return;input.target=target->id;input.text_service=true;deliver(std::move(input));
        });}catch(...){return false;}
    }
    [[nodiscard]] bool request_value(std::string path,double value){
        if(!on_owner()||!post_||!std::isfinite(value))return false;
        [[maybe_unused]] auto keep_alive=shared_from_this();
        const auto matches=[&](const std::optional<AtSpiObject>& object,const std::string& id,const AccessibilityRange& expected){
            return object&&object->id==id&&object->role==AtSpiRole::slider&&object->enabled&&object->numeric&&
                object->numeric->minimum==expected.minimum&&object->numeric->maximum==expected.maximum&&
                value>=object->numeric->minimum&&value<=object->numeric->maximum;
        };
        auto window=model_.window_of(path);auto object=model_.object(path);
        if(!window||!object||!object->numeric||!matches(object,object->id,*object->numeric))return false;
        const auto found=windows_.find(*window);if(found==windows_.end()||!found->second.value.can_set||!found->second.value.set)return false;
        const auto id=object->id;const auto expected=*object->numeric;auto can=found->second.value.can_set;
        try{
            if(!can(id,value)||!matches(model_.object(path),id,expected))return false;
            auto weak=weak_from_this();return post_([weak,path=std::move(path),id,expected,value]{try{
                auto self=weak.lock();if(!self)return;auto window=self->model_.window_of(path);auto object=self->model_.object(path);
                if(!window||!object||object->id!=id||object->role!=AtSpiRole::slider||!object->enabled||!object->numeric||
                   object->numeric->minimum!=expected.minimum||object->numeric->maximum!=expected.maximum||value<expected.minimum||value>expected.maximum)return;
                const auto found=self->windows_.find(*window);if(found==self->windows_.end())return;
                auto can=found->second.value.can_set;if(!can||!can(id,value))return;
                object=self->model_.object(path);if(!object||object->id!=id||object->role!=AtSpiRole::slider||!object->enabled||!object->numeric||
                   object->numeric->minimum!=expected.minimum||object->numeric->maximum!=expected.maximum)return;
                const auto live=self->windows_.find(*window);if(live==self->windows_.end())return;auto set=live->second.value.set;if(set)set(id,value);
            }catch(...){}});
        }catch(...){return false;}
    }
    [[nodiscard]] bool request(std::string path,WidgetInputKind kind){
        if(!on_owner())return false;auto id=model_.window_of(path);if(!id||!model_.action(path,kind)||!post_)return false;
        const auto found=windows_.find(*id);if(found==windows_.end()||!found->second.input)return false;
        try{auto weak=weak_from_this();return post_([weak,path=std::move(path),kind]{
            auto self=weak.lock();if(!self)return;
            auto id=self->model_.window_of(path);if(!id)return;
            auto input=self->model_.action(path,kind);if(!input)return;
            const auto found=self->windows_.find(*id);if(found==self->windows_.end())return;
            auto deliver=found->second.input;if(deliver)deliver(std::move(*input));
        });}catch(...){return false;}
    }
};
} // namespace ii::gui::detail
