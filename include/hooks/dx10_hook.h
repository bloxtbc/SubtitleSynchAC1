#pragma once

#include <Windows.h>
#include <d3d10.h>

#include <core/renderer_backend.h>

class ModRuntime;

typedef HRESULT(__stdcall* Present)(IDXGISwapChain*, UINT, UINT);
typedef HRESULT(__stdcall* ResizeBuffers)(IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT);
typedef LRESULT(CALLBACK* WndProc)(HWND, UINT, WPARAM, LPARAM);


class DX10Hook {
public:
    static bool install(IRendererBackend* backend, ModRuntime* mod);
    static void remove();

private:
    static HRESULT __stdcall hkPresent(IDXGISwapChain* swapChain, UINT syncInterval, UINT flags);
    static HRESULT __stdcall hkResizeBuffers(
        IDXGISwapChain* swapChain,
        UINT bufferCount,
        UINT width,
        UINT height,
        DXGI_FORMAT newFormat,
        UINT swapChainFlags);

    static LRESULT CALLBACK hkWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

private:
    inline static IRendererBackend* g_backend = nullptr;
    inline static ModRuntime* g_mod = nullptr;

    inline static Present oPresent = nullptr;
    inline static ResizeBuffers oResizeBuffers = nullptr;
    inline static WndProc oWndProc = nullptr;

    inline static HWND g_hwnd = nullptr;
    inline static ID3D10Device* g_device = nullptr;
    inline static IDXGISwapChain* g_swapchain = nullptr;

    inline static bool g_initialized = false;
};