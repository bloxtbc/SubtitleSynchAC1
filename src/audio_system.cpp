#include <cstdio>

#include "audio_system.h"

AudioQueue g_AudioQueue;

void AudioQueue::push(uintptr_t evt)
{
    m_queue.push(static_cast<uint32_t>(evt));
}


AudioQueue::VoicelineId AudioQueue::pop()
{
    if (m_queue.empty())
        return 0;

    uint32_t id = m_queue.front();
    m_queue.pop();

    return id;
}

void AudioQueue::clear()
{
    std::queue<VoicelineId> empty;
    std::swap(m_queue, empty);
    m_last = 0;
}

bool AudioQueue::empty() const
{
    return m_queue.empty();
}