#include <cstdio>

#include "audio_system.h"

AudioQueue g_AudioQueue;

void AudioQueue::push(uintptr_t evt)
{
    const VoicelineId value = static_cast<VoicelineId>(evt);
    const size_t tail = m_tail.load(std::memory_order_relaxed);
    const size_t nextTail = (tail + 1) & (Capacity - 1);

    if (nextTail == m_head.load(std::memory_order_acquire))
        return; // queue is full, drop event

    m_buffer[tail] = value;
    m_tail.store(nextTail, std::memory_order_release);
}


AudioQueue::VoicelineId AudioQueue::pop()
{
    const size_t head = m_head.load(std::memory_order_relaxed);
    if (head == m_tail.load(std::memory_order_acquire))
        return 0;

    const VoicelineId id = m_buffer[head];
    m_head.store((head + 1) & (Capacity - 1), std::memory_order_release);

    return id;
}

void AudioQueue::clear()
{
    m_head.store(0, std::memory_order_release);
    m_tail.store(0, std::memory_order_release);
    m_last = 0;
}

bool AudioQueue::empty() const
{
    return m_head.load(std::memory_order_acquire) == m_tail.load(std::memory_order_acquire);
}

bool AudioQueue::full() const
{
    return ((m_tail.load(std::memory_order_acquire) + 1) & (Capacity - 1)) == m_head.load(std::memory_order_acquire);
}