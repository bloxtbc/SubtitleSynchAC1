#include "subtitle_layout.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace {
float bounded(float value, float fallback, float low, float high) {
    return std::clamp(std::isfinite(value) ? value : fallback, low, high);
}
}

float subtitleFontSize(const SubtitleSettings& s, ImVec2 display) {
    const float base = bounded(s.fontSize, 30, 8, 120);
    const float factor = bounded(s.scale, 1, 0.5f, 3);
    const float reference = bounded(s.referenceHeight, 1080, 480, 4320);
    const float resolution = s.autoScale ? bounded(display.y, reference, 1, 16384) / reference : 1;
    return std::clamp(base * factor * resolution, 8.0f, 512.0f);
}

SubtitleLayout measureSubtitle(
    const SubtitleSettings& s, ImVec2 display, const char* text,
    float extraBottomMargin) {
    const float width = bounded(display.x, 1, 1, 32768);
    const float height = bounded(display.y, 1, 1, 16384);
    const float maxWidth = width * bounded(s.maxWidthPercent, 65, 20, 100) / 100;
    const float ratio = s.autoScale ? height / bounded(s.referenceHeight, 1080, 480, 4320) : 1;
    // Old configs used negative vertical padding; never let that clip glyphs.
    ImVec2 padding(bounded(s.padding.x, 8, 0, 100) * ratio,
                   bounded(s.padding.y, 6, 0, 100) * ratio);
    padding.x = std::min(padding.x, std::max(0.0f, (maxWidth - 1) / 2));
    const float wrap = std::max(1.0f, maxWidth - 2 * padding.x);
    std::vector<SubtitleLine> lines;
    const char* end = text + std::strlen(text);
    const char* cursor = text;
    float longest = 0;
    do {
        const char* paragraphEnd = std::find(cursor, end, '\n');
        do {
            const char* next = ImGui::GetFont()->CalcWordWrapPosition(ImGui::GetFontSize(), cursor, paragraphEnd, wrap);
            if (next <= cursor && cursor < paragraphEnd) {
                // Even a single over-wide glyph must advance as a whole UTF-8 character.
                next = cursor + 1;
                while (next < paragraphEnd && (static_cast<unsigned char>(*next) & 0xc0) == 0x80) ++next;
            }
            const char* trimmed = next;
            while (trimmed > cursor && (trimmed[-1] == ' ' || trimmed[-1] == '\t' || trimmed[-1] == '\r')) --trimmed;
            const float lineWidth = ImGui::CalcTextSize(cursor, trimmed, false).x;
            lines.push_back({std::string(cursor, trimmed), {}, lineWidth});
            longest = std::max(longest, lineWidth);
            cursor = next;
            while (cursor < paragraphEnd && (*cursor == ' ' || *cursor == '\t' || *cursor == '\r')) ++cursor;
        } while (cursor < paragraphEnd);
        if (paragraphEnd == end) break;
        cursor = paragraphEnd + 1;
    } while (cursor <= end);
    const ImVec2 textSize(longest, lines.size() * ImGui::GetFontSize());
    ImVec2 size(std::min(maxWidth, textSize.x + 2 * padding.x), textSize.y + 2 * padding.y);
    const float reference = bounded(s.referenceHeight, 1080, 480, 4320);
    const float baseMargin = bounded(s.bottomMargin, 100, 0, 1000) * height / reference;
    const float transientLift = bounded(extraBottomMargin, 0, 0, 2000) * height / reference;
    ImVec2 pos;
    if (s.autoPosition) {
        pos = ImVec2((width - size.x) / 2, height - baseMargin - transientLift - size.y);
    } else {
        pos = ImVec2(bounded(s.position.x, width / 2, 0, width) - size.x / 2,
                     bounded(s.position.y, height / 2, 0, height) - size.y / 2 - transientLift);
    }
    pos.x = std::clamp(pos.x, 0.0f, std::max(0.0f, width - size.x));
    pos.y = std::clamp(pos.y, 0.0f, std::max(0.0f, height - size.y));
    for (size_t i = 0; i < lines.size(); ++i) {
        auto& line = lines[i];
        line.offset = ImVec2((size.x - line.width) / 2, padding.y + i * ImGui::GetFontSize());
        const float top = i == 0 ? 0.0f : padding.y + i * ImGui::GetFontSize();
        const float bottom = i + 1 == lines.size()
            ? size.y : padding.y + (i + 1) * ImGui::GetFontSize();
        line.backgroundOffset = ImVec2(line.offset.x - padding.x, top);
        line.backgroundSize = ImVec2(line.width + 2 * padding.x,
                                     bottom - top);
    }
    return {pos, size, padding, wrap, std::move(lines)};
}
