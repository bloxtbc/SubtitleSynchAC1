#include <Windows.h>
#include <MinHook.h>
#include <d3d9.h>
#include <cstdio>

#include "hooks/dx9_hook.h"
#include "mod_runtime.h"

#pragma comment(lib, "d3d9.lib")

static void** GetDeviceVTable(LPDIRECT3DDEVICE9 device)
{
    return *reinterpret_cast<void***>(device);
}

bool DX9Hook::install(IRendererBackend* backend, ModRuntime* mod)
{
    g_backend = backend;
    g_mod = mod;

    IDirect3D9* d3d = Direct3DCreate9(D3D_SDK_VERSION);
    if (!d3d)
        return false;

    D3DPRESENT_PARAMETERS pp = {};
    pp.Windowed = TRUE;
    pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    pp.hDeviceWindow = GetForegroundWindow();

    LPDIRECT3DDEVICE9 device = nullptr;

    if (FAILED(d3d->CreateDevice(
        D3DADAPTER_DEFAULT,
        D3DDEVTYPE_HAL,
        pp.hDeviceWindow,
        D3DCREATE_SOFTWARE_VERTEXPROCESSING,
        &pp,
        &device)))
    {
        d3d->Release();
        return false;
    }

    void** vtable = GetDeviceVTable(device);

    void* endSceneAddr = vtable[42];
    void* resetAddr    = vtable[16];

    device->Release();
    d3d->Release();

    if (MH_Initialize() != MH_OK)
        return false;


    MH_STATUS endSceneHook = MH_CreateHook(
        endSceneAddr,
        reinterpret_cast<LPVOID>(&hkEndScene),
        reinterpret_cast<LPVOID*>(&oEndScene)
    );

    if (endSceneHook != MH_OK)
        return false;

    if (MH_EnableHook(endSceneAddr) != MH_OK)
        return false;


    MH_STATUS resetHook = MH_CreateHook(
        resetAddr,
        reinterpret_cast<LPVOID>(&hkReset),
        reinterpret_cast<LPVOID*>(&oReset)
    );

    if (resetHook != MH_OK)
        return false;

    if (MH_EnableHook(resetAddr) != MH_OK)
        return false;

    return true;
}

void DX9Hook::remove()
{
    MH_DisableHook(MH_ALL_HOOKS);
    MH_Uninitialize();
}

long __stdcall DX9Hook::hkEndScene(LPDIRECT3DDEVICE9 device)
{
    if (!g_initialized)
    {
        g_device = device;

        D3DDEVICE_CREATION_PARAMETERS params;
        device->GetCreationParameters(&params);

        g_hwnd = params.hFocusWindow;
        g_mod->m_window = params.hFocusWindow;

        oWndProc = (WNDPROC)SetWindowLongPtr(
            g_hwnd,
            GWLP_WNDPROC,
            (LONG_PTR)hkWndProc);

        g_backend->init(g_hwnd, device);
        g_mod->init();

        g_initialized = true;

        printf("[DX9Hook] Initialized\n");
    }

    g_backend->newFrame();
    g_mod->update();

    g_backend->render();

    return oEndScene(device);
}

long __stdcall DX9Hook::hkReset(LPDIRECT3DDEVICE9 device, D3DPRESENT_PARAMETERS* pp)
{
    if (g_backend)
        g_backend->invalidateDevice();

    long hr = oReset(device, pp);

    if (SUCCEEDED(hr))
    {
        if (g_backend)
            g_backend->recreateDevice();
    }

    return hr;
}

LRESULT CALLBACK DX9Hook::hkWndProc(HWND hwnd, UINT msg, WPARAM w, LPARAM l)
{
    if (g_backend &&  g_backend->wndProc(hwnd, msg, w, l))
        return TRUE;

    if (oWndProc)
        return CallWindowProc(oWndProc, hwnd, msg, w, l);

    return DefWindowProc(hwnd, msg, w, l);
}