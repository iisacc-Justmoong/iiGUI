#include <iiGUI/FontMetadata.h>
#include <iiGUI/FontDatabase.h>
#include <filesystem>
#include <iostream>
#include <stdexcept>
using namespace ii;
namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void pinned(const std::filesystem::path& resources){for(const auto& face:gui::FontDatabase::bundled_fonts()){auto value=gui::detail::inspect_font_metadata(resources/std::string(face.file));check(bool(value),"Pinned font metadata unreadable");check(value->family.find(face.family)!=value->family.npos&&value->weight==face.weight&&!value->style.empty(),"Pinned family/weight/style metadata differs");}}
std::filesystem::path fixture(const std::filesystem::path& fixtures,const char* name){auto file=fixtures/name;check(std::filesystem::is_regular_file(file),"Font fixture missing");return file;}
void missing(const std::filesystem::path& fixtures){auto file=fixtures/"missing.ttf";check(!std::filesystem::exists(file),"Missing-font fixture exists");check(!gui::detail::inspect_font_metadata(file),"Missing font accepted");}
void short_file(const std::filesystem::path& fixtures){check(!gui::detail::inspect_font_metadata(fixture(fixtures,"short.ttf")),"Truncated font accepted");}
void unsupported(const std::filesystem::path& fixtures){check(!gui::detail::inspect_font_metadata(fixture(fixtures,"unsupported.ttf")),"Unknown SFNT signature accepted");}
void bounds(const std::filesystem::path& fixtures){auto result=gui::detail::inspect_font_metadata(fixture(fixtures,"out-of-bounds.ttf"));check(!result&&result.error().code==core::ErrorCode::invalid_argument,"Out-of-range name table accepted");}
void collection(const std::filesystem::path& fixtures){check(!gui::detail::inspect_font_metadata(fixture(fixtures,"collection.ttc"),1),"Invalid TTC face accepted");}
void standalone(const std::filesystem::path& resources){check(!gui::detail::inspect_font_metadata(resources/"Pretendard-Regular.ttf",1),"A standalone font exposed a second face");}
}
int main(int argc,char** argv){try{check(argc==3,"Expected case/native-resources");const std::string test=argv[1];const auto fixtures=std::filesystem::path(argv[2]).parent_path()/"font-metadata";if(test=="pinned")pinned(argv[2]);else if(test=="missing")missing(fixtures);else if(test=="short")short_file(fixtures);else if(test=="unsupported")unsupported(fixtures);else if(test=="bounds")bounds(fixtures);else if(test=="collection")collection(fixtures);else if(test=="standalone")standalone(argv[2]);else throw std::runtime_error("Unknown font metadata test");std::cout<<"Font metadata "<<test<<" passed\n";}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
