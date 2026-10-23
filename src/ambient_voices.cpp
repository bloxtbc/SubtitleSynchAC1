#include "ambient_voices.h"
#include "subtitle_core.h"

float secondaryMaxDistance(uint32_t voiceId) {
    switch (voiceId >> 16) {
    case 0x2097: return 15.0f;
    case 0x20d3: return 20.0f;
    case 0x20a0: return 20.0f;
    default: return 21.0f;  // guards, mission targets, other ambient speakers
    }
}

void AmbientVoices::applyLevel(AmbientVoice& voice, const VoiceLevel& level) {
    voice.levelKnown = level.known;
    voice.positional = level.positional;
    voice.distance = level.distance;
    if (!level.known || !level.positional) {
        voice.inRange = true;  // 2D voice, or no distance: keep the old always-show rule
        return;
    }
    const float limit = secondaryMaxDistance(voice.id);
    voice.inRange = level.distance <= (voice.inRange ? limit + LeaveMargin : limit);
}

void AmbientVoices::add(const AmbientVoice& voice) {
    for (auto it = m_voices.begin(); it != m_voices.end();) {
        const bool sameVoice = it->instance == voice.instance && it->id == voice.id;
        const bool sameSpeaker = voice.emitter != 0xffffffffu && it->emitter == voice.emitter;
        if (sameVoice || sameSpeaker) it = m_voices.erase(it);
        else ++it;
    }
    AmbientVoice added = voice;
    added.inRange = false;
    applyLevel(added, {voice.levelKnown, voice.positional, voice.distance, 0.0f, voice.emitter});
    m_voices.push_back(added);
}

void AmbientVoices::update(std::chrono::steady_clock::time_point now, ReadVoiceLevel read) {
    for (auto it = m_voices.begin(); it != m_voices.end();) {
        const double elapsed = std::chrono::duration<double>(now - it->start).count();
        bool ended = elapsed >= it->audioSeconds;
        // 2D voices have no distance; a voice without a readable instance
        // keeps the state it started with.
        const bool twoD = it->levelKnown && !it->positional;
        if (!ended && read && it->instance && it->emitter != 0xffffffffu && !twoD) {
            const VoiceLevel level = read(it->instance);
            if (level.emitter != it->emitter) ended = true;  // freed or reused instance
            else if (level.known) applyLevel(*it, level);    // else keep the last distance
        }
        if (ended) it = m_voices.erase(it);
        else ++it;
    }
}

namespace {
float orderDistance(const AmbientVoice& voice) {
    return voice.levelKnown && voice.positional ? voice.distance : 0.0f;
}
bool better(const AmbientVoice& a, const AmbientVoice& b) {
    const int ra = secondaryRank(a.id), rb = secondaryRank(b.id);
    if (ra != rb) return ra > rb;
    return orderDistance(a) < orderDistance(b);
}
}

const AmbientVoice* AmbientVoices::choose(const AmbientVoice* current) const {
    const AmbientVoice* best = nullptr;
    for (const auto& voice : m_voices)
        if (voice.inRange && (!best || better(voice, *best))) best = &voice;
    if (!current || !current->inRange || !best || best == current) return best;
    const int rc = secondaryRank(current->id), rb = secondaryRank(best->id);
    if (rb != rc) return rb > rc ? best : current;
    return orderDistance(*best) + SwitchMargin < orderDistance(*current) ? best : current;
}

const AmbientVoice* AmbientVoices::find(uint32_t id, uintptr_t instance) const {
    for (const auto& voice : m_voices)
        if (voice.id == id && voice.instance == instance) return &voice;
    return nullptr;
}
