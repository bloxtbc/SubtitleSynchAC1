#pragma once

#include <cstdint>
#include <deque>
#include <vector>
#include <Windows.h>

#include "subtitle_runtime.h"
#include "audio_system.h"
#include "overlay_ui.h"
#include "ini_config.h"
#include "playback_clock.h"
#include "pause_manager_probe.h"
#include "save_activity_monitor.h"
#include "ambient_voices.h"

// Diagnostics only: follows a started voice instance to measure its real length.
struct VoiceProbe {
    uint32_t id = 0;
    uint32_t rawId = 0;
    uintptr_t instance = 0;
    uint32_t emitter = 0xffffffffu;
    uint64_t startMs = 0;
    double dbSeconds = 0.0;
    SubtitlePriority priority = SubtitlePriority::None;
    bool dbReached = false;
    bool pausedDuring = false;
};

class ModRuntime {
public:
    void init();
    void update();
    void setPressPromptActive(bool active);

    void onPause(bool paused);
    void onFocus(bool focused);

    bool handleVoiceline(const AudioEvent& event);
    // Ambient voices: tracked while they play, shown only within range.
    void addAmbient(const AudioEvent& event, SubtitleRuntime::clock::time_point now);
    void updateAmbient(const PlaybackClock::State& playback);
    // Voice-length probes: only while a capture is recording.
    void addVoiceProbe(const AudioEvent& event, SubtitlePriority priority);
    void updateVoiceProbes(bool paused);
    bool startSubtitle(uint32_t id, const SubtitleMatch& match, const std::vector<SubtitleSegment>& segments,
                       SubtitleRuntime::clock::time_point start, uint64_t eventMs,
                       bool levelKnown, float levelDb, const AmbientVoice* ambient);
    void logActiveEnd(const char* reason, uint32_t replacementId, uint64_t displayNowMs,
                      SubtitleRuntime::clock::time_point now);
    void endActiveSubtitle(const char* reason, SubtitleRuntime::clock::time_point now);

    AudioQueue m_audio;
    SubtitleEngine m_engine;
    SubtitleOverlay m_overlay;
    SubtitleRuntime m_runtime;
    HWND m_window;

    bool m_paused = false;
    bool m_audioHookReady = false;
    std::string m_lastDiagnosticText;
    bool m_lastDiagnosticVisible = false;
    bool m_lastDiagnosticPaused = false;
    EscapePauseTracker m_escapePause;
    ResumeDelayGate m_resumeGate;
    MenuPauseTracker m_menuPause;
    PauseManagerProbe m_pauseManagerProbe;
    SaveActivityMonitor m_saveActivityMonitor;
    bool m_escapeSampled = false;
    bool m_lastEscapeDown = false;
    bool m_f2Sampled = false;
    bool m_lastF2Down = false;
    ULONGLONG m_lastEscapeReleaseMs = 0;
    bool m_pauseManagerSampled = false;
    bool m_lastPauseManagerPaused = false;
    uint32_t m_lastPauseManagerActiveCount = 0;
    uint32_t m_lastPauseManagerCommittedCount = 0;
    uint32_t m_activeSubtitleId = 0;
    SubtitlePriority m_activeSubtitlePriority = SubtitlePriority::None;
    SubtitleRuntime::clock::time_point m_activeSubtitleStart{};
    uint64_t m_activeSubtitleDisplayStartMs = 0;
    double m_activeSubtitleBaseDuration = 0.0;
    double m_activeSubtitleDuration = 0.0;
    std::vector<SubtitleSegment> m_activeSubtitleSegments;
    bool m_lastDiagnosticDebugWindow = false;
    bool m_lastDiagnosticDebugPreview = false;
    bool m_lastDiagnosticSaveActivity = false;
    bool m_lastDiagnosticsEnabled = false;
    uint64_t m_lastDiagnosticsSession = 0;
    bool m_pressPromptActive = false;
    uint32_t m_captureMarkerSequence = 0;
    std::deque<AudioEvent> m_pausedDialogueEvents;
    // The root hook reports a direct story line first; its voice start repeats it.
    uint32_t m_lastRootPrimaryId = 0;
    uint64_t m_lastRootPrimaryMs = 0;
    bool m_activeLevelKnown = false;
    float m_activeLevelDb = 0.0f;
    AmbientVoices m_ambient;
    // The active subtitle belongs to this tracked ambient voice.
    bool m_activeIsAmbient = false;
    uintptr_t m_activeAmbientInstance = 0;
    static constexpr size_t MaxVoiceProbes = 32;
    std::deque<VoiceProbe> m_voiceProbes;
};
