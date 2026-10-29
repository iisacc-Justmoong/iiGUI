#include "DesktopCommon.h"
#include "WebAsync.h"
#include <emscripten.h>
#include <array>
namespace ii::gui {
namespace {
IIGUI_WEB_ASYNC_JS(void,await_clipboard,(int* state),{while(Atomics.load(HEAP32,state>>2)===0){const wait=Atomics.waitAsync(HEAP32,state>>2,0);if(wait.async)await wait.value;}});
}
core::Result<ClipboardContent> read_clipboard(){std::vector<char> bytes(1024*1024+1);alignas(4) std::array<int,2> state{};
    MAIN_THREAD_EM_ASM({const pointer=$0;const output=$1;const limit=$2;const done=(code)=>{Atomics.store(HEAP32,pointer>>2,code);Atomics.notify(HEAP32,pointer>>2);};if(!navigator.clipboard){done(-1);return;}navigator.clipboard.readText().then(text=>{const count=lengthBytesUTF8(text);if(count>=limit){done(-2);return;}stringToUTF8(text,output,limit);HEAP32[(pointer>>2)+1]=count;done(1);},()=>done(-1));},state.data(),bytes.data(),bytes.size());
    await_clipboard(state.data());if(state[0]!=1)return core::fail(core::ErrorCode::failed,"Browser denied clipboard access, context is insecure, or text exceeds 1 MiB");ClipboardContent result;result.text.assign(bytes.data(),state[1]);if(!detail::valid_clipboard(result))return core::fail(core::ErrorCode::invalid_argument,"Invalid clipboard text");return result;
}
core::Result<void> write_clipboard(const ClipboardContent& value){if(!detail::valid_clipboard(value))return core::fail(core::ErrorCode::invalid_argument,"Invalid clipboard payload");if(!value.image.empty()||!value.files.empty())return core::fail(core::ErrorCode::unsupported,"Browser clipboard currently supports text and URLs");alignas(4) std::array<int,2> state{};const auto& text=value.text.empty()?value.url:value.text;
    MAIN_THREAD_EM_ASM({const pointer=$0;const text=UTF8ToString($1);const done=(code)=>{Atomics.store(HEAP32,pointer>>2,code);Atomics.notify(HEAP32,pointer>>2);};if(!navigator.clipboard){done(-1);return;}navigator.clipboard.writeText(text).then(()=>done(1),()=>done(-1));},state.data(),text.c_str());await_clipboard(state.data());if(state[0]!=1)return core::fail(core::ErrorCode::failed,"Browser denied clipboard write; a secure context and user activation may be required");return {};
}
core::Result<void> open_external_url(std::string_view value){if(!detail::valid_external_url(value))return core::fail(core::ErrorCode::invalid_argument,"Invalid external URL");std::string url(value);const auto opened=MAIN_THREAD_EM_ASM_INT({const target=window.open(UTF8ToString($0),'_blank');if(!target)return 0;target.opener=null;return 1;},url.c_str());if(!opened)return core::fail(core::ErrorCode::failed,"Browser blocked the external URL; call from a user action");return {};}
}
