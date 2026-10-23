#pragma once
#include <cstddef>
#include <cstdint>

enum class Resolution : uint32_t { Exact = 0, DirectReference = 1, Unresolved = 2, LeafVoice = 3 };
struct ResolvedAudio {
    uint32_t id = 0;
    Resolution resolution = Resolution::Unresolved;
};
using ReadAudioMemory = bool (*)(uintptr_t, void*, size_t);

// AC1's manager has a 16-byte resource table; type-1 events hold an audio
// resource handle at object+8. Resource handles are NOT subtitle identifiers.
ResolvedAudio resolveAudio(uint32_t eventId, uintptr_t manager, uint32_t handle,
                           ReadAudioMemory read);

struct VoiceLevel {
    bool known = false;
    bool positional = false;
    float distance = -1.0f;
    float levelDb = 0.0f;
    uint32_t emitter = 0xffffffffu;
};
// Engine distance curve {min, clamp, fade start, max}; -2 = engine default
// {0.5, -1, -1, -1}. Mirrors DX10 0xc595e0: silent (-96 dB) beyond max.
float voiceCurveDb(float distance, const float curve[4]);
// Read a just-started voice instance (DX10 0xc4d250 output): emitter position
// at +0x88, flags at +0xa0 (bit0 2D, bit2 curve valid), curve at +0x118,
// emitter id at +8; listener = first microphone (*micHeadSlot, position at +0,
// "no data" flag at +0x30).
VoiceLevel readVoiceLevel(uintptr_t instance, uintptr_t micHeadSlot, ReadAudioMemory read);

// Voice start of a leaf event (types 1, 5, 6, 7, 9), reached directly or as
// the child that a switch/multi event actually chose. The handle is the leaf
// event's table handle; leafEventId receives that event's full ID.
ResolvedAudio resolveLeafAudio(uintptr_t manager, uint32_t handle, uint32_t& leafEventId,
                               ReadAudioMemory read);
