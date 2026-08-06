#pragma once

#include <Windows.h>
#include <d3d9.h>

#include <imgui.h>
#include <imgui_impl_dx9.h>
#include <imgui_impl_win32.h>

#include "core/renderer_backend.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

class DX9Backend : public IRendererBackend {
public:
    void init(HWND hwnd, void* device) override
    {
        m_hwnd = hwnd;
        m_device = (LPDIRECT3DDEVICE9)device;

        ImGui::CreateContext();
        ImGui_ImplWin32_Init(hwnd);
        ImGui_ImplDX9_Init(m_device);
    }

    void shutdown() override
    {
        ImGui_ImplDX9_Shutdown();
        ImGui_ImplWin32_Shutdown();
    }

    void newFrame() override
    {
        ImGui_ImplDX9_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
    }

    void render() override
    {
        ImGui::Render();
        ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
    }

    void invalidateDevice() override
    {
        ImGui_ImplDX9_InvalidateDeviceObjects();
    }

    void recreateDevice() override
    {
        ImGui_ImplDX9_CreateDeviceObjects();
    }

    LRESULT wndProc(HWND hwnd, UINT msg, WPARAM w, LPARAM l) override
    {
        const LRESULT result = ImGui_ImplWin32_WndProcHandler(hwnd, msg, w, l);
        ImGuiIO& io = ImGui::GetIO();
        if (result && (io.WantCaptureMouse || io.WantCaptureKeyboard))
            return TRUE;

        return FALSE;
    }

private:
    HWND m_hwnd{};
    LPDIRECT3DDEVICE9 m_device{};
};

