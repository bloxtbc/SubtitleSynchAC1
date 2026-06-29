#pragma once

#include <string>
#include <cstdint>
#include <vector>
#include <unordered_map>

struct SubtitleSegment {
    std::string text;
    double waitAfter = 0.0;
};

class SubtitleEngine {
public:
    bool load(const std::string& filepath);

    std::string getRaw(uint32_t voiceId) const;

    std::vector<SubtitleSegment> getSegments(uint32_t voiceId) const;

    static std::vector<SubtitleSegment> parseSegments(const std::string& text);

    static std::string stripTags(const std::string& s);
    static double extractDuration(const std::string& s);

private:
    std::unordered_map<std::string, std::string> m_db;

    static std::string makeKey(uint32_t voiceId);
    static std::string makeAltKey(uint32_t voiceId);
};

extern SubtitleEngine g_SubtitleEngine;