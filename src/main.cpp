#include <Windows.h>

#include "core/renderer_backend.h"
#include "backend/dx9_backend.h"
#include "backend/dx10_backend.h"

#include "mod_runtime.h"
#include "hooks/dx9_hook.h"
#include "hooks/dx10_hook.h"
#include "console.h"
#include "patcher.h"
#include "globals.h"

static DX9Backend g_dx9Backend;
static DX10Backend g_dx10Backend;
static ModRuntime g_mod;

DWORD WINAPI MainThread(LPVOID)
{
    #ifdef ENABLE_CONSOLE
        init_console();
    #endif

    printf("Current Exe Version: %s\n", g_version == eExeVersion::DX9 ? "DX9" : "DX10");

    if (g_version == eExeVersion::DX9) {
        if (!DX9Hook::install(&g_dx9Backend, &g_mod)) {
            MessageBoxA(nullptr, "DX9 hook failed", "Error", MB_ICONERROR);
            return 0;
        }
    } else if (g_version == eExeVersion::DX10) {
        if (!DX10Hook::install(&g_dx10Backend, &g_mod)) {
            MessageBoxA(nullptr, "DX10 hook failed", "Error", MB_ICONERROR);
            return 0;
        }
    } else {
        MessageBoxA(nullptr, "Unsupported executable version", "Error", MB_ICONERROR);
        return 0;
    }

    return 1;
}

BOOL WINAPI DllMain(HINSTANCE hinst, DWORD reason, LPVOID)
{   
    if (reason == DLL_PROCESS_ATTACH)
    {
        if (MEMCMP32(0x00401375 + 1, 0x42D6)) { // dx9
            g_version = eExeVersion::DX9;
        }
        else if (MEMCMP32(0x004013DE + 1, 0x428D)) { // dx10
            g_version = eExeVersion::DX10;
        }

        GetModuleFileNameA(hinst, dllPath, MAX_PATH);

        DisableThreadLibraryCalls(hinst);
        CreateThread(nullptr, 0, MainThread, nullptr, 0, nullptr);
    }

    return TRUE;
}