#pragma once
#include <cstdint>
#include <stdexcept>
#include <string_view>
using jint=std::int32_t;
struct FakeMapObject{};
struct FakeMapMethod{std::string_view name;};
using jobject=FakeMapObject*;using jclass=FakeMapObject*;using jmethodID=FakeMapMethod*;
struct FakeMapState{
    int loads=0,lookups=0,combinations=0;
    bool fail_load=false,pending=false;
    FakeMapObject type,map;
    FakeMapMethod load{"load"},get{"get"},dead{"getDeadChar"};
};
FakeMapState& fake_map_state();
struct JNIEnv {
    jobject CallStaticObjectMethod(jclass,jmethodID,...);
    jint CallIntMethod(jobject,jmethodID,...);
    jint CallStaticIntMethod(jclass,jmethodID,...);
};
JNIEnv& fake_map_env();
namespace ii::gui::detail {
class AndroidJni final{
public:
    JNIEnv* env=&fake_map_env();
    jclass type(const char*){return &fake_map_state().type;}
    jmethodID method(jclass,const char* name,const char*){
        if(std::string_view(name)!="get")throw std::runtime_error("Unexpected instance method");
        return &fake_map_state().get;
    }
    jmethodID static_method(jclass,const char* name,const char*){
        return std::string_view(name)=="load"?&fake_map_state().load:&fake_map_state().dead;
    }
    void check(){if(fake_map_state().pending){fake_map_state().pending=false;throw std::runtime_error("Simulated JNI exception");}}
};
}
