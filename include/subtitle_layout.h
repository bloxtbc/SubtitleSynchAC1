#pragma once
#include "subtitle_settings.h"
#include <string>
#include <vector>

struct SubtitleLine {
    std::string text;
    ImVec2 offset;
    float width;
    ImVec2 backgroundOffset;
    ImVec2 backgroundSize;
};

struct SubtitleLayout {
    ImVec2 position, size, padding;
    float wrapWidth;
    std::vector<SubtitleLine> lines;
};

float subtitleFontSize(const SubtitleSettings& settings, ImVec2 display);
// Call with the same ImGui font/size active as will be used for drawing.
SubtitleLayout measureSubtitle(
    const SubtitleSettings& settings, ImVec2 display, const char* text,
    float extraBottomMargin = 0.0f);
