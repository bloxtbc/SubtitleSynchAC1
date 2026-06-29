#include <chrono>

#include "subtitle_runtime.h"

void SubtitleRuntime::start(
    const std::vector<SubtitleSegment>& segments,
    clock::duration fallbackDuration)
{
    m_segments = segments;
    m_index = 0;
    m_fallbackDuration = fallbackDuration;
    m_active = !m_segments.empty();

    if (!m_active)
        return;

    m_currentText = m_segments[0].text;

    auto now = clock::now();

    double wait = m_segments[0].waitAfter;
    
    m_nextSwitch = now + (
        wait > 0.0
        ? std::chrono::duration_cast<clock::duration>(std::chrono::duration<double>(wait))
        : m_fallbackDuration
    );

    m_startTime = clock::now();
    m_endTime = m_startTime + fallbackDuration;
}

void SubtitleRuntime::advance(clock::time_point now)
{
    if (++m_index >= m_segments.size()) {
        reset();
        return;
    }

    m_currentText = m_segments[m_index].text;

    double wait = m_segments[m_index].waitAfter;

    m_nextSwitch = now + (
        wait > 0.0
            ? std::chrono::duration_cast<clock::duration>(std::chrono::duration<double>(wait))
            : m_fallbackDuration
    );
}

void SubtitleRuntime::update(clock::time_point now)
{
    if (!m_active || m_segments.empty())
        return;

    if (now < m_nextSwitch)
        return;

    if (now >= m_endTime)
    {
        reset();
        return;
    }

    advance(now);
}

void SubtitleRuntime::reset()
{
    m_nextSwitch = clock::time_point::min();
    m_startTime = clock::time_point::min();
    m_endTime = clock::time_point::min();
    m_currentText.clear();
    m_segments.clear();
    m_index = 0;

    m_active = false;
}

const std::string& SubtitleRuntime::currentText() const
{
    return m_currentText;
}

bool SubtitleRuntime::active() const
{
    return m_active;
}