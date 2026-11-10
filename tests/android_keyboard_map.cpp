#include "../src/Platform/AndroidKeyboardMap.h"
#include <cstdarg>
#include <limits>
#include <stdexcept>
using namespace ii::gui::detail;
FakeMapState& fake_map_state(){static FakeMapState state;return state;}
JNIEnv& fake_map_env(){static JNIEnv env;return env;}
jobject JNIEnv::CallStaticObjectMethod(jclass,jmethodID method,...){
    if(method->name!="load")throw std::runtime_error("Unexpected static object call");
    auto& state=fake_map_state();++state.loads;
    if(state.fail_load){state.pending=true;return nullptr;}return &state.map;
}
jint JNIEnv::CallIntMethod(jobject,jmethodID method,...){
    if(method->name!="get")throw std::runtime_error("Unexpected map call");
    ++fake_map_state().lookups;va_list args;va_start(args,method);const auto key=va_arg(args,int),meta=va_arg(args,int);va_end(args);
    return key==99?std::numeric_limits<jint>::min()+39:key+meta;
}
jint JNIEnv::CallStaticIntMethod(jclass,jmethodID method,...){
    if(method->name!="getDeadChar")throw std::runtime_error("Unexpected static integer call");
    ++fake_map_state().combinations;va_list args;va_start(args,method);const auto accent=va_arg(args,int),value=va_arg(args,int);va_end(args);
    return accent+value;
}
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int main(){
    auto& state=fake_map_state();AndroidKeyboardMap map;
    check(map.character(5,10,1)==11&&map.character(5,20,2)==22&&state.loads==1&&state.lookups==2,
          "Device map was not reused for one event");
    check(map.character(5,99,0)==0x80000027u,"JNI lost the high-bit dead-key marker");
    check(map.character(6,3,4)==7&&state.loads==2,"Device change reused the wrong key map");
    check(map.combine(2,3)==5&&state.combinations==1,"Dead-key combination skipped the system service");
    state.fail_load=true;check(map.character(7,3,4)==0,"JNI failure produced a guessed character");
    state.fail_load=false;check(map.character(7,3,4)==7,"Key map did not recover after a JNI exception");
}
