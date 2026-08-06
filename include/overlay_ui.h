#pragma once

#include <Windows.h>
#include <string>
#include <vector>
#include <chrono>

#include "subtitle_core.h"
#include "ini_config.h"

class SubtitleOverlay {
public:
    void init();

    void setText(const std::string& text);
    void setSegments(const std::vector<SubtitleSegment>& segments);

    void update();
    void render(HWND window);
    void drawDebugWindow();

    void setVisible(bool v) { m_visible = v; }
    bool isVisible() const { return m_visible; }
    bool isDebugVisible() const { return m_debugVisible; }
    bool isDebugWindowVisible() const { return m_debugWindow; }
    std::string currentText() const { return m_currentText; }
    std::string debugText() const { return m_debugInput; }

private:
    void advanceSegment();
    
private:
    std::string m_rawText;
    std::vector<SubtitleSegment> m_segments;
    
    size_t m_index = 0;
    
    std::chrono::steady_clock::time_point m_visibleUntil{};
    std::chrono::steady_clock::time_point m_segmentUntil{};
    
    bool m_visible = false;
    bool m_debugVisible = false;
    bool m_debugWindow = false;
    
    std::string m_currentText;
    std::string m_debugInput;

    SubtitleConfig m_config;
};