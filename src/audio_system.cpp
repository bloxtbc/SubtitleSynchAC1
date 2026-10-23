#include "audio_system.h"
AudioQueue g_AudioQueue;

bool AudioQueue::push(const AudioEvent& event) {
    std::unique_lock<std::mutex> lock(m_mutex, std::try_to_lock);
    if (!lock.owns_lock() || m_count == Capacity) {
        m_dropped.fetch_add(1, std::memory_order_relaxed);
        return false;
    }
    m_buffer[m_tail] = event;
    m_tail = (m_tail + 1) % Capacity;
    ++m_count;
    return true;
}

bool AudioQueue::pop(AudioEvent& event) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_count) return false;
    event = m_buffer[m_head];
    m_head = (m_head + 1) % Capacity;
    --m_count;
    return true;
}

size_t AudioQueue::discardAll() {
    std::lock_guard<std::mutex> lock(m_mutex);
    const size_t discarded = m_count;
    m_head = m_tail = m_count = 0;
    return discarded;
}
