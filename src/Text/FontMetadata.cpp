#include <iiGUI/FontMetadata.h>
#include <iiSDKCore/Core/Text/Text.h>
#include <fstream>
#include <algorithm>
#include <limits>
#include <vector>

namespace ii::gui::detail {
namespace {
using Data=std::vector<unsigned char>;
unsigned u16(const Data& b,std::size_t p){return (unsigned(b.at(p))<<8)|b.at(p+1);}
std::uint32_t u32(const Data& b,std::size_t p){return (std::uint32_t(b.at(p))<<24)|(std::uint32_t(b.at(p+1))<<16)|(std::uint32_t(b.at(p+2))<<8)|b.at(p+3);}
bool tag(const Data& b,std::size_t p,const char* value){return b.size()>=p+4&&std::equal(b.begin()+p,b.begin()+p+4,value);}
struct Reader final {
    std::ifstream file;std::uint64_t size=0;
    explicit Reader(const std::filesystem::path& path):file(path,std::ios::binary|std::ios::ate){if(!file)throw std::runtime_error("Cannot open font metadata source");const auto n=file.tellg();if(n<12||n>512*1024*1024)throw std::invalid_argument("Font metadata source size is invalid");size=std::uint64_t(n);}
    Data read(std::uint64_t offset,std::size_t count){if(count>2*1024*1024||offset>size||count>size-offset)throw std::invalid_argument("Font metadata range exceeds its source");Data result(count);file.seekg(std::streamoff(offset));if(count&&!file.read(reinterpret_cast<char*>(result.data()),std::streamsize(count)))throw std::runtime_error("Cannot read font metadata source");return result;}
};
std::string decode(const Data& bytes,std::size_t offset,std::size_t count,unsigned platform,unsigned encoding){
    if(offset>bytes.size()||count>bytes.size()-offset||count>16384)return {};
    std::string result;
    if(platform==0||(platform==3&&(encoding==0||encoding==1||encoding==10))){
        if(count%2)return {};std::u16string value;value.reserve(count/2);for(std::size_t i=0;i<count;i+=2)value.push_back(char16_t(u16(bytes,offset+i)));
        auto converted=core::to_utf8(value);if(!converted)return {};result=std::move(*converted);
    }else if(platform==1&&encoding==0){for(std::size_t i=0;i<count;++i){auto c=bytes[offset+i];if(c>=128)return {};result.push_back(char(c));}}
    if(result.empty()||result.size()>4096||result.find('\0')!=result.npos)return {};return result;
}
}
core::Result<FontMetadata> inspect_font_metadata(const std::filesystem::path& path,unsigned face){
    try{
        Reader source(path);auto header=source.read(0,12);std::uint64_t base=0;
        if(tag(header,0,"ttcf")){const auto count=u32(header,8);if(!count||count>4096||face>=count)throw std::invalid_argument("Font collection index is invalid");base=u32(source.read(12+std::uint64_t(face)*4,4),0);header=source.read(base,12);}
        else if(face)throw std::invalid_argument("A standalone font has only one face");
        if(!tag(header,0,"OTTO")&&!tag(header,0,"true")&&u32(header,0)!=0x00010000)throw std::invalid_argument("Unsupported SFNT metadata format");
        const auto count=u16(header,4);if(!count||count>4096)throw std::invalid_argument("SFNT table count is invalid");auto tables=source.read(base+12,std::size_t(count)*16);
        Data names;FontMetadata result;for(std::size_t i=0;i<count;++i){const auto p=i*16;const auto offset=u32(tables,p+8),length=u32(tables,p+12);
            if(tag(tables,p,"name"))names=source.read(offset,length);
            if(tag(tables,p,"OS/2")&&length>=6){const auto weight=u16(source.read(offset,6),4);if(weight>=1&&weight<=1000)result.weight=int(weight);}}
        if(names.size()<6||u16(names,0)>1)throw std::invalid_argument("SFNT name metadata is missing");const auto records=u16(names,2),storage=u16(names,4);
        if(records>4096||6+std::size_t(records)*12>names.size()||storage>names.size()||storage<6+std::size_t(records)*12)throw std::invalid_argument("SFNT name records are invalid");
        int family_score=-1,style_score=-1;
        for(std::size_t i=0;i<records;++i){const auto p=6+i*12;const auto platform=u16(names,p),encoding=u16(names,p+2),language=u16(names,p+4),id=u16(names,p+6);
            if(id!=1&&id!=2&&id!=16&&id!=17)continue;auto text=decode(names,std::size_t(storage)+u16(names,p+10),u16(names,p+8),platform,encoding);if(text.empty())continue;
            const int score=(id>=16?100:0)+(language==0x409?30:language==0?20:0)+(platform==3?5:platform==0?4:0);
            if((id==1||id==16)&&score>family_score){result.family=std::move(text);family_score=score;}
            if((id==2||id==17)&&score>style_score){result.style=std::move(text);style_score=score;}}
        if(result.family.empty())throw std::invalid_argument("SFNT has no supported family name");if(result.style.empty())result.style="Regular";return result;
    }catch(const std::invalid_argument& error){return core::fail(core::ErrorCode::invalid_argument,error.what(),std::current_exception());}
    catch(...){return core::fail(core::ErrorCode::failed,"Font metadata inspection failed",std::current_exception());}
}
}
