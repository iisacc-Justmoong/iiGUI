#include "iiGUI/FontFaceReference.h"
#include <iostream>
struct File{};struct Reference{};enum class Simulation{none};
struct SDK {int calls=0;Reference value;
    int CreateFontFaceReference(File*,std::uint32_t index,Simulation,Reference** out){++calls;*out=&value;return int(index);}
    int CreateFontFaceReference(const wchar_t*,const void*,std::uint32_t,Simulation,Reference**){return -1;}
};
struct IDL {int calls=0;Reference value;
    int CreateFontFaceReference_(File*,std::uint32_t index,Simulation,Reference** out){++calls;*out=&value;return int(index);}
    int CreateFontFaceReference(const wchar_t*,const void*,std::uint32_t,Simulation,Reference**){return -1;}
};
int main(){File file;Reference* ref=nullptr;SDK sdk;IDL idl;
    if(ii::gui::detail::font_face_reference(&sdk,&file,3,Simulation::none,&ref)!=3||ref!=&sdk.value||sdk.calls!=1)return 1;
    if(ii::gui::detail::font_face_reference(&idl,&file,7,Simulation::none,&ref)!=7||ref!=&idl.value||idl.calls!=1)return 2;
    std::cout<<"NATIVE_FONT_SIGNATURES_OK\n";
}
