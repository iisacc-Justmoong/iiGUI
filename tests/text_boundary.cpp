#include <iiGUI/TextBoundary.h>
#include <iostream>
#include <stdexcept>
#include <thread>
using ii::gui::TextIndex;
namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(ii::core::Result<T> r){if(!r)throw std::runtime_error(r.error().message);return std::move(*r);}
void run(const std::string& name){
    if(name=="empty"){TextIndex t;check(t.text().empty()&&t.utf16_size()==0&&take(t.byte_for_utf16(0))==0&&t.next_word(8)==0,"Empty text index changed");}
    else if(name=="ascii"){auto t=take(TextIndex::create("a_b 19\nxyz"));check(t.utf16_size()==10&&take(t.byte_for_utf16(8))==8&&t.previous_word(10)==7&&t.next_word(0)==3,"ASCII word or position mapping changed");}
    else if(name=="utf16"){auto t=take(TextIndex::create("A한😀Z"));check(t.utf16_size()==5&&take(t.byte_for_utf16(2))==4&&take(t.byte_for_utf16(4))==8&&take(t.utf16_for_byte(8))==4,"Native UTF-16 mapping changed");check(!t.byte_for_utf16(3)&&!t.utf16_for_byte(2)&&!t.byte_for_utf16(6),"Scalar-splitting offset accepted");}
    else if(name=="graphemes"){auto t=take(TextIndex::create("e\xcc\x81한👩‍👩‍👧‍👦!"));check(t.next_grapheme(0)==2&&t.previous_grapheme(2)==0&&t.next_grapheme(3)==14&&t.previous_grapheme(14)==3,"Extended grapheme cluster was split");}
    else if(name=="flags"){auto t=take(TextIndex::create("🇰🇷🇺🇸🇨"));check(t.next_grapheme(0)==4&&t.next_grapheme(4)==8&&t.next_grapheme(8)==10,"Regional-indicator state changed");}
    else if(name=="hangul"){auto t=take(TextIndex::create("한글"));check(t.next_grapheme(0)==3&&t.previous_grapheme(3)==0&&t.next_word(0)==4,"Hangul composition or word movement changed");}
    else if(name=="crlf"){auto t=take(TextIndex::create("a\r\nb\n"));check(t.next_grapheme(1)==3&&t.previous_grapheme(3)==1&&t.next_grapheme(4)==5,"CRLF cluster was divided");}
    else if(name=="words"){auto t=take(TextIndex::create("한글 e\xcc\x81 😀 tail_2"));check(t.next_word(0)==2&&t.next_word(2)==5&&t.previous_word(t.utf16_size())==9&&t.next_word(4)==5,"Unicode word movement split a cluster or skipped a word");}
    else if(name=="nul"){auto t=take(TextIndex::create(std::string("a\0b",3)));check(t.utf16_size()==3&&t.next_word(1)==3,"Text NUL became a path restriction");}
    else if(name=="invalid"){check(!TextIndex::create(std::string("\xf0\x80\x80\x80",4))&&!TextIndex::create(std::string("\xed\xa0\x80",3)),"Invalid UTF-8 accepted");}
    else if(name=="sparse"){auto t=take(TextIndex::create(std::string(1024*1024,'a')));check(t.storage_bytes()<2*1024*1024&&take(t.byte_for_utf16(900000))==900000,"ASCII index allocated a per-scalar table");}
    else if(name=="sharing"){auto t=take(TextIndex::create("한글 😀"));auto copy=t;bool okay=false;std::thread worker([copy,&okay]{okay=copy.utf16_size()==5&&copy.next_grapheme(3)==5;});worker.join();check(okay&&copy.text().data()==t.text().data(),"Immutable index lost sharing or thread independence");}
    else throw std::runtime_error("Unknown text boundary case");
}
}
int main(int argc,char** argv){try{run(argc>1?argv[1]:"empty");std::cout<<"TEXT_BOUNDARY_OK\n";}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
