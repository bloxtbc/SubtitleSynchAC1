#pragma once
#include <chrono>
#include <cstdint>
#include <vector>

#include "audio_resolver.h"

// Largest listener distance (metres) at which an ambient line is shown, by
// speaker kind: 0x2097 citizens to rescue, 0x20d3 their thanks, 0x2098 guards,
// 0x20b6 mission targets, 0x20a0 preachers (heard from far away).
float secondaryMaxDistance(uint32_t voiceId);

// An ambient (secondary database) voice that is still playing.
struct AmbientVoice {
    uint32_t id = 0;
    uintptr_t instance = 0;
    uint32_t emitter = 0xffffffffu;
    std::chrono::steady_clock::time_point start{};
    double audioSeconds = 3.0;  // database duration, without the display tail
    bool levelKnown = false;
    bool positional = false;
    float distance = -1.0f;
    bool inRange = true;
};

using ReadVoiceLevel = VoiceLevel (*)(uintptr_t instance);

// Ambient voices keep playing while their subtitle is hidden. The tracker
// follows them until their audio ends, so a speaker who comes into range
// mid-line is shown again, and one who leaves the range is hidden.
class AmbientVoices {
public:
    // Margin against flicker at the range limit, and the distance by which a
    // speaker of the same kind must be nearer to take over the line.
    static constexpr float LeaveMargin = 2.0f;
    static constexpr float SwitchMargin = 3.0f;

    // A voice started. A newer line of the same speaker replaces the older one.
    void add(const AmbientVoice& voice);
    // Drop voices whose audio ended or whose instance changed; refresh the
    // distance of the others. read may be null (no live distance).
    void update(std::chrono::steady_clock::time_point now, ReadVoiceLevel read);
    // The voice to show. current (may be null) keeps the line unless a speaker
    // of a higher rank, or of the same rank and clearly nearer, is in range.
    const AmbientVoice* choose(const AmbientVoice* current) const;
    const AmbientVoice* find(uint32_t id, uintptr_t instance) const;
    void clear() { m_voices.clear(); }
    size_t size() const { return m_voices.size(); }

private:
    static void applyLevel(AmbientVoice& voice, const VoiceLevel& level);
    std::vector<AmbientVoice> m_voices;
};
