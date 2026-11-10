#include <iiGUI/DesktopCommon.h>
#include <iiGUI/WebAsync.h>
#include <emscripten.h>
#include <array>
#include <cstdlib>
#include <memory>
namespace ii::gui {
namespace {
IIGUI_WEB_ASYNC_JS(void,await_clipboard,(int* state),{while(Atomics.load(HEAP32,state>>2)===0){const wait=Atomics.waitAsync(HEAP32,state>>2,0);if(wait.async)await wait.value;}});
}
core::Result<ClipboardContent> read_clipboard(){alignas(4) std::array<int,3> state{};
    MAIN_THREAD_EM_ASM({const pointer=$0;const limit=$1;const done=(code)=>{Atomics.store(HEAP32,pointer>>2,code);Atomics.notify(HEAP32,pointer>>2);};if(!navigator.clipboard){done(-1);return;}try{navigator.clipboard.readText().then(text=>{let output=0;try{const encoded=new TextEncoder().encode(text);const count=encoded.length;if(count>limit){done(-2);return;}output=_malloc(count+1);if(!output){done(-3);return;}HEAPU8.set(encoded,output);HEAPU8[output+count]=0;HEAP32[(pointer>>2)+1]=count;HEAP32[(pointer>>2)+2]=output;done(1);}catch(_){if(output)_free(output);done(-3);}},()=>done(-1));}catch(_){done(-1);}},state.data(),maximum_clipboard_text_bytes);
    await_clipboard(state.data());if(state[0]==-2)return core::fail(core::ErrorCode::out_of_range,"Clipboard exceeds the text limit");if(state[0]!=1)return core::fail(core::ErrorCode::failed,"Browser denied clipboard access or could not allocate the text");
    const auto address=static_cast<std::uintptr_t>(static_cast<std::uint32_t>(state[2]));std::unique_ptr<char,decltype(&std::free)> bytes(reinterpret_cast<char*>(address),&std::free);
    ClipboardContent result;result.text.assign(bytes.get(),std::size_t(state[1]));if(!detail::valid_clipboard(result))return core::fail(core::ErrorCode::invalid_argument,"Invalid clipboard text");return result;
}
core::Result<void> write_clipboard(const ClipboardContent& value){if(!detail::valid_clipboard(value))return core::fail(core::ErrorCode::invalid_argument,"Invalid clipboard payload");if(!value.image.empty()||!value.files.empty())return core::fail(core::ErrorCode::unsupported,"Browser clipboard currently supports text and URLs");alignas(4) std::array<int,2> state{};const auto& text=value.text.empty()?value.url:value.text;
    MAIN_THREAD_EM_ASM({const pointer=$0;const done=(code)=>{Atomics.store(HEAP32,pointer>>2,code);Atomics.notify(HEAP32,pointer>>2);};if(!navigator.clipboard){done(-1);return;}try{const text=new TextDecoder().decode(HEAPU8.subarray($1,$1+$2));navigator.clipboard.writeText(text).then(()=>done(1),()=>done(-1));}catch(_){done(-1);}},state.data(),text.data(),text.size());await_clipboard(state.data());if(state[0]!=1)return core::fail(core::ErrorCode::failed,"Browser denied clipboard write; a secure context and user activation may be required");return {};
}
core::Result<void> open_external_url(std::string_view value){if(!detail::valid_external_url(value))return core::fail(core::ErrorCode::invalid_argument,"Invalid external URL");std::string url(value);const auto opened=MAIN_THREAD_EM_ASM_INT({const target=window.open(UTF8ToString($0),'_blank');if(!target)return 0;target.opener=null;return 1;},url.c_str());if(!opened)return core::fail(core::ErrorCode::failed,"Browser blocked the external URL; call from a user action");return {};}
}
