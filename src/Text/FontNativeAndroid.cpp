#include <iiGUI/FontNative.h>
#include <iiGUI/FontMetadata.h>
#include <iiGUI/AndroidSurface.h>
#include <iiGUI/AndroidLabelFont.h>
#include <android/asset_manager.h>
#include <algorithm>
#include <fstream>
#include <map>
#include <mutex>
#include <dlfcn.h>
#include <sstream>
#include <deque>

struct AFont;
struct AFontMatcher;
struct ASystemFontIterator;

namespace ii::gui::detail {
namespace {
struct Face final {std::string family;int weight=400;std::filesystem::path file;std::shared_ptr<AndroidLabelFont> loaded;unsigned collection_index=0;};
struct Registry final {std::mutex mutex;bool system_loaded=false,index_dirty=true;std::string system;std::vector<Face> faces;std::map<std::string,std::vector<std::size_t>,std::less<>> index;std::deque<std::size_t> recent_system;};
Registry& registry(){static Registry value;return value;}
struct NativeApi final {
    void* library=dlopen("libandroid.so",RTLD_NOW|RTLD_LOCAL);
    AFontMatcher* (*create)()=nullptr;void (*destroy)(AFontMatcher*)=nullptr;
    AFont* (*match)(const AFontMatcher*,const char*,const std::uint16_t*,std::uint32_t,std::uint32_t*)=nullptr;
    void (*close)(AFont*)=nullptr;const char* (*path)(const AFont*)=nullptr;std::size_t (*index)(const AFont*)=nullptr;
    NativeApi(){if(!library)return;create=reinterpret_cast<decltype(create)>(dlsym(library,"AFontMatcher_create"));destroy=reinterpret_cast<decltype(destroy)>(dlsym(library,"AFontMatcher_destroy"));match=reinterpret_cast<decltype(match)>(dlsym(library,"AFontMatcher_match"));close=reinterpret_cast<decltype(close)>(dlsym(library,"AFont_close"));path=reinterpret_cast<decltype(path)>(dlsym(library,"AFont_getFontFilePath"));index=reinterpret_cast<decltype(index)>(dlsym(library,"AFont_getCollectionIndex"));}
    ~NativeApi(){if(library)dlclose(library);}
    bool available()const{return create&&destroy&&match&&close&&path&&index;}
};
std::string attribute(std::string_view tag,std::string_view key){
    std::size_t at=0;while((at=tag.find(key,at))!=tag.npos){const auto end=at+key.size();if(at&&tag[at-1]!=' '&&tag[at-1]!='\t'&&tag[at-1]!='\n'){at=end;continue;}auto value=tag.substr(end);while(!value.empty()&&(value.front()==' '||value.front()=='\t'))value.remove_prefix(1);if(!value.starts_with('=')){at=end;continue;}value.remove_prefix(1);while(!value.empty()&&(value.front()==' '||value.front()=='\t'))value.remove_prefix(1);if(value.empty()||(value.front()!='\''&&value.front()!='"'))return {};const char quote=value.front();value.remove_prefix(1);const auto count=value.find(quote);return count==value.npos?std::string{}:std::string(value.substr(0,count));}return {};
}
std::string legacy_system_family(){
    std::ifstream stream("/system/etc/fonts.xml",std::ios::binary|std::ios::ate);if(!stream)return {};const auto size=stream.tellg();if(size<=0||size>4*1024*1024)return {};std::string xml(std::size_t(size),'\0');stream.seekg(0);if(!stream.read(xml.data(),size))return {};
    std::size_t at=0;while((at=xml.find("<family",at))!=xml.npos){const auto end=xml.find('>',at);if(end==xml.npos)return {};const auto tag=std::string_view(xml).substr(at,end-at+1);at=end+1;if(tag.size()>7&&tag[7]!=' '&&tag[7]!='>'&&tag[7]!='\n')continue;const auto family=attribute(tag,"name");if(!family.empty()&&family!="sans-serif")continue;const auto close=xml.find("</family>",at);if(close==xml.npos)return {};std::string_view block(xml.data()+at,close-at);
        std::size_t font=0;while((font=block.find("<font",font))!=block.npos){const auto body=block.find('>',font),finish=block.find('<',body==block.npos?font:body+1);if(body==block.npos||finish==block.npos)break;auto file=block.substr(body+1,finish-body-1);const auto first=file.find_first_not_of(" \t\r\n"),last=file.find_last_not_of(" \t\r\n");if(first!=file.npos){file=file.substr(first,last-first+1);for(const auto* directory:{"/system/fonts","/product/fonts"})if(auto value=inspect_font_metadata(std::filesystem::path(directory)/std::string(file)))return value->family;}font=finish+1;}}
    return {};
}
void system_fonts(Registry& state){
    if(state.system_loaded)return;state.system_loaded=true;
    NativeApi api;
    if(api.available()){
        auto matcher=api.create();if(matcher){const std::uint16_t text[]{'A'};auto font=api.match(matcher,"sans-serif",text,1,nullptr);if(font){const auto* path=api.path(font);const auto index=api.index(font);if(path&&index<4096)if(auto metadata=inspect_font_metadata(path,unsigned(index))){state.system=metadata->family;state.faces.push_back({metadata->family,metadata->weight,path,{},unsigned(index)});}api.close(font);}api.destroy(matcher);}
    }
    if(state.system.empty())state.system=legacy_system_family();
    for(const auto* directory:{"/system/fonts","/product/fonts"}){
        std::error_code error;std::filesystem::directory_iterator it(directory,error),end;
        while(!error&&it!=end){const auto path=it->path();const auto extension=path.extension();if(extension==".ttf"||extension==".otf"||extension==".ttc")if(auto metadata=inspect_font_metadata(path))state.faces.push_back({metadata->family,metadata->weight,path,{}});it.increment(error);}
    }
}
std::vector<std::uint8_t> read_file(const std::filesystem::path& file){std::ifstream stream(file,std::ios::binary|std::ios::ate);if(!stream)throw std::runtime_error("Cannot open native font face");const auto size=stream.tellg();if(size<12||size>16*1024*1024)throw std::runtime_error("Native font face exceeds the renderer budget");std::vector<std::uint8_t> bytes(std::size_t(size),0);stream.seekg(0);if(!stream.read(reinterpret_cast<char*>(bytes.data()),std::streamsize(bytes.size())))throw std::runtime_error("Cannot read native font face");return bytes;}
std::vector<std::uint8_t> read_asset(std::string_view file){
    auto* app=android_application();if(!app)throw std::runtime_error("Android font registration requires an activity context");std::string name(file);
    auto* asset=AAssetManager_open(app->activity->assetManager,name.c_str(),AASSET_MODE_BUFFER);if(!asset)throw std::runtime_error("APK is missing a bundled font asset");
    struct Close{AAsset* value;~Close(){AAsset_close(value);}}close{asset};const auto size=AAsset_getLength64(asset);if(size<12||size>16*1024*1024)throw std::runtime_error("Invalid native font asset size");
    std::vector<std::uint8_t> bytes(std::size_t(size),0);std::size_t offset=0;while(offset<bytes.size()){const auto read=AAsset_read(asset,bytes.data()+offset,bytes.size()-offset);if(read<=0)throw std::runtime_error("Cannot read native font asset");offset+=std::size_t(read);}return bytes;
}
class Provider final:public FontProvider {
    std::string directory_;
public:
    explicit Provider(std::string directory):directory_(std::move(directory)){}
    std::vector<std::string> families()const override{auto& state=registry();std::lock_guard guard(state.mutex);system_fonts(state);std::vector<std::string> result;for(const auto& face:state.faces)result.push_back(face.family);std::ranges::sort(result);result.erase(std::unique(result.begin(),result.end()),result.end());return result;}
    std::string system_family()const override{auto& state=registry();std::lock_guard guard(state.mutex);system_fonts(state);if(state.system.empty())throw std::runtime_error("Android system font could not be identified from the native matcher or system configuration");return state.system;}
    std::string resolve(const FontFace& face)const override{for(const auto& name:families())if(FontDatabase::fold_name(name)==FontDatabase::fold_name(face.family))return name;return system_family();}
    FontLoadReport load(std::span<const FontResource> resources)override{
        auto& state=registry();std::lock_guard guard(state.mutex);system_fonts(state);FontLoadReport result;
        for(const auto& resource:resources){FontLoad loaded{std::string(resource.file)};
            try{auto bytes=directory_.empty()?read_asset(resource.file):read_file(std::filesystem::path(directory_)/std::string(resource.file));auto font=std::make_shared<AndroidLabelFont>(std::move(bytes));
                auto found=std::find_if(state.faces.begin(),state.faces.end(),[&](const auto& entry){return entry.family==resource.family&&entry.weight==resource.weight;});Face face{std::string(resource.family),resource.weight,{},std::move(font)};
                if(found==state.faces.end())state.faces.push_back(std::move(face));else *found=std::move(face);state.index_dirty=true;loaded.loaded=true;
            }catch(const std::exception& error){loaded.error=error.what();}result.resources.push_back(std::move(loaded));}return result;
    }
};
}
std::shared_ptr<AndroidLabelFont> native_android_font(std::string_view family,int weight){
    auto& state=registry();std::lock_guard guard(state.mutex);system_fonts(state);Face* selected=nullptr;const auto key=FontDatabase::fold_name(family);
    if(state.index_dirty){std::map<std::string,std::vector<std::size_t>,std::less<>> index;for(std::size_t i=0;i<state.faces.size();++i)index[FontDatabase::fold_name(state.faces[i].family)].push_back(i);state.index=std::move(index);state.index_dirty=false;}
    if(const auto found=state.index.find(key);found!=state.index.end())for(auto i:found->second){auto& face=state.faces[i];if(!selected||std::abs(face.weight-weight)<std::abs(selected->weight-weight))selected=&face;}
    if(!selected)throw std::runtime_error("The selected native font is unavailable");if(!selected->loaded)selected->loaded=std::make_shared<AndroidLabelFont>(read_file(selected->file),selected->collection_index);
    if(!selected->file.empty()){const auto index=std::size_t(selected-state.faces.data());std::erase(state.recent_system,index);state.recent_system.push_back(index);while(state.recent_system.size()>8){auto old=state.recent_system.front();state.recent_system.pop_front();if(!state.faces[old].file.empty())state.faces[old].loaded.reset();}}
    return selected->loaded;
}
std::filesystem::path font_resource_directory(std::string_view configured){return std::filesystem::path(configured);}
std::shared_ptr<FontProvider> native_font_provider(std::string directory){return std::make_shared<Provider>(std::move(directory));}
}
