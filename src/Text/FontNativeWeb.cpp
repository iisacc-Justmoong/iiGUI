#include <iiGUI/FontNative.h>
#include <iiGUI/WebAsync.h>
#include <emscripten.h>
#include <fstream>
#include <set>
namespace ii::gui::detail {
namespace {
IIGUI_WEB_ASYNC_JS(int,register_font,(const char* family,int weight,const unsigned char* bytes,int length),{
    try{const face=new FontFace(UTF8ToString(family),HEAPU8.slice(bytes,bytes+length),{weight:String(weight)});await face.load();self.fonts.add(face);return 1;}catch(error){console.error('iiGUI font:',error);return 0;}
});
class Provider final:public FontProvider {
    std::filesystem::path directory_;std::set<std::string> families_{"sans-serif","serif","monospace"};
public:
    explicit Provider(std::filesystem::path directory):directory_(std::move(directory)){}
    std::vector<std::string> families()const override{return {families_.begin(),families_.end()};}
    std::string system_family()const override{return "sans-serif";}
    std::string resolve(const FontFace& face)const override{return families_.contains(face.family)?face.family:system_family();}
    FontLoadReport load(std::span<const FontResource> resources)override{FontLoadReport result;for(const auto& resource:resources){FontLoad entry{std::string(resource.file)};try{std::ifstream file(directory_/resource.file,std::ios::binary|std::ios::ate);if(!file)throw std::runtime_error("Bundled browser font is absent from preloaded resources");const auto count=file.tellg();if(count<12||count>16*1024*1024)throw std::runtime_error("Invalid browser font size");std::vector<unsigned char> bytes(std::size_t(count),0);file.seekg(0);if(!file.read(reinterpret_cast<char*>(bytes.data()),count))throw std::runtime_error("Cannot read preloaded browser font");std::string family(resource.family);if(!register_font(family.c_str(),resource.weight,bytes.data(),int(bytes.size())))throw std::runtime_error("Browser FontFace rejected bundled font");families_.insert(family);entry.loaded=true;}catch(const std::exception& e){entry.error=e.what();}result.resources.push_back(std::move(entry));}return result;}
};
}
std::filesystem::path font_resource_directory(std::string_view configured){return configured.empty()?std::filesystem::path("/resources"):std::filesystem::path(configured);}
std::shared_ptr<FontProvider> native_font_provider(std::string directory){return std::make_shared<Provider>(font_resource_directory(directory));}
}
