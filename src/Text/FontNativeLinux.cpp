#include <iiGUI/FontNative.h>
#include <fontconfig/fontconfig.h>
#include <pango/pangocairo.h>
#include <pango/pangofc-fontmap.h>
#include <algorithm>
#include <mutex>
#include <set>
#include <unistd.h>

namespace ii::gui::detail {
namespace {
std::mutex mutex;
std::set<std::filesystem::path> loaded;
struct Pattern final{FcPattern* value=nullptr;~Pattern(){if(value)FcPatternDestroy(value);}};
std::string matched(std::string_view requested){
    Pattern pattern{FcPatternCreate()};if(!pattern.value)throw std::bad_alloc();std::string family(requested);
    FcPatternAddString(pattern.value,FC_FAMILY,reinterpret_cast<const FcChar8*>(family.c_str()));FcConfigSubstitute(nullptr,pattern.value,FcMatchPattern);FcDefaultSubstitute(pattern.value);FcResult result{};
    Pattern match{FcFontMatch(nullptr,pattern.value,&result)};FcChar8* value=nullptr;
    if(!match.value||FcPatternGetString(match.value,FC_FAMILY,0,&value)!=FcResultMatch||!value)throw std::runtime_error("Cannot resolve a native font family");return reinterpret_cast<const char*>(value);
}
class Provider final:public FontProvider {
    std::string directory_;
public:
    explicit Provider(std::string directory):directory_(std::move(directory)){if(!FcInit())throw std::runtime_error("Cannot initialize native font configuration");}
    std::vector<std::string> families()const override{
        std::lock_guard guard(mutex);Pattern pattern{FcPatternCreate()};auto objects=FcObjectSetBuild(FC_FAMILY,nullptr);if(!pattern.value||!objects)throw std::bad_alloc();
        auto fonts=FcFontList(nullptr,pattern.value,objects);FcObjectSetDestroy(objects);if(!fonts)throw std::runtime_error("Cannot enumerate native font families");
        std::set<std::string> names;for(int i=0;i<fonts->nfont;++i){FcChar8* family=nullptr;for(int n=0;FcPatternGetString(fonts->fonts[i],FC_FAMILY,n,&family)==FcResultMatch;++n)if(family)names.emplace(reinterpret_cast<const char*>(family));}
        FcFontSetDestroy(fonts);return {names.begin(),names.end()};
    }
    std::string system_family()const override{std::lock_guard guard(mutex);return matched("sans-serif");}
    std::string resolve(const FontFace& value)const override{std::lock_guard guard(mutex);return matched(value.family);}
    FontLoadReport load(std::span<const FontResource> resources)override{
        std::lock_guard guard(mutex);FontLoadReport result;const auto directory=font_resource_directory(directory_);bool changed=false;
        for(const auto& resource:resources){const auto path=directory/std::string(resource.file);bool success=loaded.contains(path);
            if(!success){success=FcConfigAppFontAddFile(FcConfigGetCurrent(),reinterpret_cast<const FcChar8*>(path.c_str()));if(success){loaded.insert(path);changed=true;}}
            result.resources.push_back({std::string(resource.file),success,success?"":"Cannot register native bundled font"});}
        if(changed){FcConfigBuildFonts(FcConfigGetCurrent());auto map=pango_cairo_font_map_get_default();if(PANGO_IS_FC_FONT_MAP(map))pango_fc_font_map_config_changed(PANGO_FC_FONT_MAP(map));}return result;
    }
};
}
std::filesystem::path font_resource_directory(std::string_view configured){if(!configured.empty())return std::filesystem::path(configured);std::string path(65536,'\0');const auto count=readlink("/proc/self/exe",path.data(),path.size());if(count<=0||std::size_t(count)==path.size())throw std::runtime_error("Cannot locate native font resources");path.resize(std::size_t(count));return std::filesystem::path(path).parent_path()/"resources";}
std::shared_ptr<FontProvider> native_font_provider(std::string directory){return std::make_shared<Provider>(std::move(directory));}
}
