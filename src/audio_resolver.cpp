#include "audio_resolver.h"
#include <algorithm>
#include <cmath>

ResolvedAudio resolveAudio(uint32_t eventId, uintptr_t manager, uint32_t handle,
                           ReadAudioMemory read) {
    if ((eventId & 0xf0000000) == 0x20000000)
        return {eventId, Resolution::Exact};
    if ((eventId & 0xf0000000) != 0x10000000 || !manager || !read)
        return {};
    uint32_t header[3]{};
    if (!read(manager, header, sizeof header)) return {};
    const uint32_t capacity=header[0], count=header[1], table=header[2];
    if (!table || !count || count > capacity || capacity > 1000000 || handle >= count)
        return {};
    // Validate every copied record. The callback does not retain game pointers.
    uint32_t root[4]{}, action[3]{}, audio[4]{};
    const auto entry = [table](uint32_t index) -> uintptr_t {
        const uint64_t address = uint64_t(table) + uint64_t(index) * 16;
        return address <= UINT32_MAX - 16 ? static_cast<uintptr_t>(address) : 0;
    };
    const auto rootAddress = entry(handle);
    if (!rootAddress || !read(rootAddress, root, sizeof root) || root[0] != eventId || !root[1])
        return {};
    if (!read(root[1], action, sizeof action) || action[0] != (eventId & 0x0fffffff))
        return {};
    // Only the direct-play event layout has been verified. Never choose a
    // random/conditional/sequence branch by walking all possible children.
    if (action[1] != 1 || !action[2] || action[2] >= count) return {};
    const auto audioAddress = entry(action[2]);
    if (!audioAddress || !read(audioAddress, audio, sizeof audio) ||
        (audio[0] & 0xf0000000) != 0x20000000) return {};
    uint32_t again[3]{}, rootAgain[4]{};
    if (!read(manager, again, sizeof again) || again[2] != table || again[1] < count ||
        !read(rootAddress, rootAgain, sizeof rootAgain) ||
        rootAgain[0] != root[0] || rootAgain[1] != root[1]) return {};
    return {audio[0], Resolution::DirectReference};
}

ResolvedAudio resolveLeafAudio(uintptr_t manager, uint32_t handle, uint32_t& leafEventId,
                               ReadAudioMemory read) {
    leafEventId = 0;
    if (!manager || !read) return {};
    uint32_t header[3]{};
    if (!read(manager, header, sizeof header)) return {};
    const uint32_t capacity=header[0], count=header[1], table=header[2];
    if (!table || !count || count > capacity || capacity > 1000000 || handle >= count)
        return {};
    uint32_t root[4]{}, action[3]{}, audio[4]{};
    const auto entry = [table](uint32_t index) -> uintptr_t {
        const uint64_t address = uint64_t(table) + uint64_t(index) * 16;
        return address <= UINT32_MAX - 16 ? static_cast<uintptr_t>(address) : 0;
    };
    const auto rootAddress = entry(handle);
    if (!rootAddress || !read(rootAddress, root, sizeof root) ||
        (root[0] & 0xf0000000) != 0x10000000 || !root[1]) return {};
    if (!read(root[1], action, sizeof action) || action[0] != (root[0] & 0x0fffffff))
        return {};
    // Leaf types share the resource-handle slot at object+8 (DX10 0xc4a7e0).
    switch (action[1]) {
    case 1: case 5: case 6: case 7: case 9: break;
    default: return {};
    }
    if (!action[2] || action[2] >= count) return {};
    const auto audioAddress = entry(action[2]);
    if (!audioAddress || !read(audioAddress, audio, sizeof audio) ||
        (audio[0] & 0xf0000000) != 0x20000000) return {};
    leafEventId = root[0];
    return {audio[0], Resolution::LeafVoice};
}

float voiceCurveDb(float distance, const float curve[4]) {
    static const float defaults[4] = {0.5f, -1.0f, -1.0f, -1.0f};
    float f[4];
    for (int i = 0; i < 4; ++i) f[i] = curve[i] == -2.0f ? defaults[i] : curve[i];
    if (!std::isfinite(distance) || distance < 0) return 0.0f;
    const float x = f[1] != -1.0f && distance > f[1] ? f[1] : distance;
    if (x < f[0]) return 0.0f;
    if (f[0] <= 0) return -96.0f;
    if (f[2] == -1.0f || f[3] == -1.0f || x < f[2])
        return (std::max)(-96.0f, 20.0f * std::log10(f[0] / x));
    if (x >= f[3] || f[3] <= f[2]) return -96.0f;
    const float amplitude = f[0] / f[2] * (1.0f - (x - f[2]) / (f[3] - f[2]));
    return amplitude > 0 ? (std::max)(-96.0f, 20.0f * std::log10(amplitude)) : -96.0f;
}

VoiceLevel readVoiceLevel(uintptr_t instance, uintptr_t micHeadSlot, ReadAudioMemory read) {
    VoiceLevel level;
    uint32_t emitter = 0, flags = 0;
    float position[3]{}, curve[4]{};
    if (!instance || !read || !read(instance + 8, &emitter, sizeof emitter) ||
        !read(instance + 0xa0, &flags, sizeof flags)) return level;
    level.emitter = emitter;
    if (flags & 1) {  // 2D voice: no emitter, always full level
        level.known = true;
        return level;
    }
    uint32_t mic = 0, micNoData = 1;
    float listener[3]{};
    if (!micHeadSlot || !read(micHeadSlot, &mic, sizeof mic) || !mic ||
        !read(mic + 0x30, &micNoData, sizeof micNoData) || micNoData ||
        !read(mic, listener, sizeof listener) ||
        !read(instance + 0x88, position, sizeof position)) return level;
    if (flags & 4) {
        if (!read(instance + 0x118, curve, sizeof curve)) return level;
    } else {
        for (auto& c : curve) c = -2.0f;
    }
    const float dx = position[0] - listener[0], dy = position[1] - listener[1],
        dz = position[2] - listener[2];
    const float distance = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (!std::isfinite(distance)) return level;
    level.known = true;
    level.positional = true;
    level.distance = distance;
    level.levelDb = voiceCurveDb(distance, curve);
    return level;
}
