#include "../src/Text/AndroidGlobalRef.h"
#include <stdexcept>
#include <utility>
using namespace ii::gui::detail;
namespace {JNIEnv env;JavaVM vm;int made=0,deleted=0,attached=0,detached=0;bool disconnected=false,failed=false,has_exception=false;}
jint JNIEnv::GetJavaVM(JavaVM** out){*out=&vm;return JNI_OK;}
jobject JNIEnv::NewGlobalRef(jobject value){if(failed)return nullptr;++made;return value;}
void JNIEnv::DeleteGlobalRef(jobject){++deleted;}
jint JavaVM::GetEnv(void** result,jint){if(disconnected)return JNI_EDETACHED;*result=&env;return JNI_OK;}
jint JavaVM::AttachCurrentThread(JNIEnv** result,void*){++attached;*result=&env;return JNI_OK;}
jint JavaVM::DetachCurrentThread(){++detached;return JNI_OK;}
void AndroidJni::check(){if(has_exception){has_exception=false;throw std::runtime_error("simulated JNI failure");}}
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int main(){
    AndroidJni jni{&env};int value=1;
    {AndroidGlobalRef one(jni,&value);AndroidGlobalRef two(std::move(one));check(two.get()==&value&&!one.get(),"Global reference move changed ownership");}
    check(made==1&&deleted==1&&attached==0,"Global reference was not deleted once");
    {AndroidGlobalRef one(jni,&value);disconnected=true;}
    check(made==2&&deleted==2&&attached==1&&detached==1,"Detached destruction did not balance its VM attachment");
    disconnected=false;failed=true;bool rejected=false;try{AndroidGlobalRef invalid(jni,&value);}catch(const std::runtime_error&){rejected=true;}
    check(rejected&&deleted==2,"A failed global reference was accepted or deleted");
    failed=false;has_exception=true;rejected=false;try{AndroidGlobalRef invalid(jni,&value);}catch(const std::runtime_error&){rejected=true;}
    check(rejected&&made==3&&deleted==3,"A JNI exception leaked a newly created global reference");
}
