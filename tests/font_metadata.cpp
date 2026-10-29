#include <iiGUI/FontMetadata.h>
#include <iiGUI/FontDatabase.h>
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace ii;
namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
struct Fixture final {
    std::filesystem::path root,file;
    explicit Fixture(const std::filesystem::path& build){root=build/("font-metadata-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));if(!std::filesystem::create_directory(root))throw std::runtime_error("Cannot create exclusive metadata fixture");file=root/"font.ttf";}
    ~Fixture(){std::error_code ignored;std::filesystem::remove_all(root,ignored);}
    void write(std::vector<unsigned char> bytes){std::ofstream out(file,std::ios::binary);out.write(reinterpret_cast<const char*>(bytes.data()),std::streamsize(bytes.size()));}
};
void pinned(const std::filesystem::path& resources){for(const auto& face:gui::FontDatabase::bundled_fonts()){auto value=gui::detail::inspect_font_metadata(resources/std::string(face.file));check(bool(value),"Pinned font metadata unreadable");check(value->family.find(face.family)!=value->family.npos&&value->weight==face.weight&&!value->style.empty(),"Pinned family/weight/style metadata differs");}}
void missing(const std::filesystem::path& build){Fixture f(build);check(!gui::detail::inspect_font_metadata(f.file),"Missing font accepted");}
void short_file(const std::filesystem::path& build){Fixture f(build);f.write({1,2,3});check(!gui::detail::inspect_font_metadata(f.file),"Truncated font accepted");}
void unsupported(const std::filesystem::path& build){Fixture f(build);f.write(std::vector<unsigned char>(32,'x'));check(!gui::detail::inspect_font_metadata(f.file),"Unknown SFNT signature accepted");}
void bounds(const std::filesystem::path& build){Fixture f(build);std::vector<unsigned char> bytes(28,0);bytes[1]=1;bytes[5]=1;bytes[12]='n';bytes[13]='a';bytes[14]='m';bytes[15]='e';bytes[20]=255;bytes[21]=255;bytes[22]=255;bytes[23]=240;bytes[27]=16;f.write(bytes);auto result=gui::detail::inspect_font_metadata(f.file);check(!result&&result.error().code==core::ErrorCode::invalid_argument,"Out-of-range name table accepted");}
void collection(const std::filesystem::path& build){Fixture f(build);std::vector<unsigned char> bytes(16,0);bytes[0]='t';bytes[1]='t';bytes[2]='c';bytes[3]='f';bytes[11]=1;f.write(bytes);check(!gui::detail::inspect_font_metadata(f.file,1),"Invalid TTC face accepted");}
void standalone(const std::filesystem::path& resources){check(!gui::detail::inspect_font_metadata(resources/"Pretendard-Regular.ttf",1),"A standalone font exposed a second face");}
}
int main(int argc,char** argv){try{check(argc==4,"Expected case/resources/build");const std::string test=argv[1];if(test=="pinned")pinned(argv[2]);else if(test=="missing")missing(argv[3]);else if(test=="short")short_file(argv[3]);else if(test=="unsupported")unsupported(argv[3]);else if(test=="bounds")bounds(argv[3]);else if(test=="collection")collection(argv[3]);else if(test=="standalone")standalone(argv[2]);else throw std::runtime_error("Unknown font metadata test");std::cout<<"Font metadata "<<test<<" passed\n";}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
