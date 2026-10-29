#pragma once
#include <iiSDKCore/Core/Property/Observable.h>
#include <string>
#include <array>
#include <functional>
struct LayoutRecord final {std::string title;int count;void act(){++count;}std::string describe()const{return title+std::to_string(count);}};
struct LayoutParent final {LayoutRecord child{"Temporary",0};std::function<void()> handler=[]{ };auto& make(){return handler;}LayoutParent()=default;LayoutParent(const LayoutParent&)=delete;LayoutParent(LayoutParent&&)=delete;};
struct LayoutArgument final {LayoutArgument()=default;LayoutArgument(const LayoutArgument&)=delete;LayoutArgument(LayoutArgument&&)=delete;};
struct LayoutModel final {
    ii::core::Observable<std::function<void()>> run{[]{}};
    ii::core::Observable<std::function<std::string()>> format{[]{return std::string("Field");}};
    LayoutRecord record{"Record",3};
    std::array<LayoutRecord,1> records{{{"Indexed",1}}};
    LayoutParent makeParent(){return {};}
    LayoutArgument argument(){return {};}
    auto capture(const LayoutArgument&){return []{};}
    std::string summary()const{return record.title+std::to_string(width.get());}
    ii::core::Observable<double> width{200},gap{8},padding{4};
    ii::core::Observable<std::string> alignment{"leading"};
    ii::core::Observable<std::string> title{"Review"},font{"Pretendard"};
};
