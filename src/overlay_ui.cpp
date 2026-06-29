#include <imgui.h>
#include <chrono>
#include <misc/cpp/imgui_stdlib.h>
#include <array>

#include "overlay_ui.h"
#include "reshaper/arabic.h"
#include "ini_config.h"
#include "subtitle_settings.h"
#include "globals.h"

SubtitleSettings g_subtitleSettings;

void SubtitleOverlay::init() {
    m_config.load();

    ImGuiIO& io = ImGui::GetIO();
    g_subtitleSettings.font = io.Fonts->AddFontDefaultVector();
    io.ConfigFlags = ImGuiConfigFlags_NoMouseCursorChange;

    std::string fullPath = dllPath;

    fullPath = fullPath.substr(0, fullPath.find_last_of("\\/"));
    fullPath += "\\subtitles.ttf";

    printf("Font full path: %s\n", fullPath.c_str());
    ImFontConfig config;
    config.MergeMode = false;

    static const ImWchar fontRanges[] =
    {
        0x0, 0xFFFF, 0x0,
    };

    ImFont* f = io.Fonts->AddFontFromFileTTF(
        fullPath.c_str(),
        30.0f,
        &config,
        fontRanges
    );

    if (!f)
        printf("Font failed to load!\n");

    if (f)
        g_subtitleSettings.font = f;
}

static std::wstring reshape(const std::string& s)
{
    return ShapingEngine::wrender(ShapingEngine::Helper::widen(s));
}

void SubtitleOverlay::setText(const std::string& text)
{
    m_rawText = text;
    m_currentText = text;
    
    m_segments.clear();
    m_index = 0;
}

void SubtitleOverlay::setSegments(const std::vector<SubtitleSegment>& segments)
{
    m_segments = segments;
    m_index = 0;

    if (!m_segments.empty())
    {
        m_currentText = m_segments[0].text;

        double wait = m_segments[0].waitAfter;

        if (wait > 0.0)
        {
            m_segmentUntil = std::chrono::steady_clock::now() +
                std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                    std::chrono::duration<double>(wait));
        }
        else
        {
            m_segmentUntil = std::chrono::steady_clock::time_point{};
        }
    }
}

void SubtitleOverlay::update()
{
    if (!m_visible || m_segments.empty())
        return;

    auto now = std::chrono::steady_clock::now();

    if (m_segmentUntil != std::chrono::steady_clock::time_point{} &&
        now >= m_segmentUntil)
    {
        advanceSegment();
    }

    if (m_visibleUntil != std::chrono::steady_clock::time_point{} &&
        now >= m_visibleUntil)
    {
        m_visible = false;
        m_segments.clear();
        m_index = 0;
    }
}

void SubtitleOverlay::advanceSegment()
{
    if (m_segments.empty())
        return;

    ++m_index;

    if (m_index >= m_segments.size())
    {
        m_visible = false;
        return;
    }

    m_currentText = m_segments[m_index].text;

    double wait = m_segments[m_index].waitAfter;

    if (wait > 0.0)
    {
        m_segmentUntil = std::chrono::steady_clock::now() +
            std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                std::chrono::duration<double>(wait));
    }
    else
    {
        m_segmentUntil = std::chrono::steady_clock::time_point{};
    }
}


void SubtitleOverlay::drawDebugWindow() {
    ImGui::SetNextWindowSize(ImVec2(300, 550));
    ImGui::SetNextWindowPos(ImVec2(0, 0));

    ImGui::Begin("SubtitleSynchAC1 by bloxtbc", &m_debugWindow, ImGuiWindowFlags_None);

    ImGui::Separator();

    ImGui::TextWrapped("This mod was developed and created by bloxtbc on NexusMods, support me and report any bugs if there are any on the NexusMods page for SubtitleSynchAC1.");

    ImGui::Checkbox("Show subtitle", &m_debugVisible);
    ImGui::Checkbox("Auto position", &g_subtitleSettings.autoPosition);

    ImGui::ColorEdit4("Text Color", (float*)&g_subtitleSettings.textColor);
    ImGui::ColorEdit4("Background Color", (float*)&g_subtitleSettings.backgroundColor);

    ImGui::Text("Loaded font: %s", g_subtitleSettings.font ? "yes" : "no");

    ImGuiIO& io = ImGui::GetIO();
    ImGui::Text("Mouse: %.1f %.1f", io.MousePos.x, io.MousePos.y);

    ImGui::DragFloat2("Padding", (float*)&g_subtitleSettings.padding, 0.1f, -10.0f, 10.0f);
    ImVec2 screenSize = io.DisplaySize;
    if (g_subtitleSettings.autoPosition) {
        g_subtitleSettings.position = ImVec2(screenSize.x / 2.0f, screenSize.y - 100.0f);
    }
    else {
        ImGui::DragFloat2("Subtitle position", (float*)&g_subtitleSettings.position, 1.0f, 0.0f, screenSize.x);
    }

    ImGui::SliderFloat("Font Scale", &g_subtitleSettings.scale, 0.5f, 3.0f);

    if (ImGui::Button("Reset to defaults"))
    {
        ImVec2 screenSize = io.DisplaySize;
        g_subtitleSettings.autoPosition = true;
        g_subtitleSettings.position = ImVec2(screenSize.x / 2.0f, screenSize.y - 100.0f);
        g_subtitleSettings.padding = ImVec2(8, -25);
        g_subtitleSettings.textColor = ImVec4(1, 1, 1, 1);
        g_subtitleSettings.backgroundColor = ImVec4(0, 0, 0, 0.5f);
        g_subtitleSettings.scale = 1.0f;
    }

    ImGui::Separator();

    ImGui::InputTextMultiline(
        "##debug_output",
        &m_debugInput,
        ImVec2(-1.0f, ImGui::GetTextLineHeight() * 8),
        ImGuiInputTextFlags_AllowTabInput
    );

    if (ImGui::Button("Save to .ini"))
       m_config.save();

    ImGui::TextWrapped("F1 toggles this debug window.");

    ImGui::End();
}

void SubtitleOverlay::render(HWND window)
{
    if (GetAsyncKeyState(VK_F1) & 1) {
        m_debugWindow = !m_debugWindow;
    }

    ImGui::GetIO().MouseDrawCursor = m_debugWindow;

    if (m_debugWindow)
        drawDebugWindow();

    if (!m_visible || m_currentText.empty())
        return;

    std::wstring reshapedW = ShapingEngine::wrender(ShapingEngine::Helper::widen(m_currentText));
    std::string reshaped = ShapingEngine::Helper::narrow(reshapedW);

    ImGui::PushStyleColor(ImGuiCol_WindowBg, g_subtitleSettings.backgroundColor);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

    float fontSize = g_subtitleSettings.fontSize * g_subtitleSettings.scale;

    if (g_subtitleSettings.font) {
        ImGui::PushFont(g_subtitleSettings.font);
        ImGui::PushFont(NULL, fontSize);
    }

    ImVec2 textSize = ImGui::CalcTextSize(reshaped.c_str());

    if (g_subtitleSettings.font) {
        ImGui::PopFont();
        ImGui::PopFont();
    }

    ImVec2 totalSize = {
        textSize.x + g_subtitleSettings.padding.x,
        textSize.y + g_subtitleSettings.padding.y
    };

    ImVec2 pos = {
        g_subtitleSettings.position.x - totalSize.x * 0.5f,
        g_subtitleSettings.position.y - totalSize.y * 0.5f
    };

    ImGui::SetNextWindowPos(pos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(totalSize, ImGuiCond_Always);

    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoSavedSettings;

    if (ImGui::Begin("SubtitleOverlay", nullptr, flags))
    {
        ImGui::PushStyleColor(ImGuiCol_Text, g_subtitleSettings.textColor);

        float fontSize = g_subtitleSettings.fontSize * g_subtitleSettings.scale;

        if (g_subtitleSettings.font)
        {
            ImGui::PushFont(g_subtitleSettings.font);
            ImGui::PushFont(NULL,fontSize);
        }

        ImGui::SetCursorPosX((ImGui::GetWindowSize().x - textSize.x) * 0.5f);
        ImGui::SetCursorPosY((ImGui::GetWindowSize().y - textSize.y) * 0.5f);

        ImGui::TextUnformatted(reshaped.c_str());

        if (g_subtitleSettings.font)
        {
            ImGui::PopFont();
            ImGui::PopFont();
        }

        ImGui::PopStyleColor();
    }

    ImGui::End();

    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();
}

void SubtitleOverlay::updateImGuiInput(bool showDebugWindow, HWND window)
{
    ImGuiIO& io = ImGui::GetIO();

    POINT mousePos;
    GetCursorPos(&mousePos);
    ScreenToClient(window, &mousePos);

    io.AddMousePosEvent((float)mousePos.x, (float)mousePos.y);
    io.AddMouseButtonEvent(0, (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0);
    io.AddMouseButtonEvent(1, (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0);
    io.AddMouseButtonEvent(2, (GetAsyncKeyState(VK_MBUTTON) & 0x8000) != 0);


    static std::array<bool, 256> wasDown{};
    
    for (int vk = 0; vk < 256; vk++)
    {
        bool down = (GetAsyncKeyState(vk) & 0x8000) != 0;

        if (down != wasDown[vk])
        {
            wasDown[vk] = down;

            ImGuiKey key = ImGuiKey_None;

            switch (vk)
            {
                case VK_CONTROL: key = ImGuiKey_LeftCtrl; break;
                case VK_SHIFT:   key = ImGuiKey_LeftShift; break;
                case VK_MENU:    key = ImGuiKey_LeftAlt; break;
                case VK_LWIN:
                case VK_RWIN:    key = ImGuiKey_LeftSuper; break;

                case VK_BACK:    key = ImGuiKey_Backspace; break;
                case VK_TAB:     key = ImGuiKey_Tab; break;
                case VK_RETURN:  key = ImGuiKey_Enter; break;
                case VK_ESCAPE:  key = ImGuiKey_Escape; break;
                case VK_SPACE:   key = ImGuiKey_Space; break;

                case VK_LEFT:    key = ImGuiKey_LeftArrow; break;
                case VK_RIGHT:   key = ImGuiKey_RightArrow; break;
                case VK_UP:      key = ImGuiKey_UpArrow; break;
                case VK_DOWN:    key = ImGuiKey_DownArrow; break;

                case VK_DELETE:  key = ImGuiKey_Delete; break;

                default:
                    if (vk >= 'A' && vk <= 'Z')
                        key = (ImGuiKey)(ImGuiKey_A + (vk - 'A'));
                    else if (vk >= '0' && vk <= '9')
                        key = (ImGuiKey)(ImGuiKey_0 + (vk - '0'));
                    break;
            }

            if (key != ImGuiKey_None)
                io.AddKeyEvent(key, down);
        }
    }
}