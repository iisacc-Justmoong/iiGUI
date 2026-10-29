#include "FontNative.h"
#include "FontFaceReference.h"
#include <windows.h>
#include <dwrite_3.h>
#include <algorithm>
#include <map>
#include <mutex>

namespace ii::gui::detail {
namespace {
using Microsoft::WRL::ComPtr;
void checked(HRESULT value,const char* operation){if(FAILED(value))throw std::runtime_error(std::string(operation)+" (HRESULT "+std::to_string(value)+")");}
std::wstring wide(std::string_view value){if(value.empty())return {};const auto n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,value.data(),int(value.size()),nullptr,0);if(!n)throw std::invalid_argument("Invalid UTF-8 font name");std::wstring result(std::size_t(n),L'\0');MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,value.data(),int(value.size()),result.data(),n);return result;}
std::string narrow(std::wstring_view value){if(value.empty())return {};const auto n=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,value.data(),int(value.size()),nullptr,0,nullptr,nullptr);if(!n)throw std::invalid_argument("Invalid UTF-16 font name");std::string result(std::size_t(n),'\0');WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,value.data(),int(value.size()),result.data(),n,nullptr,nullptr);return result;}
struct Registry final {
    std::mutex mutex;
    ComPtr<IDWriteFactory3> factory;
    ComPtr<IDWriteFontCollection> collection;
    std::map<std::filesystem::path,ComPtr<IDWriteFontFile>> files;
    Registry(){checked(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,__uuidof(IDWriteFactory3),reinterpret_cast<IUnknown**>(factory.GetAddressOf())),"Cannot create native font database");rebuild(files);}
    void rebuild(const std::map<std::filesystem::path,ComPtr<IDWriteFontFile>>& candidate){
        ComPtr<IDWriteFontSetBuilder> builder;checked(factory->CreateFontSetBuilder(&builder),"Cannot create font set");
        ComPtr<IDWriteFontSet> system;checked(factory->GetSystemFontSet(&system),"Cannot read system font set");checked(builder->AddFontSet(system.Get()),"Cannot add system fonts");
        for(const auto& [path,file]:candidate){(void)path;BOOL supported=FALSE;DWRITE_FONT_FILE_TYPE type{};DWRITE_FONT_FACE_TYPE face{};UINT32 count=0;checked(file->Analyze(&supported,&type,&face,&count),"Cannot inspect native font file");if(!supported)throw std::runtime_error("Unsupported native font file");
            for(UINT32 i=0;i<count;++i){ComPtr<IDWriteFontFaceReference> reference;checked(font_face_reference(factory.Get(),file.Get(),i,DWRITE_FONT_SIMULATIONS_NONE,reference.GetAddressOf()),"Cannot reference native font face");checked(builder->AddFontFaceReference(reference.Get()),"Cannot add native font face");}}
        ComPtr<IDWriteFontSet> set;checked(builder->CreateFontSet(&set),"Cannot finalize native font set");ComPtr<IDWriteFontCollection1> replacement;checked(factory->CreateFontCollectionFromFontSet(set.Get(),&replacement),"Cannot finalize native font collection");checked(replacement.As(&collection),"Cannot publish native font collection");
    }
};
Registry& registry(){static Registry value;return value;}
std::string family_name(IDWriteFontFamily* family){
    ComPtr<IDWriteLocalizedStrings> names;checked(family->GetFamilyNames(&names),"Cannot read native family name");UINT32 index=0;BOOL exists=FALSE;
    wchar_t locale[LOCALE_NAME_MAX_LENGTH]{};if(GetUserDefaultLocaleName(locale,LOCALE_NAME_MAX_LENGTH))checked(names->FindLocaleName(locale,&index,&exists),"Cannot select native font locale");
    if(!exists)checked(names->FindLocaleName(L"en-us",&index,&exists),"Cannot select native font locale");if(!exists)index=0;
    UINT32 length=0;checked(names->GetStringLength(index,&length),"Cannot size native font name");std::wstring value(std::size_t(length)+1,L'\0');checked(names->GetString(index,value.data(),length+1),"Cannot read native font name");value.resize(length);return narrow(value);
}
class Provider final:public FontProvider {
    std::string directory_;
public:
    explicit Provider(std::string directory):directory_(std::move(directory)){}
    std::vector<std::string> families()const override{auto collection=native_font_collection();std::vector<std::string> result;for(UINT32 i=0;i<collection->GetFontFamilyCount();++i){ComPtr<IDWriteFontFamily> family;checked(collection->GetFontFamily(i,&family),"Cannot enumerate native families");result.push_back(family_name(family.Get()));}std::ranges::sort(result);return result;}
    std::string system_family()const override{NONCLIENTMETRICSW metrics{};metrics.cbSize=sizeof(metrics);if(!SystemParametersInfoW(SPI_GETNONCLIENTMETRICS,sizeof(metrics),&metrics,0))throw std::runtime_error("Cannot query native system font");return narrow(metrics.lfMessageFont.lfFaceName);}
    std::string resolve(const FontFace& face)const override{auto collection=native_font_collection();const auto name=wide(face.family);UINT32 index=0;BOOL exists=FALSE;checked(collection->FindFamilyName(name.c_str(),&index,&exists),"Cannot resolve native family");if(!exists)return system_family();ComPtr<IDWriteFontFamily> family;checked(collection->GetFontFamily(index,&family),"Cannot resolve native family");return family_name(family.Get());}
    FontLoadReport load(std::span<const FontResource> resources)override{
        auto& state=registry();std::lock_guard guard(state.mutex);auto candidate=state.files;FontLoadReport report;const auto directory=font_resource_directory(directory_);
        for(const auto& resource:resources){const auto path=directory/std::string(resource.file);FontLoad result{std::string(resource.file)};
            try{if(!candidate.contains(path)){ComPtr<IDWriteFontFile> file;checked(state.factory->CreateFontFileReference(path.c_str(),nullptr,&file),"Cannot open native bundled font");BOOL supported=FALSE;DWRITE_FONT_FILE_TYPE type{};DWRITE_FONT_FACE_TYPE face{};UINT32 count=0;checked(file->Analyze(&supported,&type,&face,&count),"Cannot inspect bundled font");if(!supported||!count)throw std::runtime_error("Invalid bundled font");candidate.emplace(path,std::move(file));}result.loaded=true;}catch(const std::exception& error){result.error=error.what();}report.resources.push_back(std::move(result));}
        if(candidate.size()!=state.files.size()){state.rebuild(candidate);state.files=std::move(candidate);}return report;
    }
};
}
std::filesystem::path font_resource_directory(std::string_view configured){if(!configured.empty())return std::filesystem::path(wide(configured));std::wstring path(32768,L'\0');const auto count=GetModuleFileNameW(nullptr,path.data(),DWORD(path.size()));if(!count||count>=path.size())throw std::runtime_error("Cannot locate native font resources");path.resize(count);return std::filesystem::path(path).parent_path()/L"resources";}
Microsoft::WRL::ComPtr<IDWriteFontCollection> native_font_collection(){auto& state=registry();std::lock_guard guard(state.mutex);return state.collection;}
std::shared_ptr<FontProvider> native_font_provider(std::string directory){return std::make_shared<Provider>(std::move(directory));}
}
