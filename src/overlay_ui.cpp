#include <imgui.h>
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <misc/cpp/imgui_stdlib.h>

#include "overlay_ui.h"
#include "reshaper/arabic.h"
#include "ini_config.h"
#include "subtitle_settings.h"
#include "subtitle_layout.h"
#include "globals.h"
#include "diagnostics.h"

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

void SubtitleOverlay::showCaptureMarker(uint32_t sequence)
{
    m_captureMarkerSequence = sequence;
    m_captureMarkerUntil = std::chrono::steady_clock::now() + std::chrono::milliseconds(1200);
}

void SubtitleOverlay::drawCaptureMarker()
{
    const auto now = std::chrono::steady_clock::now();
    if (!m_captureMarkerSequence || now >= m_captureMarkerUntil)
        return;

    const ImVec2 display = ImGui::GetIO().DisplaySize;
    if (display.x <= 0.0f || display.y <= 0.0f)
        return;

    const float remaining = std::chrono::duration<float>(m_captureMarkerUntil - now).count();
    const float fade = std::clamp(remaining / 0.30f, 0.0f, 1.0f);
    const float markerFontSize = std::clamp(18.0f * display.y / 1080.0f, 14.0f, 24.0f);
    char label[32]{};
    std::snprintf(label, sizeof(label), "F2 MARK #%03lu",
        static_cast<unsigned long>(m_captureMarkerSequence));

    ImGui::PushFont(g_subtitleSettings.font, markerFontSize);
    const ImVec2 textSize = ImGui::CalcTextSize(label);
    const ImVec2 padding(10.0f, 5.0f);
    ImGui::SetNextWindowPos(
        ImVec2((display.x - textSize.x - padding.x * 2.0f) * 0.5f, 16.0f),
        ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.58f * fade);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, padding);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 5.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.25f, 0.95f, 0.45f, 0.85f * fade));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 1.0f, 0.68f, fade));
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoInputs |
        ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoBringToFrontOnFocus;
    if (ImGui::Begin("CaptureMarkerIndicator", nullptr, flags))
        ImGui::TextUnformatted(label);
    ImGui::End();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(3);
    ImGui::PopFont();
}


void SubtitleOverlay::drawDebugWindow() {
    ImGui::SetNextWindowSize(ImVec2(450, 0), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(0, 0));

    ImGui::Begin("SubtitleSynchAC1 by bloxtbc", &m_debugWindow, ImGuiWindowFlags_AlwaysAutoResize);

    ImGui::TextWrapped("SubtitleSynchAC1 was created by bloxtbc.");
    ImGui::TextWrapped("Co-authors: Hadrorex, Zull11");
    ImGui::Separator();

    ImGui::Checkbox("Show subtitles", &g_subtitleSettings.showSubtitles);
    ImGui::SliderFloat("Subtitle scale", &g_subtitleSettings.scale, 0.5f, 3.0f);
    ImGui::Checkbox("Show sound descriptions ([laughs], [groans])", &g_subtitleSettings.showSoundCaptions);

    ImGui::Separator();
    ImGui::TextWrapped("Preview text (shown while this window is open):");
    ImGui::InputTextMultiline(
        "##preview_text",
        &m_debugInput,
        ImVec2(420.0f, ImGui::GetTextLineHeight() * 4),
        ImGuiInputTextFlags_AllowTabInput
    );

    static bool saveFailed = false;
    if (ImGui::Button("Save to .ini"))
        saveFailed = !m_config.save();
    if (saveFailed)
        ImGui::TextWrapped("Cannot write SubtitleSynchAC1.ini in the mod folder.");

    ImGui::TextWrapped("F1 opens and closes this window.");

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

    drawCaptureMarker();

    if (!m_visible || m_currentText.empty())
    {
        m_layoutLogged = false;
        return;
    }

    std::wstring reshapedW = ShapingEngine::wrender(ShapingEngine::Helper::widen(m_currentText));
    std::string reshaped = ShapingEngine::Helper::narrow(reshapedW);

    // Win32 refreshes DisplaySize from the game's client area every frame,
    // including fullscreen resolution changes. Never use desktop dimensions.
    const ImVec2 display = ImGui::GetIO().DisplaySize;
    if (display.x <= 0 || display.y <= 0) return;
    ImGui::PushFont(g_subtitleSettings.font, subtitleFontSize(g_subtitleSettings, display));
    const float saveLift = (m_saveIndicatorActive || m_pressPromptActive)
        ? g_subtitleSettings.saveIndicatorLift : 0.0f;
    const auto layout = measureSubtitle(g_subtitleSettings, display, reshaped.c_str(), saveLift);
    const unsigned liftSources = (m_saveIndicatorActive ? 1u : 0u) | (m_pressPromptActive ? 2u : 0u);
    if (Diagnostics::enabled()) {
        if (!m_layoutLogged || layout.position.x != m_lastLayoutX || layout.position.y != m_lastLayoutY ||
            layout.size.x != m_lastLayoutWidth || layout.size.y != m_lastLayoutHeight ||
            liftSources != m_lastLayoutLiftSources) {
            Diagnostics::log("subtitle_layout x=%.1f y=%.1f width=%.1f height=%.1f lines=%zu lift=%.1f save=%u interaction=%u screen=%.0fx%.0f",
                layout.position.x, layout.position.y, layout.size.x, layout.size.y, layout.lines.size(),
                saveLift, m_saveIndicatorActive, m_pressPromptActive, display.x, display.y);
            m_lastLayoutX = layout.position.x; m_lastLayoutY = layout.position.y;
            m_lastLayoutWidth = layout.size.x; m_lastLayoutHeight = layout.size.y;
            m_lastLayoutLiftSources = liftSources;
            m_layoutLogged = true;
        }
    } else m_layoutLogged = false;

    ImGui::PushStyleColor(ImGuiCol_Text, m_secondary ? g_subtitleSettings.secondaryTextColor : g_subtitleSettings.textColor);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowMinSize, ImVec2(1, 1));
    ImGui::SetNextWindowPos(layout.position, ImGuiCond_Always);
    ImGui::SetNextWindowSize(layout.size, ImGuiCond_Always);

    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoFocusOnAppearing |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoBackground;
    if (ImGui::Begin("SubtitleOverlay", nullptr, flags))
    {
        const ImVec2 origin = ImGui::GetWindowPos();
        const ImU32 color = ImGui::GetColorU32(ImGuiCol_Text);
        const ImU32 backgroundColor = ImGui::ColorConvertFloat4ToU32(g_subtitleSettings.backgroundColor);
        for (const auto& line : layout.lines) {
            if (line.text.empty()) continue;
            const ImVec2 start(origin.x + line.backgroundOffset.x,
                               origin.y + line.backgroundOffset.y);
            const ImVec2 finish(start.x + line.backgroundSize.x,
                                start.y + line.backgroundSize.y);
            ImGui::GetWindowDrawList()->AddRectFilled(start, finish, backgroundColor, 4.0f);
        }
        for (const auto& line : layout.lines)
            ImGui::GetWindowDrawList()->AddText(
                ImVec2(origin.x + line.offset.x, origin.y + line.offset.y), color, line.text.c_str());
    }
    ImGui::End();
    ImGui::PopStyleVar(4);
    ImGui::PopStyleColor();
    ImGui::PopFont();
}
