#pragma once
#include <Windows.h>

#include <imgui.h>
#include "imgui_impl_win32.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);                // Use ImGui::GetCurrentContext()

class IRendererBackend {
public:
    virtual ~IRendererBackend() = default;

    virtual void init(HWND hwnd, void* device) = 0;
    virtual void shutdown() = 0;

    virtual void newFrame() = 0;
    virtual void render() = 0;

    virtual void invalidateDevice() = 0;
    virtual void recreateDevice() = 0;

    virtual LRESULT wndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
            return TRUE;

        return FALSE;
    }
};