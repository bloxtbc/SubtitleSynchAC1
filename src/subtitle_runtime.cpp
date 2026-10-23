#include <algorithm>
#include <chrono>
#include <cmath>

#include "subtitle_runtime.h"

double applySubtitleTailExtension(double baseDurationSeconds, int extensionMs)
{
    const double base = std::isfinite(baseDurationSeconds) && baseDurationSeconds > 0.0
        ? baseDurationSeconds : 3.0;
    const int boundedExtension = std::clamp(extensionMs, 0, 10000);
    return base + static_cast<double>(boundedExtension) / 1000.0;
}

void SubtitleRuntime::start(
    const std::vector<SubtitleSegment>& segments,
    clock::duration fallbackDuration, clock::time_point now)
{
    m_segments = segments;
    m_index = 0;
    m_fallbackDuration = fallbackDuration;
    m_active = !m_segments.empty();

    if (!m_active)
        return;

    m_startTime = now;
    m_endTime = m_startTime + fallbackDuration;
    m_currentText = m_segments[0].text;

    const double wait = m_segments[0].waitAfter;
    m_nextSwitch = m_segments.size() == 1 ? m_endTime : now + (
        wait > 0.0
        ? std::chrono::duration_cast<clock::duration>(std::chrono::duration<double>(wait))
        : m_fallbackDuration
    );
}

bool SubtitleRuntime::advance(clock::time_point now)
{
    if (++m_index >= m_segments.size()) {
        reset();
        return false;
    }

    m_currentText = m_segments[m_index].text;

    const double wait = m_segments[m_index].waitAfter;

    m_nextSwitch = m_index + 1 == m_segments.size() ? m_endTime : now + (
        wait > 0.0
            ? std::chrono::duration_cast<clock::duration>(std::chrono::duration<double>(wait))
            : m_fallbackDuration
    );
    return true;
}

SubtitleUpdateResult SubtitleRuntime::update(clock::time_point now)
{
    SubtitleUpdateResult result;
    if (!m_active || m_segments.empty())
        return result;

    if (now >= m_endTime)
    {
        result.kind = SubtitleUpdateKind::DurationExpired;
        result.previousIndex = m_index;
        result.currentIndex = m_index;
        reset();
        return result;
    }

    result.previousIndex = m_index;
    while (m_active && now >= m_nextSwitch) {
        if (!advance(m_nextSwitch)) {
            result.kind = SubtitleUpdateKind::SegmentsExhausted;
            result.currentIndex = result.previousIndex;
            return result;
        }
        result.kind = SubtitleUpdateKind::SegmentChanged;
    }
    result.currentIndex = m_index;
    return result;
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
