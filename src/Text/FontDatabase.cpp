#include <iiGUI/FontDatabase.h>
#include <iiGUI/FontNative.h>
#include "FontAssets.generated.h"
#include "../third_party/utf8proc/utf8proc.h"
#include <iiSDKCore/Core/Property/Observable.h>
#include <iiSDKCore/Core/Text/Utf8.h>
#include <algorithm>
#include <cstdlib>
#include <map>
#include <thread>

namespace ii::gui {
namespace {
void name(std::string_view value,bool empty=true){if((!empty&&value.empty())||value.size()>4096||value.find('\0')!=value.npos||!core::valid_utf8(value))throw std::invalid_argument("Font names require bounded UTF-8 without NUL");}
void face(const FontFace& value){name(value.family,false);name(value.style);if(value.weight<1||value.weight>1000)throw std::invalid_argument("Font weight is outside 1..1000");}
template<class F>auto attempt(F&& action)->std::invoke_result_t<F&>{try{return action();}catch(...){return core::fail(core::ErrorCode::failed,"Font operation failed",std::current_exception());}}
}
struct FontDatabase::Impl final {
    const std::thread::id owner=std::this_thread::get_id();
    std::shared_ptr<FontProvider> provider;
    std::vector<std::string> families;
    std::map<std::string,std::string,std::less<>> index;
    std::string system;
    std::optional<FontFace> application;
    std::map<std::string,std::vector<std::string>,std::less<>> fallback;
    core::Observable<std::uint64_t> epoch{0};
    FontLoadReport loaded;
    bool attempted=false,busy=false,closed=false;
    void readable()const{if(owner!=std::this_thread::get_id()||closed)throw std::logic_error("Font database requires its active owner thread");}
    core::Result<void> writable()const{
        if(owner!=std::this_thread::get_id())return core::fail(core::ErrorCode::wrong_executor,"Font database requires its owner thread");
        if(closed)return core::fail(core::ErrorCode::closed,"Font database is closed");
        if(busy)return core::fail(core::ErrorCode::busy,"Font database notification is read-only");return {};
    }
    void read()const{readable();(void)epoch.get();}
    core::Result<void> changed(){busy=true;struct Reset{bool& flag;~Reset(){flag=false;}}reset{busy};try{epoch.set(epoch.get()+1);return {};}catch(...){return core::fail(core::ErrorCode::failed,"Font observation failed after commit",std::current_exception());}}
};
FontDatabase::FontDatabase(std::shared_ptr<FontProvider> provider,std::optional<FontFace> application):impl_(std::make_shared<Impl>()){
    if(!provider)throw std::invalid_argument("Font database requires a provider");if(application)face(*application);
    impl_->provider=std::move(provider);impl_->application=std::move(application);
    if(auto result=refresh();!result){if(result.error().cause)std::rethrow_exception(result.error().cause);throw std::runtime_error(result.error().message);}
}
FontDatabase::~FontDatabase(){impl_->closed=true;}
std::shared_ptr<FontDatabase> FontDatabase::application(){static thread_local std::shared_ptr<FontDatabase> current=native();return current;}
std::shared_ptr<FontDatabase> FontDatabase::native(std::string directory){name(directory);auto provider=detail::native_font_provider(std::move(directory));auto system=provider->system_family();return std::make_shared<FontDatabase>(std::move(provider),FontFace{system,400,"Regular"});}
std::span<const FontResource> FontDatabase::bundled_fonts()noexcept{return detail::font_assets;}
std::string FontDatabase::fold_name(std::string_view value){
    name(value);if(value.empty())return {};
    utf8proc_uint8_t* output=nullptr;const auto count=utf8proc_map(reinterpret_cast<const utf8proc_uint8_t*>(value.data()),utf8proc_ssize_t(value.size()),&output,utf8proc_option_t(UTF8PROC_STABLE|UTF8PROC_CASEFOLD));
    std::unique_ptr<void,decltype(&std::free)> released(output,&std::free);
    if(count<0)throw std::invalid_argument("Cannot case-fold a font name");return std::string(reinterpret_cast<const char*>(output),std::size_t(count));
}
std::vector<std::string> FontDatabase::families()const{auto p=impl_;p->read();return p->families;}
std::string FontDatabase::system_family()const{auto p=impl_;p->readable();return p->system;}
std::optional<FontFace> FontDatabase::application_font()const{auto p=impl_;p->read();return p->application;}
bool FontDatabase::family_available(std::string_view value)const{auto p=impl_;p->read();return p->index.contains(fold_name(value));}
std::string FontDatabase::resolve(const FontFace& value)const{auto p=impl_;p->read();face(value);auto result=p->provider->resolve(value);name(result);return result;}
std::vector<std::string> FontDatabase::substitutions(std::string_view value)const{auto p=impl_;p->read();const auto found=p->fallback.find(fold_name(value));return found==p->fallback.end()?std::vector<std::string>{}:found->second;}
std::vector<std::string> FontDatabase::family_stack(std::string_view requested)const{
    auto p=impl_;p->read();name(requested);std::vector<std::string> result;
    const auto append=[&](std::string value){if(value.empty())return;const auto key=fold_name(value);if(std::ranges::none_of(result,[&](const auto& entry){return fold_name(entry)==key;}))result.push_back(std::move(value));};
    if(!requested.empty())append(std::string(requested));
    for(auto& family:substitutions(requested))append(std::move(family));
    if(p->application)append(p->application->family);append(p->system);return result;
}
std::string FontDatabase::resolve_family(std::string_view requested)const{
    auto p=impl_;p->read();if(auto found=p->index.find(fold_name(requested));found!=p->index.end())return found->second;
    for(const auto& family:family_stack(requested))if(auto found=p->index.find(fold_name(family));found!=p->index.end())return found->second;
    return system_family();
}
std::uint64_t FontDatabase::revision()const{auto p=impl_;p->read();return p->epoch.get();}
core::Result<bool> FontDatabase::refresh(){
    auto p=impl_;if(auto ready=p->writable();!ready)return std::unexpected(ready.error());
    return attempt([&]()->core::Result<bool>{
        auto families=p->provider->families();if(families.size()>65536)throw std::length_error("Font family catalogue is too large");
        std::map<std::string,std::string,std::less<>> index;
        for(const auto& family:families){name(family,false);index.try_emplace(fold_name(family),family);}auto system=p->provider->system_family();name(system);
        if(families==p->families&&system==p->system)return false;
        p->families=std::move(families);p->index=std::move(index);p->system=std::move(system);if(auto changed=p->changed();!changed)return std::unexpected(changed.error());return true;
    });
}
core::Result<FontLoadReport> FontDatabase::load_bundled_fonts(){
    auto p=impl_;if(auto ready=p->writable();!ready)return std::unexpected(ready.error());
    if(p->attempted)return p->loaded;
    return attempt([&]()->core::Result<FontLoadReport>{
        auto loaded=p->provider->load(bundled_fonts());
        if(loaded.resources.size()!=bundled_fonts().size())throw std::invalid_argument("Font provider omitted a bundled resource result");
        for(std::size_t i=0;i<loaded.resources.size();++i)if(loaded.resources[i].file!=bundled_fonts()[i].file)throw std::invalid_argument("Font provider changed bundled resource ordering");
        p->loaded=std::move(loaded);p->attempted=true;auto refreshed=refresh();if(!refreshed)return std::unexpected(refreshed.error());
        if(!*refreshed&&std::ranges::any_of(p->loaded.resources,[](const auto& font){return font.loaded;}))if(auto changed=p->changed();!changed)return std::unexpected(changed.error());
        return p->loaded;
    });
}
core::Result<bool> FontDatabase::set_application_font(FontFace value){
    auto p=impl_;if(auto ready=p->writable();!ready)return std::unexpected(ready.error());
    return attempt([&]()->core::Result<bool>{face(value);if(!p->application||*p->application==value)return false;p->application=std::move(value);if(auto changed=p->changed();!changed)return std::unexpected(changed.error());return true;});
}
core::Result<bool> FontDatabase::install_substitution(std::string target,std::string replacement){
    auto p=impl_;if(auto ready=p->writable();!ready)return std::unexpected(ready.error());
    return attempt([&]()->core::Result<bool>{name(target,false);name(replacement,false);auto key=fold_name(target);auto found=p->fallback.find(key);if(found!=p->fallback.end())for(const auto& value:found->second)if(fold_name(value)==fold_name(replacement))return false;
        auto next=found==p->fallback.end()?std::vector<std::string>{}:found->second;next.push_back(std::move(replacement));p->fallback.insert_or_assign(std::move(key),std::move(next));if(auto changed=p->changed();!changed)return std::unexpected(changed.error());return true;});
}
core::ObservableSubscription FontDatabase::observe(std::function<void()> callback)const{auto p=impl_;p->readable();if(!callback)throw std::invalid_argument("Font observation requires a callback");const std::weak_ptr<Impl> weak=p;return p->epoch.observe([weak,callback=std::move(callback)](auto,auto){if(auto state=weak.lock();state&&!state->closed)callback();});}
}
