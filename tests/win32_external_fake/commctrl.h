#pragma once
#include <windows.h>
#include <map>
#include <functional>
#include <utility>
using DWORD_PTR=std::uintptr_t;
using SUBCLASSPROC=LRESULT (CALLBACK *)(HWND,UINT,WPARAM,LPARAM,UINT_PTR,DWORD_PTR);
namespace fake_subclass {
struct Record {SUBCLASSPROC procedure;UINT_PTR id;DWORD_PTR data;};
inline std::map<HWND,Record> hooks;
inline constexpr LRESULT default_result=123;
inline std::function<void()> on_default;
inline std::size_t count(){return hooks.size();}
inline LRESULT dispatch(HWND window,UINT message,WPARAM w,LPARAM l){
    auto it=hooks.find(window);if(it==hooks.end())return default_result;
    const auto record=it->second;
    return record.procedure(window,message,w,l,record.id,record.data);
}
}
inline BOOL SetWindowSubclass(HWND window,SUBCLASSPROC callback,UINT_PTR id,DWORD_PTR data){
    if(!fake_win32::windows.contains(window))return FALSE;
    return fake_subclass::hooks.emplace(window,fake_subclass::Record{callback,id,data}).second?TRUE:FALSE;
}
inline BOOL RemoveWindowSubclass(HWND window,SUBCLASSPROC callback,UINT_PTR id){
    auto it=fake_subclass::hooks.find(window);
    if(it==fake_subclass::hooks.end()||it->second.procedure!=callback||it->second.id!=id)return FALSE;
    fake_subclass::hooks.erase(it);return TRUE;
}
inline LRESULT DefSubclassProc(HWND,UINT,WPARAM,LPARAM){if(fake_subclass::on_default)fake_subclass::on_default();return fake_subclass::default_result;}
