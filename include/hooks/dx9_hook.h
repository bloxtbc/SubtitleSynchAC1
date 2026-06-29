#pragma once

#include <Windows.h>
#include <d3d9.h>

#include <core/renderer_backend.h>

class ModRuntime;


typedef long(__stdcall* EndScene)(LPDIRECT3DDEVICE9);
typedef long(__stdcall* Reset)(LPDIRECT3DDEVICE9, D3DPRESENT_PARAMETERS*);
typedef LRESULT(CALLBACK* WndProc)(HWND, UINT, WPARAM, LPARAM);

class DX9Hook {
public:
    static bool install(IRendererBackend* backend, ModRuntime* mod);
    static void remove();
private:
    static long __stdcall hkEndScene(LPDIRECT3DDEVICE9 device);
    static long __stdcall hkReset(LPDIRECT3DDEVICE9 device, D3DPRESENT_PARAMETERS* pp);
    static LRESULT CALLBACK hkWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

private:
    inline static IRendererBackend* g_backend = nullptr;
    inline static ModRuntime* g_mod = nullptr;

    inline static EndScene oEndScene = nullptr;
    inline static Reset oReset = nullptr;
    inline static WndProc oWndProc = nullptr;

    inline static HWND g_hwnd = nullptr;
    inline static LPDIRECT3DDEVICE9 g_device = nullptr;
    inline static bool g_initialized = false;
};