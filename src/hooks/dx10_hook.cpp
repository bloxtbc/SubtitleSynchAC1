#include <MinHook.h>
#include <d3d10.h>
#include <dxgi.h>
#include <cstdio>

#include "hooks/dx10_hook.h"
#include "mod_runtime.h"
#include "imgui_impl_win32.h"

#pragma comment(lib, "d3d10.lib")
#pragma comment(lib, "dxgi.lib")

static void** GetSwapChainVTable(IDXGISwapChain* sc)
{
    return *reinterpret_cast<void***>(sc);
}
bool DX10Hook::install(IRendererBackend* backend, ModRuntime* mod)
{
    g_backend = backend;
    g_mod = mod;

    while (!GetForegroundWindow())
        Sleep(10);

    HWND hwnd = GetForegroundWindow();

    DXGI_SWAP_CHAIN_DESC scd = {};
    scd.BufferCount = 1;
    scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.OutputWindow = hwnd;
    scd.SampleDesc.Count = 1;
    scd.Windowed = TRUE;
    scd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    ID3D10Device* device = nullptr;
    IDXGISwapChain* swapchain = nullptr;

    if (FAILED(D3D10CreateDeviceAndSwapChain(
        nullptr,
        D3D10_DRIVER_TYPE_HARDWARE,
        nullptr,
        0,
        D3D10_SDK_VERSION,
        &scd,
        &swapchain,
        &device)))
    {
        return false;
    }

    void** vtable = *reinterpret_cast<void***>(swapchain);

    void* presentAddr = vtable[8];

    swapchain->Release();
    device->Release();

    MH_Initialize();

    MH_CreateHook(presentAddr,
        reinterpret_cast<LPVOID>(hkPresent),
        reinterpret_cast<LPVOID*>(&oPresent));

    MH_EnableHook(presentAddr);

    return true;
}

HRESULT __stdcall DX10Hook::hkPresent(
    IDXGISwapChain* swapChain,
    UINT syncInterval,
    UINT flags)
{
    if (!g_initialized)
    {
        g_swapchain = swapChain;

        if (SUCCEEDED(swapChain->GetDevice(__uuidof(ID3D10Device), (void**)&g_device)))
        {
            DXGI_SWAP_CHAIN_DESC desc;
            swapChain->GetDesc(&desc);

            g_hwnd = desc.OutputWindow;
            g_mod->m_window = g_hwnd;

            oWndProc = (WNDPROC)SetWindowLongPtr(
                g_hwnd,
                GWLP_WNDPROC,
                (LONG_PTR)hkWndProc);

            g_backend->init(g_hwnd, g_device);
            g_mod->init();

            g_initialized = true;

            printf("[DX10Hook] Initialized\n");
        }
    }

    g_backend->newFrame();
    g_mod->update();
    g_backend->render();

    return oPresent(swapChain, syncInterval, flags);
}

HRESULT __stdcall DX10Hook::hkResizeBuffers(
    IDXGISwapChain* swapChain,
    UINT bufferCount,
    UINT width,
    UINT height,
    DXGI_FORMAT newFormat,
    UINT swapChainFlags)
{
    if (g_backend)
        g_backend->invalidateDevice();

    HRESULT hr = oResizeBuffers(
        swapChain,
        bufferCount,
        width,
        height,
        newFormat,
        swapChainFlags);

    if (SUCCEEDED(hr))
    {
        if (g_backend)
            g_backend->recreateDevice();
    }

    return hr;
}

LRESULT CALLBACK DX10Hook::hkWndProc(HWND hwnd, UINT msg, WPARAM w, LPARAM l)
{
    if (g_backend &&  g_backend->wndProc(hwnd, msg, w, l))
        return TRUE;

    if (oWndProc)
        return CallWindowProc(oWndProc, hwnd, msg, w, l);

    return DefWindowProc(hwnd, msg, w, l);
}