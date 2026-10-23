#pragma once
#include <array>
#include <atomic>
#include <cstdint>
#include <mutex>
#include "audio_resolver.h"

struct AudioEvent {
    uint32_t id = 0;
    uint32_t threadId = 0;
    uint64_t timestampMs = 0;
    uint32_t rawId = 0;
    Resolution resolution = Resolution::Exact;
    // Leaf voice starts only: how loud the engine plays this voice for the
    // listener. levelKnown=false keeps the old always-show behaviour.
    bool levelKnown = false;
    bool positional = false;
    float distance = -1.0f;
    float levelDb = 0.0f;
    uint32_t emitter = 0xffffffffu;
    // The started voice instance; the render thread re-reads its distance
    // while the line plays. 0 = unknown.
    uintptr_t instance = 0;
};

// Multiple audio threads are allowed. Never block or allocate in a callback.
class AudioQueue {
public:
    static constexpr size_t Capacity = 1024;
    bool push(const AudioEvent& event);
    bool pop(AudioEvent& event);
    size_t discardAll();
    uint32_t takeDropped() { return m_dropped.exchange(0); }
private:
    std::mutex m_mutex;
    std::array<AudioEvent, Capacity> m_buffer{};
    size_t m_head = 0, m_tail = 0, m_count = 0;
    std::atomic<uint32_t> m_dropped{0};
};
extern AudioQueue g_AudioQueue;
