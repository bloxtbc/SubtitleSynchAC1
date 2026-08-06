#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <cstddef>

class AudioQueue {
public:
    using VoicelineId = uint32_t;

    VoicelineId pop();

    void push(uintptr_t evt);
    void clear();

private:
    bool empty() const;
    bool full() const;

    static constexpr size_t Capacity = 1024;
    static_assert((Capacity & (Capacity - 1)) == 0, "AudioQueue capacity must be a power of two");

    std::array<VoicelineId, Capacity> m_buffer{};
    std::atomic<size_t> m_head{0};
    std::atomic<size_t> m_tail{0};
    VoicelineId m_last = 0;
};

extern AudioQueue g_AudioQueue;