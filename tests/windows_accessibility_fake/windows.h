#pragma once
#include <cstdint>
#include <cwchar>
#include <cstddef>

using BOOL=int;using UINT=std::uint32_t;using DWORD=std::uint32_t;using LONG=std::int32_t;using ULONG=std::uint32_t;
using HRESULT=std::int32_t;using LCID=std::uint32_t;using WORD=std::uint16_t;using DISPID=std::int32_t;
using WPARAM=std::uintptr_t;using LPARAM=std::intptr_t;using LRESULT=std::intptr_t;using HWND=void*;
using BSTR=wchar_t*;using LPOLESTR=wchar_t*;using REFIID=const void*;
struct POINT{LONG x,y;};struct RECT{LONG left,top,right,bottom;};
struct MSG{HWND hwnd;UINT message;WPARAM wParam;LPARAM lParam;DWORD time;POINT pt;};
struct ITypeInfo;struct DISPPARAMS;struct EXCEPINFO;struct IDispatch;
struct VARIANT{std::uint16_t vt=0;union{LONG lVal;IDispatch* pdispVal;};};
#define STDMETHODCALLTYPE
#define S_OK HRESULT(0)
#define S_FALSE HRESULT(1)
#define E_POINTER HRESULT(0x80004003u)
#define E_FAIL HRESULT(0x80004005u)
#define E_NOTIMPL HRESULT(0x80004001u)
#define E_INVALIDARG HRESULT(0x80070057u)
#define E_OUTOFMEMORY HRESULT(0x8007000eu)
#define E_NOINTERFACE HRESULT(0x80004002u)
#define E_ACCESSDENIED HRESULT(0x80070005u)
#define DISP_E_MEMBERNOTFOUND HRESULT(0x80020003u)
#define CO_E_OBJNOTCONNECTED HRESULT(0x800401fdu)
#define RPC_E_CHANGED_MODE HRESULT(0x80010106u)
#define SUCCEEDED(hr) ((hr)>=0)
#define WM_APP 0x8000u
#define WM_GETOBJECT 0x003du
#define OBJID_CLIENT LONG(-4)
#define OBJID_WINDOW LONG(0)
#define CHILDID_SELF 0
#define VT_EMPTY 0
#define VT_I4 3
#define VT_DISPATCH 9
#define CP_UTF8 65001
#define MB_ERR_INVALID_CHARS 8
#define COINIT_APARTMENTTHREADED 2
#define COINIT_DISABLE_OLE1DDE 4
#define SELFLAG_TAKEFOCUS 1
#define EVENT_OBJECT_FOCUS 0x8005u
#define EVENT_OBJECT_REORDER 0x8004u
#define EVENT_OBJECT_VALUECHANGE 0x800eu
#define NAVDIR_FIRSTCHILD 7
#define NAVDIR_LASTCHILD 8
#define NAVDIR_NEXT 5
#define NAVDIR_PREVIOUS 6
template<class T> const void* fake_uuid(){static int value=0;return &value;}
#define __uuidof(T) fake_uuid<T>()
inline bool IsEqualIID(REFIID a,REFIID b){return a==b;}
inline void VariantInit(VARIANT* v){v->vt=VT_EMPTY;v->lVal=0;}

HRESULT CoInitializeEx(void*,DWORD);void CoUninitialize();
BSTR SysAllocStringLen(const wchar_t*,UINT);UINT SysStringLen(BSTR);void SysFreeString(BSTR);
int MultiByteToWideChar(UINT,DWORD,const char*,int,wchar_t*,int);
BOOL GetClientRect(HWND,RECT*);BOOL ClientToScreen(HWND,POINT*);UINT GetDpiForWindow(HWND);
int GetWindowTextLengthW(HWND);int GetWindowTextW(HWND,wchar_t*,int);
BOOL PostMessageW(HWND,UINT,WPARAM,LPARAM);HWND SetFocus(HWND);
void NotifyWinEvent(DWORD,HWND,LONG,LONG);
