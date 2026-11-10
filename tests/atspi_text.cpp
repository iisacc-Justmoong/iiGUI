#include "../src/Widgets/AtSpiText.h"
#include "../src/Widgets/AtSpiTextSource.h"
#include <stdexcept>
#include <string>

using namespace ii::gui;
using namespace ii::gui::detail;
namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void unicode_offsets(){
    auto text=AtSpiTextIndex::create("A😀e\u0301한\n",{1,2},false);
    check(text&&text->characters()==6&&text->utf16_size()==7,"Unicode scalar count was confused with UTF-16 length");
    check(text->utf16_at(0)==0&&text->utf16_at(2)==3&&text->utf16_at(6)==7,"Scalar positions were not converted to UTF-16");
    check(text->scalar_at_utf16(3)==2&&!text->scalar_at_utf16(2),"A surrogate pair was split");
    check(text->selection()==TextRange{1,1}&&text->caret()==2,"Selection or caret used UTF-16 offsets");
    check(text->slice(1,4)=="😀e\u0301"&&text->character(1)==0x1f600&&text->character(3)==0x301,"Text slices or scalar values were corrupted");
    check(!text->utf16_at(7)&&!text->slice(4,3)&&!text->character(6),"An invalid character range was accepted");
    auto reverse=AtSpiTextIndex::create("A😀B",{1,2},true);check(reverse&&reverse->caret()==1,"Reverse selection lost its caret");
}
void malformed_and_sparse(){
    check(!AtSpiTextIndex::create(std::string("x\0y",3),{},false),"D-Bus text accepted an embedded NUL");
    check(!AtSpiTextIndex::create(std::string("\xf0\x28\x8c\x28",4),{},false),"Invalid UTF-8 was accepted");
    check(!AtSpiTextIndex::create("😀",{1,0},false),"A selection inside a surrogate pair was accepted");
    std::string content(700,'a');content.insert(257,"😀");auto text=AtSpiTextIndex::create(content,{259,0},false);
    check(text&&text->characters()==701&&text->utf16_at(258)==259&&text->scalar_at_utf16(259)==258,"Sparse index failed across a checkpoint");
    check(text->slice(256,259)=="a😀a","Sparse slicing crossed the wrong bytes");
}
void provider_reads_and_revision(){
    auto state=std::make_shared<WidgetState>([](ii::runtime::Task task){task();});
    auto editor=state->action_surface({},"editor",{.accessibility_name="Document"},{});check(bool(editor),"Cannot create editor");
    std::string content(65535,'a');content+="😀tail";auto index=TextIndex::create(content);check(bool(index),"Cannot index provider text");
    std::uint64_t revision=1;int reads=0;bool mutate=false,password=false,malformed=false;
    auto provider=std::make_shared<TextInputProvider>();
    provider->state=[&]()->ii::core::Result<TextInputState>{return TextInputState{.length=index->utf16_size(),.selection={65535,2},.password=password,.revision=revision};};
    provider->read=[&](TextRange range)->ii::core::Result<TextContent>{
        if(malformed&&range.location==0)return TextContent{{0,1},"aa"};
        if(malformed&&range.location==1)return TextContent{{1,1},""};
        ++reads;auto first=index->byte_for_utf16(range.location),last=index->byte_for_utf16(range.location+range.length);
        if(!first||!last)return ii::core::fail(ii::core::ErrorCode::out_of_range,"Split scalar");
        if(mutate)++revision;return TextContent{range,std::string(index->text().substr(*first,*last-*first))};
    };
    check(bool(editor->set_text_input_provider(provider)),"Cannot install editor provider");auto snapshot=editor->snapshot();check(bool(snapshot),"Cannot snapshot editor");
    auto target=atspi_text_target(*snapshot);check(bool(target),"An editor with a revision was not exposed");
    auto text=read_atspi_text(*state,*target);check(text&&text->characters()==65540&&text->slice(65535,65536)=="😀"&&reads>=3,"Chunked provider text was truncated or split");
    malformed=true;check(!read_atspi_text(*state,*target),"A provider fragment with the wrong UTF-16 length was accepted");malformed=false;
    mutate=true;check(!read_atspi_text(*state,*target),"A changing provider published stale text");mutate=false;
    password=true;auto protected_snapshot=editor->snapshot();check(protected_snapshot&&!atspi_text_target(*protected_snapshot),"A password provider was exposed");
    const auto before=reads;check(!read_atspi_text(*state,*target)&&reads==before,"A password provider was read");state->close();
}
}
int main(){unicode_offsets();malformed_and_sparse();provider_reads_and_revision();}
