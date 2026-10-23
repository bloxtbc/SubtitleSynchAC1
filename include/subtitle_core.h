#pragma once

#include <string>
#include <cstdint>
#include <vector>
#include <unordered_map>

struct SubtitleSegment {
    std::string text;
    double waitAfter = 0.0;
};

enum class SubtitlePriority : uint32_t {
    None = 0,
    Secondary = 1,
    Primary = 2,
};

struct SubtitleMatch {
    std::string raw;
    SubtitlePriority priority = SubtitlePriority::None;

    explicit operator bool() const { return !raw.empty(); }
};

bool shouldDisplaySubtitle(SubtitlePriority active, SubtitlePriority incoming);

// Ambient speakers compete for the one subtitle line. Banks of the packaged
// secondary database: 0x2097 citizens to rescue and 0x20d3 their thanks rank
// above 0x2098 guards and 0x20b6 mission targets, which rank above preachers
// and anything else.
int secondaryRank(uint32_t voiceId);

struct SecondaryCandidate {
    uint32_t id = 0;
    bool levelKnown = false;
    float levelDb = 0.0f;
};
// Should an incoming secondary line replace the active secondary line?
// activeSpeaking: the active line's audio has not ended yet (only its tail).
bool shouldReplaceSecondary(const SecondaryCandidate& active, bool activeSpeaking,
                            const SecondaryCandidate& incoming);

// Drop bracketed sound descriptions such as "[laughs]" from display text and
// tidy the spaces around them. Timing segments are left untouched.
std::string removeSoundCaptions(const std::string& text);

class SubtitleEngine {
public:
    bool load(const std::string& filepath);
    bool loadSecondary(const std::string& filepath);

    std::string getRaw(uint32_t voiceId) const;
    SubtitleMatch getMatch(uint32_t voiceId) const;
    SubtitlePriority getPriority(uint32_t voiceId) const;

    std::vector<SubtitleSegment> getSegments(uint32_t voiceId) const;

    static std::vector<SubtitleSegment> parseSegments(const std::string& text);

    static std::string stripTags(const std::string& s);
    static double extractDuration(const std::string& s);

private:
    std::unordered_map<std::string, std::string> m_primaryDb;
    std::unordered_map<std::string, std::string> m_secondaryDb;

    static bool loadFile(const std::string& filepath,
                         std::unordered_map<std::string, std::string>& destination);

    static std::string makeKey(uint32_t voiceId);
};

extern SubtitleEngine g_SubtitleEngine;
