#pragma once
#include <chrono>
#include <atomic>
#include <mutex>

// One shared logical clock: pause time never advances subtitle deadlines.
class PlaybackClock {
public:
    using Clock = std::chrono::steady_clock;
    struct State { Clock::time_point now; bool paused; };
    bool setPaused(bool paused, Clock::time_point wall = Clock::now()) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (paused == m_paused) return false;
        if (paused) m_pauseStart = wall;
        else m_excluded += wall - m_pauseStart;
        m_paused = paused;
        m_pausedFast.store(paused, std::memory_order_release);
        return true;
    }
    State state(Clock::time_point wall = Clock::now()) {
        std::lock_guard<std::mutex> lock(m_mutex);
        return {(m_paused ? m_pauseStart : wall) - m_excluded, m_paused};
    }
    bool paused() const { return m_pausedFast.load(std::memory_order_acquire); }
private:
    std::mutex m_mutex;
    bool m_paused = false;
    std::atomic<bool> m_pausedFast{false};
    Clock::time_point m_pauseStart{};
    Clock::duration m_excluded{};
};
extern PlaybackClock g_playbackClock;
constexpr bool playbackSubtitleVisible(bool active, bool paused) { return active && !paused; }

enum class EscapePauseAction {
    None,
    Pause,
    Resume,
};

class EscapePauseTracker {
public:
    EscapePauseAction update(bool escapeDown, bool paused) {
        if (escapeDown == m_escapeDown)
            return EscapePauseAction::None;

        m_escapeDown = escapeDown;
        if (escapeDown) {
            if (paused) {
                // The menu starts closing on the second press, but gameplay
                // audio does not resume at the start of that frame. Keep the
                // subtitle frozen until the key is released.
                m_resumeOnRelease = true;
                return EscapePauseAction::None;
            }

            m_resumeOnRelease = false;
            return EscapePauseAction::Pause;
        }

        if (m_resumeOnRelease) {
            m_resumeOnRelease = false;
            return EscapePauseAction::Resume;
        }

        return EscapePauseAction::None;
    }

    bool closingOnRelease() const { return m_resumeOnRelease; }

private:
    bool m_escapeDown = false;
    bool m_resumeOnRelease = false;
};

// AC1 resumes dialogue after the menu-closing Escape release. Keep this delay
// on wall time because the subtitle playback clock itself is frozen.
class ResumeDelayGate {
public:
    using Clock = std::chrono::steady_clock;

    bool request(std::chrono::milliseconds delay, Clock::time_point now = Clock::now()) {
        if (delay.count() <= 0) {
            m_pending = false;
            return true;
        }
        m_deadline = now + delay;
        m_pending = true;
        return false;
    }

    bool update(Clock::time_point now = Clock::now()) {
        if (!m_pending || now < m_deadline)
            return false;
        m_pending = false;
        return true;
    }

    void cancel() { m_pending = false; }
    bool pending() const { return m_pending; }

private:
    bool m_pending = false;
    Clock::time_point m_deadline{};
};

// Input-independent: nested menus remain paused, and every way of closing the
// last menu uses the same wall-time gate. Reopening cancels a pending resume.
class MenuPauseTracker {
public:
    using Clock = ResumeDelayGate::Clock;
    EscapePauseAction update(bool menuPaused, std::chrono::milliseconds delay,
                            Clock::time_point now = Clock::now()) {
        if (menuPaused) {
            m_gate.cancel();
            const bool entered = !m_menuPaused;
            m_menuPaused = true;
            return entered ? EscapePauseAction::Pause : EscapePauseAction::None;
        }
        if (m_menuPaused) {
            m_menuPaused = false;
            if (m_gate.request(delay, now)) return EscapePauseAction::Resume;
        }
        return m_gate.update(now) ? EscapePauseAction::Resume : EscapePauseAction::None;
    }
    bool pending() const { return m_gate.pending(); }
private:
    bool m_menuPaused = false;
    ResumeDelayGate m_gate;
};
