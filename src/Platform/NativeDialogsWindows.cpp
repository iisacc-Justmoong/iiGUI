#define NOMINMAX
#include <windows.h>
#include <shobjidl.h>
#include <wrl/client.h>
#include <iiGUI/NativeDesktop_p.hpp>
#include <iiGUI/DesktopCommon.h>
#include <atomic>
#include <thread>

namespace ii::gui::detail {
namespace {
using Microsoft::WRL::ComPtr;
constexpr UINT cancel_dialog=WM_APP+0x491;
std::wstring wide(std::string_view text){int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text.data(),int(text.size()),nullptr,0);if(n<=0&&!text.empty())throw std::invalid_argument("Invalid UTF-8 dialog text");std::wstring value(std::size_t(n),L'\0');if(n)MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text.data(),int(text.size()),value.data(),n);return value;}
std::string utf8(const wchar_t* text){if(!text)return {};const auto size=wcslen(text);const int n=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,text,int(size),nullptr,0,nullptr,nullptr);if(n<0)throw std::runtime_error("Invalid native dialog path");std::string value(std::size_t(n),'\0');if(n)WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,text,int(size),value.data(),n,nullptr,nullptr);return value;}
void checked(HRESULT result,const char* message){if(FAILED(result))throw std::runtime_error(message);}
struct DialogState final {
    std::atomic<HWND> window=nullptr;std::atomic<bool> canceled=false;IFileDialog* dialog=nullptr;
    void cancel(){canceled.store(true);if(auto handle=window.load())PostMessageW(handle,cancel_dialog,0,0);}
    static LRESULT CALLBACK procedure(HWND handle,UINT code,WPARAM w,LPARAM l){auto* state=reinterpret_cast<DialogState*>(GetWindowLongPtrW(handle,GWLP_USERDATA));if(code==WM_NCCREATE){state=static_cast<DialogState*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);SetWindowLongPtrW(handle,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(state));}if(code==cancel_dialog&&state){if(state->dialog)state->dialog->Close(HRESULT_FROM_WIN32(ERROR_CANCELLED));return 0;}return DefWindowProcW(handle,code,w,l);}
};
struct Owner final{std::shared_ptr<DialogState> state;std::jthread worker;~Owner(){state->cancel();}};
}
core::Result<std::shared_ptr<NativeDialog>> native_file_dialog(void* parent,FileDialogOptions options,UiExecutor executor,FileDialogCompletion completion){
    if(!valid_dialog(options)||!completion)return core::fail(core::ErrorCode::invalid_argument,"Invalid native file dialog options");
    try{auto state=std::make_shared<DialogState>();auto owner=std::make_shared<Owner>();owner->state=state;
        owner->worker=std::jthread([state,parent,options=std::move(options),executor,completion=std::move(completion)]()mutable{
            core::Result<FileDialogSelection> result=FileDialogSelection{};bool initialized=false;HWND gate=nullptr;std::wstring class_name=L"iiGUI.Dialog."+std::to_wstring(reinterpret_cast<std::uintptr_t>(state.get()));
            try{checked(CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED|COINIT_DISABLE_OLE1DDE),"Cannot initialize native dialog apartment");initialized=true;
                WNDCLASSW type{};type.lpfnWndProc=DialogState::procedure;type.hInstance=GetModuleHandleW(nullptr);type.lpszClassName=class_name.c_str();if(!RegisterClassW(&type))throw std::runtime_error("Cannot create the dialog cancellation endpoint");gate=CreateWindowW(class_name.c_str(),L"",0,0,0,0,0,HWND_MESSAGE,nullptr,type.hInstance,state.get());if(!gate)throw std::runtime_error("Cannot create the dialog cancellation window");state->window.store(gate);
                ComPtr<IFileDialog> dialog;checked(CoCreateInstance(options.mode==FileDialogMode::save?CLSID_FileSaveDialog:CLSID_FileOpenDialog,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&dialog)),"Cannot create native file picker");state->dialog=dialog.Get();DWORD flags=0;checked(dialog->GetOptions(&flags),"Cannot read native picker options");flags|=FOS_FORCEFILESYSTEM|FOS_NOCHANGEDIR;if(options.mode==FileDialogMode::directory)flags|=FOS_PICKFOLDERS;if(options.multiple)flags|=FOS_ALLOWMULTISELECT;if(options.mode==FileDialogMode::save)flags|=FOS_OVERWRITEPROMPT;checked(dialog->SetOptions(flags),"Cannot configure native file picker");
                auto title=wide(options.title),filename=wide(options.filename);if(!title.empty())checked(dialog->SetTitle(title.c_str()),"Cannot set picker title");if(!filename.empty())checked(dialog->SetFileName(filename.c_str()),"Cannot set picker filename");
                if(!options.initial_directory.empty()){ComPtr<IShellItem> folder;auto path=wide(options.initial_directory);if(SUCCEEDED(SHCreateItemFromParsingName(path.c_str(),nullptr,IID_PPV_ARGS(&folder))))dialog->SetDefaultFolder(folder.Get());}
                std::vector<std::wstring> names,patterns;for(const auto& filter:options.filters){names.push_back(wide(filter.title));std::wstring pattern;for(const auto& extension:filter.extensions){if(!pattern.empty())pattern+=L';';pattern+=L"*."+wide(extension);}patterns.push_back(std::move(pattern));}std::vector<COMDLG_FILTERSPEC> filters;for(std::size_t i=0;i<names.size();++i)filters.push_back({names[i].c_str(),patterns[i].c_str()});if(!filters.empty()&&options.mode!=FileDialogMode::directory)checked(dialog->SetFileTypes(UINT(filters.size()),filters.data()),"Cannot set native picker filters");
                const auto shown=state->canceled.load()?HRESULT_FROM_WIN32(ERROR_CANCELLED):dialog->Show(static_cast<HWND>(parent));
                if(shown==HRESULT_FROM_WIN32(ERROR_CANCELLED))result->canceled=true;else{checked(shown,"Native file picker failed");const auto append=[&](IShellItem* item){PWSTR path=nullptr;checked(item->GetDisplayName(SIGDN_FILESYSPATH,&path),"Picker did not return a local filesystem item");struct Free final{PWSTR value;~Free(){CoTaskMemFree(value);}}free{path};auto text=utf8(path);if(text.size()>32768||result->paths.size()>=256)throw std::length_error("Native picker selection exceeds its limit");result->paths.push_back(std::move(text));};
                    if(options.multiple){ComPtr<IFileOpenDialog> open;checked(dialog.As(&open),"Cannot read native multi-selection");ComPtr<IShellItemArray> items;checked(open->GetResults(&items),"Cannot read native picker results");DWORD count=0;checked(items->GetCount(&count),"Cannot count native picker results");if(count>256)throw std::length_error("Native picker selection exceeds its limit");for(DWORD i=0;i<count;++i){ComPtr<IShellItem> item;checked(items->GetItemAt(i,&item),"Cannot read selected item");append(item.Get());}}else{ComPtr<IShellItem> item;checked(dialog->GetResult(&item),"Cannot read selected item");append(item.Get());}}
                state->dialog=nullptr;
            }catch(...){result=core::fail(core::ErrorCode::failed,"Native Windows file dialog failed",std::current_exception());}
            state->window.store(nullptr);if(gate)DestroyWindow(gate);UnregisterClassW(class_name.c_str(),GetModuleHandleW(nullptr));if(initialized)CoUninitialize();
            if(!state->canceled.load()){auto admitted=executor.post([completion=std::move(completion),result=std::move(result)]()mutable{completion(std::move(result));});(void)admitted;}
        });
        auto dialog=std::make_shared<NativeDialog>();dialog->owner=owner;dialog->cancel=[state]{state->cancel();};return dialog;
    }catch(...){return core::fail(core::ErrorCode::failed,"Cannot create a native file dialog",std::current_exception());}
}
}
