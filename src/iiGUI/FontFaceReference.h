#pragma once
#include <cstdint>
namespace ii::gui::detail {
// Windows SDK overloads the file/path variants. Some MinGW IDL headers name
// the file variant with a trailing underscore while retaining its COM slot.
// Select the declared signature without reopening the analyzed font by path.
template<class Factory,class File,class Simulation,class Reference>
auto font_face_reference(Factory* factory,File* file,std::uint32_t index,Simulation simulation,Reference** reference){
    if constexpr(requires{factory->CreateFontFaceReference(file,index,simulation,reference);})
        return factory->CreateFontFaceReference(file,index,simulation,reference);
    else return factory->CreateFontFaceReference_(file,index,simulation,reference);
}
}
