#pragma once

#include <chrono>
#include <vector>
#include <string>

#include "subtitle_core.h"

class SubtitleRuntime {
public:
    using clock = std::chrono::steady_clock;

    SubtitleRuntime() = default;

    void start(const std::vector<SubtitleSegment>& segments,
               clock::duration fallbackDuration);

    void update(clock::time_point now = clock::now());

    void reset();

    const std::string& currentText() const;

    bool active() const;
private:
    void advance(clock::time_point now);

private:
    std::vector<SubtitleSegment> m_segments;

    size_t m_index = 0;

    std::string m_currentText;

    clock::time_point m_nextSwitch = clock::time_point::min();
    clock::time_point m_startTime = clock::time_point::min();
    clock::time_point m_endTime = clock::time_point::min();

    clock::duration m_fallbackDuration{ std::chrono::seconds(3) };

    bool m_active = false;
};