#include <xmmintrin.h>
#include <algorithm>
#include <filesystem>

#include "mod_runtime.h"
#include "hooks/asm_hooks.h"
#include "hooks/press_prompt_hook.h"
#include "globals.h"
#include "diagnostics.h"
#include "playback_clock.h"

namespace {
const char* priorityName(SubtitlePriority priority) {
    switch (priority) {
    case SubtitlePriority::Primary: return "primary";
    case SubtitlePriority::Secondary: return "secondary";
    default: return "missing";
    }
}
}

void ModRuntime::init(void) {
    std::string baseDir = std::filesystem::path(dllPath).parent_path().string();
    std::string jsonPath = baseDir + "\\subtitles.json";
    std::string secondaryJsonPath = baseDir + "\\subtitles_secondary.json";

    Diagnostics::init(baseDir);
    const bool pauseManagerReady = m_pauseManagerProbe.initialize();
    PressPromptHook::initialize();
    if (!m_engine.load(jsonPath)) {
        Diagnostics::error("subtitle database could not be loaded");
    } else {
        if (std::filesystem::exists(secondaryJsonPath)) {
            if (!m_engine.loadSecondary(secondaryJsonPath))
                Diagnostics::error("secondary subtitle database could not be loaded");
            else
                Diagnostics::log("secondary_database loaded path=%s", secondaryJsonPath.c_str());
        } else {
            Diagnostics::log("secondary_database absent path=%s", secondaryJsonPath.c_str());
        }
        m_audioHookReady = applyASMPatches();
        Diagnostics::log("pause_tracking source=menu-manager-state manager_ready=%u",
            pauseManagerReady);
    }
    m_overlay.init();
    const bool saveMonitorReady = m_saveActivityMonitor.initialize();
    Diagnostics::log("save_activity_monitor ready=%u directory=%ls",
        saveMonitorReady, saveMonitorReady ? m_saveActivityMonitor.directory().c_str() : L"");
}

void ModRuntime::setPressPromptActive(bool active) {
    if (active != m_pressPromptActive) {
        Diagnostics::log("press_prompt active=%u", active);
        m_pressPromptActive = active;
    }
    m_overlay.setPressPromptActive(active);
}

void ModRuntime::update(void) {
    //some float bullshit for msvc :(
    unsigned int mxcsr_saved = _mm_getcsr();
    unsigned int cw_saved = 0;
    _controlfp_s(&cw_saved, 0, 0);

    _mm_setcsr((mxcsr_saved & ~_MM_ROUND_MASK) | _MM_ROUND_NEAREST);
    _controlfp_s(nullptr, _RC_NEAR, _MCW_RC);

    const auto dropped = g_AudioQueue.takeDropped();
    if (dropped) {
        Diagnostics::log("queue_dropped count=%u", dropped);
        Diagnostics::captureQueueDrops(GetTickCount64(), dropped);
    }

    const bool gameHasFocus = GetForegroundWindow() == m_window;
    const bool f2Down = (GetAsyncKeyState(VK_F2) & 0x8000) != 0;
    const bool f2Pressed = m_f2Sampled && !m_lastF2Down && f2Down;
    m_f2Sampled = true;
    m_lastF2Down = f2Down;
    EscapePauseAction pauseAction = EscapePauseAction::None;
    bool escapeDown = m_lastEscapeDown;
    if (gameHasFocus) {
        escapeDown = (GetAsyncKeyState(VK_ESCAPE) & 0x8000) != 0;
        if (!m_escapeSampled) {
            m_escapeSampled = true;
            m_lastEscapeDown = escapeDown;
        } else if (escapeDown != m_lastEscapeDown) {
            m_lastEscapeDown = escapeDown;
            const auto edgeTime = GetTickCount64();
            Diagnostics::log("escape_edge down=%u", escapeDown);
            if (!escapeDown)
                m_lastEscapeReleaseMs = edgeTime;
        }
        if (Diagnostics::captureEnabled() && f2Pressed) {
            ++m_captureMarkerSequence;
            double elapsed = 0.0;
            if (m_runtime.active())
                elapsed = std::chrono::duration<double>(
                    g_playbackClock.state().now - m_activeSubtitleStart).count();
            Diagnostics::captureMarker(GetTickCount64(), m_captureMarkerSequence, m_activeSubtitleId,
                priorityName(m_activeSubtitlePriority), elapsed, m_runtime.currentText());
            m_overlay.showCaptureMarker(m_captureMarkerSequence);
            Diagnostics::log("capture_marker key=F2 marker=%lu active_id=0x%08lx elapsed=%.3f",
                static_cast<unsigned long>(m_captureMarkerSequence),
                static_cast<unsigned long>(m_activeSubtitleId), elapsed);
        }
    }
    auto setPlaybackPaused = [](bool paused, const char* source) {
        if (!g_playbackClock.setPaused(paused))
            return;
        Diagnostics::log("playback_pause paused=%u source=%s", paused, source);
    };

    PauseManagerSnapshot pauseManager;
    if (m_pauseManagerProbe.sample(pauseManager)) {
        const bool managerPaused = pauseManager.menuOpen;
        const bool changed = !m_pauseManagerSampled ||
            managerPaused != m_lastPauseManagerPaused ||
            pauseManager.activeCount != m_lastPauseManagerActiveCount ||
            pauseManager.committedCount != m_lastPauseManagerCommittedCount;
        if (changed) {
            Diagnostics::log("pause_manager_state object=%p active_count=%lu committed_count=%lu counts_available=%u menu_object=%p menu_open=%u paused=%u",
                reinterpret_cast<void*>(pauseManager.object),
                static_cast<unsigned long>(pauseManager.activeCount),
                static_cast<unsigned long>(pauseManager.committedCount), pauseManager.countsAvailable,
                reinterpret_cast<void*>(pauseManager.menuObject), pauseManager.menuOpen, managerPaused);
        }
        if (m_pauseManagerSampled && m_lastPauseManagerPaused && !managerPaused) {
            const auto now = GetTickCount64();
            if (m_lastEscapeReleaseMs && now >= m_lastEscapeReleaseMs) {
                Diagnostics::log("pause_manager_resume escape_release_delay_ms=%llu",
                    static_cast<unsigned long long>(now - m_lastEscapeReleaseMs));
            } else {
                Diagnostics::log("pause_manager_resume escape_release_delay_ms=unavailable");
            }
        }
        const bool pendingBefore = m_menuPause.pending();
        m_resumeGate.cancel();
        if (!m_pauseManagerSampled && g_playbackClock.paused())
            m_menuPause.update(true, std::chrono::milliseconds(g_subtitleSettings.resumeDelayMs));
        const auto action = m_menuPause.update(managerPaused,
            std::chrono::milliseconds(g_subtitleSettings.resumeDelayMs));
        if (action == EscapePauseAction::Pause)
            setPlaybackPaused(true, "menu-manager-enter");
        else if (action == EscapePauseAction::Resume)
            setPlaybackPaused(false, "menu-manager-resume-delay");
        if (!pendingBefore && m_menuPause.pending())
            Diagnostics::log("playback_resume_pending source=menu-manager-exit delay_ms=%d",
                g_subtitleSettings.resumeDelayMs);
        m_pauseManagerSampled = true;
        m_lastPauseManagerPaused = managerPaused;
        m_lastPauseManagerActiveCount = pauseManager.activeCount;
        m_lastPauseManagerCommittedCount = pauseManager.committedCount;
    } else if (!m_pauseManagerSampled && gameHasFocus) {
        // Input edges are a fallback only. They must never release a nested
        // menu, or bypass the delay used by mouse/controller Resume.
        pauseAction = m_escapePause.update(escapeDown, g_playbackClock.paused());
    }

    bool resumeNow = false;
    if (pauseAction == EscapePauseAction::Pause) {
        m_resumeGate.cancel();
        setPlaybackPaused(true, "escape-press");
    } else if (pauseAction == EscapePauseAction::Resume) {
        const auto delay = std::chrono::milliseconds(g_subtitleSettings.resumeDelayMs);
        resumeNow = m_resumeGate.request(delay);
        if (!resumeNow) {
            Diagnostics::log("playback_resume_pending source=configured-delay delay_ms=%d",
                g_subtitleSettings.resumeDelayMs);
        }
    }

    if (m_resumeGate.update()) {
        resumeNow = true;
    }
    if (resumeNow)
        setPlaybackPaused(false, "configured-resume-delay");

    setPressPromptActive(PressPromptHook::visible(GetTickCount64()));
    const auto diagnosticsSession = Diagnostics::recordingSession();
    const bool recordingStarted = Diagnostics::enabled() &&
        (!m_lastDiagnosticsEnabled || diagnosticsSession != m_lastDiagnosticsSession);
    if (recordingStarted) {
        Diagnostics::log("runtime_snapshot pause_manager_slot=%p menu_manager_slot=%p prompt_hook=%p audio_hook_ready=%u paused=%u resume_delay_ms=%d",
            reinterpret_cast<void*>(m_pauseManagerProbe.pointerSlot()),
            reinterpret_cast<void*>(m_pauseManagerProbe.menuPointerSlot()),
            reinterpret_cast<void*>(PressPromptHook::address()), m_audioHookReady, g_playbackClock.paused(),
            g_subtitleSettings.resumeDelayMs);
    }
    m_lastDiagnosticsEnabled = Diagnostics::enabled();
    m_lastDiagnosticsSession = diagnosticsSession;

    const bool saveActivity = m_saveActivityMonitor.update(
        g_subtitleSettings.saveIndicatorDurationMs);
    m_overlay.setSaveIndicatorActive(saveActivity);
    if (Diagnostics::enabled() && saveActivity != m_lastDiagnosticSaveActivity) {
        Diagnostics::log("save_activity active=%u lift=%.1f duration_ms=%d",
            saveActivity, g_subtitleSettings.saveIndicatorLift,
            g_subtitleSettings.saveIndicatorDurationMs);
        m_lastDiagnosticSaveActivity = saveActivity;
    }

    AudioEvent event;
    const auto queuePlayback = g_playbackClock.state();
    if (!queuePlayback.paused) {
        while (!m_pausedDialogueEvents.empty()) {
            event = m_pausedDialogueEvents.front();
            m_pausedDialogueEvents.pop_front();
            Diagnostics::log("paused_dialogue action=replay id=0x%08lx event_t=%llu remaining=%zu",
                static_cast<unsigned long>(event.id),
                static_cast<unsigned long long>(event.timestampMs), m_pausedDialogueEvents.size());
            handleVoiceline(event);
        }
    }
    // Bound render-thread work even if producers keep adding events.
    for (size_t n = 0; n < AudioQueue::Capacity && g_AudioQueue.pop(event); ++n)
    {
        const auto eventPriority = m_engine.getPriority(event.id);
        Diagnostics::captureAudio(event.timestampMs, event.id, event.rawId, event.threadId,
            static_cast<uint32_t>(event.resolution), priorityName(eventPriority));
        Diagnostics::log("audio event_t=%llu thread=%lu id=0x%08lx raw=0x%08lx resolution=%lu",
            static_cast<unsigned long long>(event.timestampMs),
            static_cast<unsigned long>(event.threadId), static_cast<unsigned long>(event.id),
            static_cast<unsigned long>(event.rawId), static_cast<unsigned long>(event.resolution));
        if (event.resolution == Resolution::Unresolved) {
            Diagnostics::captureDecision(GetTickCount64(), event.id, event.rawId,
                "unresolved-resource", m_activeSubtitleId);
            continue;
        }
        if (event.resolution == Resolution::LeafVoice && Diagnostics::enabled())
            Diagnostics::log("voice_level id=0x%08lx raw=0x%08lx known=%d positional=%d distance=%.1f level_db=%.1f emitter=0x%08lx",
                static_cast<unsigned long>(event.id), static_cast<unsigned long>(event.rawId),
                event.levelKnown, event.positional, event.distance, event.levelDb,
                static_cast<unsigned long>(event.emitter));
        if (event.resolution == Resolution::LeafVoice && eventPriority != SubtitlePriority::None)
            addVoiceProbe(event, eventPriority);
        // With the voice-start hook, ambient lines come from it: only it knows
        // how loud the speaker is. Story lines keep the earlier root report.
        if (event.resolution != Resolution::LeafVoice) {
            if (eventPriority == SubtitlePriority::Secondary && AudioHook::leafActive()) {
                Diagnostics::captureDecision(GetTickCount64(), event.id, event.rawId,
                    "secondary-awaits-voice-start", m_activeSubtitleId);
                continue;
            }
            if (eventPriority == SubtitlePriority::Primary) {
                m_lastRootPrimaryId = event.id;
                m_lastRootPrimaryMs = event.timestampMs;
            }
        } else if (eventPriority == SubtitlePriority::Primary) {
            if (event.id == m_lastRootPrimaryId && event.timestampMs >= m_lastRootPrimaryMs &&
                event.timestampMs - m_lastRootPrimaryMs <= 1000) {
                Diagnostics::log("voice_start_delay id=0x%08lx root_to_voice_ms=%llu",
                    static_cast<unsigned long>(event.id),
                    static_cast<unsigned long long>(event.timestampMs - m_lastRootPrimaryMs));
                Diagnostics::captureDecision(GetTickCount64(), event.id, event.rawId,
                    "voice-start-duplicate", m_activeSubtitleId);
                continue;
            }
        } else if (eventPriority == SubtitlePriority::Secondary) {
            // Follow the ambient voice while it plays, also during a pause;
            // updateAmbient shows it only while the speaker is in range.
            addAmbient(event, queuePlayback.now);
            continue;
        }
        if (queuePlayback.paused) {
            if (eventPriority == SubtitlePriority::None) {
                Diagnostics::captureDecision(GetTickCount64(), event.id, event.rawId,
                    "paused-unmapped", m_activeSubtitleId);
                Diagnostics::log("paused_dialogue action=discard-unmapped id=0x%08lx",
                    static_cast<unsigned long>(event.id));
                continue;
            }
            if (event.id == m_activeSubtitleId) {
                Diagnostics::captureDecision(GetTickCount64(), event.id, event.rawId,
                    "paused-active-duplicate", m_activeSubtitleId);
                Diagnostics::log("paused_dialogue action=discard-active-duplicate id=0x%08lx",
                    static_cast<unsigned long>(event.id));
                continue;
            }
            if (!m_pausedDialogueEvents.empty() &&
                m_pausedDialogueEvents.back().id == event.id &&
                event.timestampMs >= m_pausedDialogueEvents.back().timestampMs &&
                event.timestampMs - m_pausedDialogueEvents.back().timestampMs <= 250) {
                Diagnostics::captureDecision(GetTickCount64(), event.id, event.rawId,
                    "paused-near-duplicate", m_activeSubtitleId);
                Diagnostics::log("paused_dialogue action=discard-near-duplicate id=0x%08lx",
                    static_cast<unsigned long>(event.id));
                continue;
            }
            if (m_pausedDialogueEvents.size() >= 32) {
                Diagnostics::captureDecision(GetTickCount64(), event.id, event.rawId,
                    "paused-buffer-full", m_activeSubtitleId);
                Diagnostics::log("paused_dialogue action=discard-capacity id=0x%08lx",
                    static_cast<unsigned long>(event.id));
                continue;
            }
            m_pausedDialogueEvents.push_back(event);
            Diagnostics::captureDecision(GetTickCount64(), event.id, event.rawId,
                "buffered-until-resume", m_activeSubtitleId);
            Diagnostics::log("paused_dialogue action=buffer id=0x%08lx pending=%zu",
                static_cast<unsigned long>(event.id), m_pausedDialogueEvents.size());
            continue;
        }
        handleVoiceline(event);
    }

    const auto playback = g_playbackClock.state();
    updateVoiceProbes(playback.paused);
    updateAmbient(playback);
    const auto runtimeUpdate = m_runtime.update(playback.now);
    if (runtimeUpdate.kind == SubtitleUpdateKind::SegmentChanged) {
        const auto elapsed = std::chrono::duration<double>(playback.now - m_activeSubtitleStart).count();
        const auto captureNow = GetTickCount64();
        for (size_t index = runtimeUpdate.previousIndex + 1;
             index < runtimeUpdate.currentIndex && index < m_activeSubtitleSegments.size(); ++index)
            Diagnostics::captureSegment("subtitle_segment_skipped", captureNow, m_activeSubtitleId,
                index, elapsed, m_activeSubtitleSegments[index].text);
        if (runtimeUpdate.previousIndex < m_activeSubtitleSegments.size())
            Diagnostics::captureSegment("subtitle_segment_end", captureNow, m_activeSubtitleId,
                runtimeUpdate.previousIndex, elapsed,
                m_activeSubtitleSegments[runtimeUpdate.previousIndex].text);
        if (runtimeUpdate.currentIndex < m_activeSubtitleSegments.size())
            Diagnostics::captureSegment("subtitle_segment_start", captureNow, m_activeSubtitleId,
                runtimeUpdate.currentIndex, elapsed,
                m_activeSubtitleSegments[runtimeUpdate.currentIndex].text);
        Diagnostics::log("subtitle_segment id=0x%08lx from=%zu to=%zu count=%zu elapsed=%.3f remaining=%.3f",
            static_cast<unsigned long>(m_activeSubtitleId), runtimeUpdate.previousIndex,
            runtimeUpdate.currentIndex, m_runtime.segmentCount(), elapsed,
            (std::max)(0.0, m_activeSubtitleDuration - elapsed));
    } else if (runtimeUpdate.kind == SubtitleUpdateKind::DurationExpired ||
               runtimeUpdate.kind == SubtitleUpdateKind::SegmentsExhausted) {
        const auto elapsed = std::chrono::duration<double>(playback.now - m_activeSubtitleStart).count();
        const auto captureNow = GetTickCount64();
        if (runtimeUpdate.previousIndex < m_activeSubtitleSegments.size())
            Diagnostics::captureSegment("subtitle_segment_end", captureNow, m_activeSubtitleId,
                runtimeUpdate.previousIndex, elapsed,
                m_activeSubtitleSegments[runtimeUpdate.previousIndex].text);
        Diagnostics::captureSubtitleEnd(m_activeSubtitleDisplayStartMs, captureNow,
            m_activeSubtitleId, priorityName(m_activeSubtitlePriority), elapsed,
            m_activeSubtitleBaseDuration, m_activeSubtitleDuration,
            runtimeUpdate.kind == SubtitleUpdateKind::DurationExpired
                ? "duration-expired" : "segments-exhausted", 0);
        Diagnostics::log("subtitle_end id=0x%08lx reason=%s segment=%zu elapsed=%.3f configured_duration=%.3f",
            static_cast<unsigned long>(m_activeSubtitleId),
            runtimeUpdate.kind == SubtitleUpdateKind::DurationExpired ? "duration-expired" : "segments-exhausted",
            runtimeUpdate.previousIndex, elapsed, m_activeSubtitleDuration);
        m_activeSubtitleId = 0;
        m_activeSubtitlePriority = SubtitlePriority::None;
        m_activeSubtitleDisplayStartMs = 0;
        m_activeSubtitleBaseDuration = 0.0;
        m_activeSubtitleDuration = 0.0;
        m_activeSubtitleSegments.clear();
        m_activeIsAmbient = false;
        m_activeAmbientInstance = 0;
    }

    // Keep the current subtitle in the frozen runtime, but do not draw it over
    // the game's pause menu. Resuming reveals the same segment again.
    bool visible = playbackSubtitleVisible(m_runtime.active(), playback.paused);
    std::string text = m_runtime.currentText();
    const bool debugWindow = m_overlay.isDebugWindowVisible();
    // The F1 window always shows its preview text in place of the subtitle.
    const bool debugPreview = debugWindow;
    if (debugWindow && debugPreview) {
        visible = !playback.paused;
        text = m_overlay.debugText();
    }
    if (!g_subtitleSettings.showSubtitles)
        visible = false;
    if (!g_subtitleSettings.showSoundCaptions) {
        // A segment that is only "[laughs]" keeps its time but shows nothing.
        text = removeSoundCaptions(text);
        if (text.empty()) visible = false;
    }

    if (Diagnostics::enabled() &&
        (debugWindow != m_lastDiagnosticDebugWindow || debugPreview != m_lastDiagnosticDebugPreview)) {
        Diagnostics::log("debug_ui window=%d preview=%d subtitle_active=%d paused=%d",
            debugWindow, debugPreview, m_runtime.active(), playback.paused);
        m_lastDiagnosticDebugWindow = debugWindow;
        m_lastDiagnosticDebugPreview = debugPreview;
    }

    m_overlay.setVisible(visible);
    m_overlay.setSecondary(!(debugWindow && debugPreview) &&
        m_activeSubtitlePriority == SubtitlePriority::Secondary);
    m_overlay.setText(text);

    if (recordingStarted || visible != m_lastDiagnosticVisible || text != m_lastDiagnosticText) {
        const char* reason = visible ? (debugWindow && debugPreview ? "debug-preview" : "game-subtitle")
            : playback.paused ? "pause-menu"
            : !m_runtime.active() ? "subtitle-inactive"
            : "empty-or-disabled";
        Diagnostics::captureDisplay(GetTickCount64(), visible, m_activeSubtitleId,
            playback.paused, text);
        if (Diagnostics::enabled())
            Diagnostics::log("display visible=%d bytes=%zu debug_window=%d debug_preview=%d paused=%d reason=%s",
                visible, text.size(), debugWindow, debugPreview, playback.paused, reason);
        m_lastDiagnosticVisible = visible;
        m_lastDiagnosticText = text;
    }
    if (Diagnostics::enabled() && playback.paused != m_lastDiagnosticPaused) {
        Diagnostics::log("playback_state paused=%d subtitle_active=%d text_bytes=%zu",
            playback.paused, m_runtime.active(), text.size());
        m_lastDiagnosticPaused = playback.paused;
    }

    m_overlay.render(m_window);
    _mm_setcsr(mxcsr_saved);
    _controlfp_s(nullptr, cw_saved, _MCW_RC);
}


bool ModRuntime::handleVoiceline(const AudioEvent& event)
{
    const uint32_t id = event.id;
    const auto match = m_engine.getMatch(id);
    Diagnostics::log("lookup id=0x%08lx key=0x%08lx hit=%d priority=%s",
        static_cast<unsigned long>(id), static_cast<unsigned long>(id),
        static_cast<bool>(match), priorityName(match.priority));
    if (!match) {
        Diagnostics::captureDecision(GetTickCount64(), id, event.rawId,
            "missing-subtitle", m_activeSubtitleId);
        return false;
    }
    auto segments = SubtitleEngine::parseSegments(SubtitleEngine::stripTags(match.raw));
    if (segments.empty()) {
        Diagnostics::captureDecision(GetTickCount64(), id, event.rawId,
            "empty-subtitle", m_activeSubtitleId);
        return false;
    }
    const auto now = g_playbackClock.state().now;
    const auto displayNowMs = GetTickCount64();
    if (m_runtime.active() &&
        !shouldDisplaySubtitle(m_activeSubtitlePriority, match.priority)) {
        Diagnostics::log("subtitle_suppressed id=0x%08lx priority=%s active_id=0x%08lx active_priority=%s",
            static_cast<unsigned long>(id), priorityName(match.priority),
            static_cast<unsigned long>(m_activeSubtitleId), priorityName(m_activeSubtitlePriority));
        Diagnostics::captureSuppressed(displayNowMs, id, priorityName(match.priority),
            m_activeSubtitleId, priorityName(m_activeSubtitlePriority));
        return false;
    }
    if (m_runtime.active() && match.priority == SubtitlePriority::Secondary &&
        m_activeSubtitlePriority == SubtitlePriority::Secondary) {
        const bool speaking = std::chrono::duration<double>(now - m_activeSubtitleStart).count() <
            m_activeSubtitleBaseDuration;
        if (!shouldReplaceSecondary({m_activeSubtitleId, m_activeLevelKnown, m_activeLevelDb}, speaking,
                                    {id, event.levelKnown, event.levelDb})) {
            Diagnostics::log("subtitle_suppressed id=0x%08lx priority=secondary active_id=0x%08lx reason=secondary-rank-or-farther level_db=%.1f active_level_db=%.1f",
                static_cast<unsigned long>(id), static_cast<unsigned long>(m_activeSubtitleId),
                event.levelDb, m_activeLevelDb);
            Diagnostics::captureSuppressed(displayNowMs, id, priorityName(match.priority),
                m_activeSubtitleId, priorityName(m_activeSubtitlePriority));
            return false;
        }
    }
    return startSubtitle(id, match, segments, now, event.timestampMs, event.levelKnown, event.levelDb, nullptr);
}

void ModRuntime::logActiveEnd(const char* reason, uint32_t replacementId, uint64_t displayNowMs,
                              SubtitleRuntime::clock::time_point now)
{
    const auto elapsed = std::chrono::duration<double>(now - m_activeSubtitleStart).count();
    if (m_runtime.currentIndex() < m_activeSubtitleSegments.size())
        Diagnostics::captureSegment("subtitle_segment_end", displayNowMs, m_activeSubtitleId,
            m_runtime.currentIndex(), elapsed,
            m_activeSubtitleSegments[m_runtime.currentIndex()].text);
    Diagnostics::captureSubtitleEnd(m_activeSubtitleDisplayStartMs, displayNowMs,
        m_activeSubtitleId, priorityName(m_activeSubtitlePriority), elapsed,
        m_activeSubtitleBaseDuration, m_activeSubtitleDuration, reason, replacementId);
    Diagnostics::log("subtitle_end id=0x%08lx reason=%s replacement_id=0x%08lx segment=%zu elapsed=%.3f configured_duration=%.3f",
        static_cast<unsigned long>(m_activeSubtitleId), reason, static_cast<unsigned long>(replacementId),
        m_runtime.currentIndex(), elapsed, m_activeSubtitleDuration);
}

void ModRuntime::endActiveSubtitle(const char* reason, SubtitleRuntime::clock::time_point now)
{
    if (!m_runtime.active())
        return;
    logActiveEnd(reason, 0, GetTickCount64(), now);
    m_runtime.reset();
    m_activeSubtitleId = 0;
    m_activeSubtitlePriority = SubtitlePriority::None;
    m_activeSubtitleDisplayStartMs = 0;
    m_activeSubtitleBaseDuration = 0.0;
    m_activeSubtitleDuration = 0.0;
    m_activeSubtitleSegments.clear();
    m_activeIsAmbient = false;
    m_activeAmbientInstance = 0;
}

bool ModRuntime::startSubtitle(uint32_t id, const SubtitleMatch& match,
    const std::vector<SubtitleSegment>& segments, SubtitleRuntime::clock::time_point start,
    uint64_t eventMs, bool levelKnown, float levelDb, const AmbientVoice* ambient)
{
    const auto now = g_playbackClock.state().now;
    const auto displayNowMs = GetTickCount64();
    const double baseDuration = SubtitleEngine::extractDuration(match.raw);
    const double duration = applySubtitleTailExtension(
        baseDuration, g_subtitleSettings.tailExtensionMs);
    if (m_runtime.active())
        logActiveEnd("replaced-by-audio", id, displayNowMs, now);
    double waitsBeforeFinal = 0.0;
    for (size_t i = 0; i + 1 < segments.size(); ++i)
        if (segments[i].waitAfter > 0.0) waitsBeforeFinal += segments[i].waitAfter;
    const double joinedAt = std::chrono::duration<double>(now - start).count();
    Diagnostics::log("subtitle_start id=0x%08lx replaces_active=%d segments=%zu base_duration=%.3f tail_extension_ms=%d duration=%.3f final_window=%.3f joined_at=%.3f",
        static_cast<unsigned long>(id), m_runtime.active(), segments.size(),
        baseDuration > 0.0 ? baseDuration : 3.0, g_subtitleSettings.tailExtensionMs, duration,
        (std::max)(0.0, duration - waitsBeforeFinal), joinedAt);
    // A voice joined mid-line starts in the past; the next runtime update
    // advances to the segment that is playing now.
    m_runtime.start(segments, std::chrono::duration_cast<SubtitleRuntime::clock::duration>(
        std::chrono::duration<double>(duration)), start);
    m_activeSubtitleId = id;
    m_activeSubtitlePriority = match.priority;
    m_activeLevelKnown = levelKnown;
    m_activeLevelDb = levelDb;
    m_activeIsAmbient = ambient != nullptr;
    m_activeAmbientInstance = ambient ? ambient->instance : 0;
    m_activeSubtitleStart = start;
    m_activeSubtitleDisplayStartMs = displayNowMs;
    m_activeSubtitleBaseDuration = baseDuration > 0.0 ? baseDuration : 3.0;
    m_activeSubtitleDuration = duration;
    m_activeSubtitleSegments = segments;
    Diagnostics::captureSubtitleStart(eventMs, displayNowMs, id,
        priorityName(match.priority), m_activeSubtitleBaseDuration, duration, match.raw);
    Diagnostics::captureSegment("subtitle_segment_start", displayNowMs, id, 0, joinedAt,
        segments.front().text);
    m_overlay.setSegments(segments);
    m_overlay.setVisible(true);
    return true;
}

void ModRuntime::addAmbient(const AudioEvent& event, SubtitleRuntime::clock::time_point now)
{
    const auto match = m_engine.getMatch(event.id);
    if (!match) {
        Diagnostics::captureDecision(GetTickCount64(), event.id, event.rawId,
            "missing-subtitle", m_activeSubtitleId);
        return;
    }
    AmbientVoice voice;
    voice.id = event.id;
    voice.instance = event.instance;
    voice.emitter = event.emitter;
    voice.start = now;
    const double baseDuration = SubtitleEngine::extractDuration(match.raw);
    voice.audioSeconds = baseDuration > 0.0 ? baseDuration : 3.0;
    voice.levelKnown = event.levelKnown;
    voice.positional = event.positional;
    voice.distance = event.distance;
    m_ambient.add(voice);
    const AmbientVoice* added = m_ambient.find(voice.id, voice.instance);
    const bool inRange = added && added->inRange;
    Diagnostics::captureDecision(GetTickCount64(), event.id, event.rawId,
        inRange ? "ambient-in-range" : "ambient-out-of-range", m_activeSubtitleId);
    Diagnostics::log("ambient_voice action=start id=0x%08lx distance=%.1f max=%.0f in_range=%d tracked=%zu",
        static_cast<unsigned long>(event.id), event.distance, secondaryMaxDistance(event.id),
        inRange, m_ambient.size());
}

void ModRuntime::updateAmbient(const PlaybackClock::State& playback)
{
    if (playback.paused)
        return;
    m_ambient.update(playback.now, AudioHook::leafActive() ? &AudioHook::readVoiceLevelNow : nullptr);
    if (m_runtime.active() && m_activeSubtitlePriority == SubtitlePriority::Primary)
        return;  // story dialogue always wins; ambient lines can join after it
    const AmbientVoice* shown = m_runtime.active() && m_activeIsAmbient
        ? m_ambient.find(m_activeSubtitleId, m_activeAmbientInstance) : nullptr;
    if (shown && !shown->inRange) {
        Diagnostics::log("ambient_voice action=hide id=0x%08lx distance=%.1f max=%.0f",
            static_cast<unsigned long>(shown->id), shown->distance, secondaryMaxDistance(shown->id));
        endActiveSubtitle("out-of-range", playback.now);
        shown = nullptr;
    }
    const AmbientVoice* best = m_ambient.choose(shown);
    if (!best || best == shown)
        return;
    const AmbientVoice voice = *best;
    const auto match = m_engine.getMatch(voice.id);
    auto segments = SubtitleEngine::parseSegments(SubtitleEngine::stripTags(match.raw));
    if (!match || segments.empty())
        return;
    Diagnostics::log("ambient_voice action=show id=0x%08lx distance=%.1f elapsed=%.3f",
        static_cast<unsigned long>(voice.id), voice.distance,
        std::chrono::duration<double>(playback.now - voice.start).count());
    startSubtitle(voice.id, match, segments, voice.start, GetTickCount64(), voice.levelKnown,
        0.0f, &voice);
}

void ModRuntime::addVoiceProbe(const AudioEvent& event, SubtitlePriority priority)
{
    if (!Diagnostics::captureEnabled() || !event.instance || event.emitter == 0xffffffffu)
        return;
    const auto match = m_engine.getMatch(event.id);
    if (!match)
        return;
    if (m_voiceProbes.size() >= MaxVoiceProbes)
        m_voiceProbes.pop_front();
    VoiceProbe probe;
    probe.id = event.id;
    probe.rawId = event.rawId;
    probe.instance = event.instance;
    probe.emitter = event.emitter;
    probe.startMs = GetTickCount64();
    probe.dbSeconds = SubtitleEngine::extractDuration(match.raw);
    probe.priority = priority;
    m_voiceProbes.push_back(probe);
}

void ModRuntime::updateVoiceProbes(bool paused)
{
    if (!Diagnostics::captureEnabled()) {
        m_voiceProbes.clear();
        return;
    }
    const auto nowMs = GetTickCount64();
    for (auto it = m_voiceProbes.begin(); it != m_voiceProbes.end();) {
        if (paused) it->pausedDuring = true;
        const uint64_t elapsedMs = nowMs >= it->startMs ? nowMs - it->startMs : 0;
        // A freed or reused instance no longer reports the same emitter.
        const VoiceLevel level = AudioHook::readVoiceLevelNow(it->instance);
        const char* reason = level.emitter != it->emitter ? "voice-ended"
            : elapsedMs >= 120000 ? "timeout" : nullptr;
        if (!reason && !it->dbReached && elapsedMs >= it->dbSeconds * 1000.0) {
            it->dbReached = true;
            Diagnostics::log("voice_probe action=db-duration-reached id=0x%08lx elapsed_ms=%llu db_ms=%.0f",
                static_cast<unsigned long>(it->id), static_cast<unsigned long long>(elapsedMs),
                it->dbSeconds * 1000.0);
        }
        if (!reason) {
            ++it;
            continue;
        }
        Diagnostics::captureVoiceEnd(nowMs, it->id, it->rawId, priorityName(it->priority), elapsedMs,
            it->dbSeconds, it->pausedDuring, reason);
        Diagnostics::log("voice_end id=0x%08lx raw=0x%08lx priority=%s voice_ms=%llu db_ms=%.0f paused_during=%u reason=%s",
            static_cast<unsigned long>(it->id), static_cast<unsigned long>(it->rawId), priorityName(it->priority),
            static_cast<unsigned long long>(elapsedMs), it->dbSeconds * 1000.0, it->pausedDuring, reason);
        it = m_voiceProbes.erase(it);
    }
}
