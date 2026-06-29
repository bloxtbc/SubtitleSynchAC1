#pragma once

#include <cstdint>
#include <array>
#include <queue>

class AudioQueue {
public:
    using VoicelineId = uint32_t;
    
    VoicelineId pop();

    void push(uintptr_t evt);
    void clear();

private:
    bool empty() const;
    bool full() const;
    
    VoicelineId extractVoiceline(uintptr_t evt) const;
    VoicelineId m_last = 0;

    std::queue<uint32_t> m_queue{};
};

extern AudioQueue g_AudioQueue;