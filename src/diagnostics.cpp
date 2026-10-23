#include "diagnostics.h"
#include <Windows.h>
#include <cstdio>
#include <cstdarg>
#include <mutex>
#include <atomic>
#include <string>
namespace {
std::mutex logMutex;
FILE* logFile = nullptr;
FILE* captureFile = nullptr;
uint64_t captureSessionStart = 0;
uint64_t captureLastFlush = 0;
std::string logDirectory;
std::atomic<bool> logEnabled{false};
std::atomic<bool> captureActive{false};
std::atomic<uint64_t> sessionSequence{0};
constexpr const char* Build = "audio-sync-v5.22";

std::string oneLine(const std::string& value) {
    std::string result;
    result.reserve(value.size());
    for (char c : value) {
        if (c == '\r' || c == '\n' || c == '\t') result.push_back(' ');
        else result.push_back(c);
    }
    return result;
}

void capturePrefix(const char* type, uint64_t atMs) {
    if (!captureFile) return;
    const uint64_t relative = atMs >= captureSessionStart ? atMs - captureSessionStart : 0;
    std::fprintf(captureFile, "%s\t%llu\t%llu\t", type,
        static_cast<unsigned long long>(atMs),
        static_cast<unsigned long long>(relative));
}
// Call only while holding logMutex. Profile switches close the old session.
void closeRecording(const char* reason) {
    logEnabled.store(false, std::memory_order_release);
    captureActive.store(false, std::memory_order_release);
    if (logFile) {
        std::fprintf(logFile, "t=%llu recording_stop reason=%s\n",
            static_cast<unsigned long long>(GetTickCount64()), reason);
        std::fclose(logFile);
        logFile = nullptr;
    }
    if (captureFile) {
        capturePrefix("session_end", GetTickCount64());
        std::fprintf(captureFile, "reason=%s\n", reason);
        std::fclose(captureFile);
        captureFile = nullptr;
    }
}
}
namespace Diagnostics {
void init(const std::string& directory) {
    logDirectory = directory;
    const auto ini = directory + "\\SubtitleSynchAC1.ini";
    // Legacy Enabled=1 files do not silently record every new game session.
    // Starting automatically now requires an explicit opt-in to StartOnLaunch.
    if (GetPrivateProfileIntA("Diagnostics", "StartOnLaunch", 0, ini.c_str()))
        setRecording(true);
}
bool enabled() { return logEnabled.load(std::memory_order_acquire); }
bool captureEnabled() { return captureActive.load(std::memory_order_acquire); }
bool recording() { return enabled() || captureEnabled(); }
uint64_t recordingSession() { return sessionSequence.load(std::memory_order_acquire); }
bool setRecording(bool active) {
    std::lock_guard<std::mutex> lock(logMutex);
    if (!active) {
        closeRecording("recording-disabled");
        return true;
    }
    if (logDirectory.empty()) return false;
    // Opening both is atomic from the UI's point of view: an incomplete
    // recording is rejected instead of silently losing F2 or pause events.
    if (logFile && captureFile) return true;
    const auto stem = logDirectory + "\\SubtitleSynchAC1";
    FILE* newLog = std::fopen((stem + ".log").c_str(), "a");
    FILE* newCapture = std::fopen((stem + ".capture.tsv").c_str(), "a");
    if (!newLog || !newCapture) {
        if (newLog) std::fclose(newLog);
        if (newCapture) std::fclose(newCapture);
        OutputDebugStringA("SubtitleSynchAC1: cannot open recording files\n");
        return false;
    }
    closeRecording("reopen");
    logFile = newLog;
    captureFile = newCapture;
    captureSessionStart = GetTickCount64();
    captureLastFlush = captureSessionStart;
    std::setvbuf(captureFile, nullptr, _IOFBF, 64 * 1024);
    std::fprintf(captureFile,
        "session\t%llu\t0\tbuild=%s\n#type\ttick_ms\tsession_ms\tfields\n",
        static_cast<unsigned long long>(captureSessionStart), Build);
    char exe[MAX_PATH]{};
    GetModuleFileNameA(nullptr, exe, MAX_PATH);
    auto base = reinterpret_cast<const unsigned char*>(GetModuleHandleA(nullptr));
    auto dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    auto nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
    std::fprintf(logFile, "t=%llu session exe=%s pe_timestamp=%08lx image_size=%08lx build=%s\n",
        static_cast<unsigned long long>(captureSessionStart), exe,
        nt->FileHeader.TimeDateStamp, nt->OptionalHeader.SizeOfImage, Build);
    std::fflush(logFile);
    std::fflush(captureFile);
    sessionSequence.fetch_add(1, std::memory_order_release);
    logEnabled.store(true, std::memory_order_release);
    captureActive.store(true, std::memory_order_release);
    return true;
}
void log(const char* format, ...) {
    if (!enabled()) return;
    std::lock_guard<std::mutex> lock(logMutex);
    if (!logFile) return;
    std::fprintf(logFile, "t=%llu ", static_cast<unsigned long long>(GetTickCount64()));
    va_list args;
    va_start(args, format);
    std::vfprintf(logFile, format, args);
    va_end(args);
    std::fputc('\n', logFile);
    std::fflush(logFile);
}
void error(const char* message) {
    OutputDebugStringA(message);
    log("error=%s", message);
}

void captureAudio(uint64_t eventMs, uint32_t id, uint32_t rawId, uint32_t threadId,
                  uint32_t resolution, const char* mapping) {
    if (!captureEnabled()) return;
    std::lock_guard<std::mutex> lock(logMutex);
    if (!captureFile) return;
    capturePrefix("audio_dispatch", eventMs);
    std::fprintf(captureFile, "id=0x%08lx\traw=0x%08lx\tthread=%lu\tresolution=%lu\tmapping=%s\n",
        static_cast<unsigned long>(id), static_cast<unsigned long>(rawId),
        static_cast<unsigned long>(threadId), static_cast<unsigned long>(resolution), mapping);
    if (eventMs >= captureLastFlush + 1000) {
        std::fflush(captureFile);
        captureLastFlush = eventMs;
    }
}

void captureSubtitleStart(uint64_t eventMs, uint64_t displayMs, uint32_t id,
                          const char* priority, double databaseDuration,
                          double effectiveDuration, const std::string& text) {
    if (!captureEnabled()) return;
    std::lock_guard<std::mutex> lock(logMutex);
    if (!captureFile) return;
    capturePrefix("subtitle_start", displayMs);
    const auto clean = oneLine(text);
    std::fprintf(captureFile,
        "id=0x%08lx\tpriority=%s\taudio_dispatch_ms=%llu\tdispatch_to_display_ms=%lld"
        "\tdb_duration_ms=%.0f\teffective_duration_ms=%.0f\ttext=%s\n",
        static_cast<unsigned long>(id), priority,
        static_cast<unsigned long long>(eventMs),
        static_cast<long long>(displayMs) - static_cast<long long>(eventMs),
        databaseDuration * 1000.0, effectiveDuration * 1000.0, clean.c_str());
    std::fflush(captureFile);
}

void captureSubtitleEnd(uint64_t startMs, uint64_t endMs, uint32_t id,
                        const char* priority, double playbackElapsed,
                        double databaseDuration, double effectiveDuration,
                        const char* reason, uint32_t replacementId) {
    if (!captureEnabled()) return;
    std::lock_guard<std::mutex> lock(logMutex);
    if (!captureFile) return;
    capturePrefix("subtitle_end", endMs);
    std::fprintf(captureFile,
        "id=0x%08lx\tpriority=%s\tstart_ms=%llu\tend_ms=%llu\twall_elapsed_ms=%llu"
        "\tplayback_elapsed_ms=%.0f\tdb_duration_ms=%.0f\teffective_duration_ms=%.0f"
        "\treason=%s\treplacement_id=0x%08lx\n",
        static_cast<unsigned long>(id), priority,
        static_cast<unsigned long long>(startMs), static_cast<unsigned long long>(endMs),
        static_cast<unsigned long long>(endMs >= startMs ? endMs - startMs : 0),
        playbackElapsed * 1000.0, databaseDuration * 1000.0, effectiveDuration * 1000.0,
        reason, static_cast<unsigned long>(replacementId));
    std::fflush(captureFile);
}

void captureSegment(const char* action, uint64_t atMs, uint32_t id, size_t index,
                    double playbackElapsed, const std::string& text) {
    if (!captureEnabled()) return;
    std::lock_guard<std::mutex> lock(logMutex);
    if (!captureFile) return;
    capturePrefix(action, atMs);
    const auto clean = oneLine(text);
    std::fprintf(captureFile, "id=0x%08lx\tsegment=%zu\tplayback_elapsed_ms=%.0f\ttext=%s\n",
        static_cast<unsigned long>(id), index, playbackElapsed * 1000.0, clean.c_str());
    std::fflush(captureFile);
}

void captureSuppressed(uint64_t atMs, uint32_t id, const char* priority,
                       uint32_t activeId, const char* activePriority) {
    if (!captureEnabled()) return;
    std::lock_guard<std::mutex> lock(logMutex);
    if (!captureFile) return;
    capturePrefix("subtitle_suppressed", atMs);
    std::fprintf(captureFile, "id=0x%08lx\tpriority=%s\tactive_id=0x%08lx\tactive_priority=%s\n",
        static_cast<unsigned long>(id), priority, static_cast<unsigned long>(activeId), activePriority);
    std::fflush(captureFile);
}

void captureMarker(uint64_t atMs, uint32_t sequence, uint32_t activeId, const char* activePriority,
                   double playbackElapsed, const std::string& text) {
    if (!captureEnabled()) return;
    std::lock_guard<std::mutex> lock(logMutex);
    if (!captureFile) return;
    capturePrefix("review_marker_f2", atMs);
    const auto clean = oneLine(text);
    std::fprintf(captureFile, "marker=%lu\tactive_id=0x%08lx\tactive_priority=%s\tplayback_elapsed_ms=%.0f\ttext=%s\n",
        static_cast<unsigned long>(sequence), static_cast<unsigned long>(activeId),
        activePriority, playbackElapsed * 1000.0, clean.c_str());
    std::fflush(captureFile);
}

void captureDisplay(uint64_t atMs, bool visible, uint32_t activeId, bool paused,
                    const std::string& text) {
    if (!captureEnabled()) return;
    std::lock_guard<std::mutex> lock(logMutex);
    if (!captureFile) return;
    capturePrefix("display_state", atMs);
    const auto clean = oneLine(text);
    std::fprintf(captureFile, "visible=%u\tactive_id=0x%08lx\tpaused=%u\ttext=%s\n",
        visible, static_cast<unsigned long>(activeId), paused, clean.c_str());
    std::fflush(captureFile);
}
void captureDecision(uint64_t atMs, uint32_t id, uint32_t rawId,
                     const char* reason, uint32_t activeId) {
    if (!captureEnabled()) return;
    std::lock_guard<std::mutex> lock(logMutex);
    if (!captureFile) return;
    capturePrefix("audio_decision", atMs);
    std::fprintf(captureFile, "id=0x%08lx\traw=0x%08lx\treason=%s\tactive_id=0x%08lx\n",
        static_cast<unsigned long>(id), static_cast<unsigned long>(rawId), reason,
        static_cast<unsigned long>(activeId));
}
void captureVoiceEnd(uint64_t atMs, uint32_t id, uint32_t rawId, const char* priority,
                     uint64_t elapsedMs, double databaseDuration, bool pausedDuring, const char* reason) {
    if (!captureEnabled()) return;
    std::lock_guard<std::mutex> lock(logMutex);
    if (!captureFile) return;
    capturePrefix("voice_end", atMs);
    std::fprintf(captureFile,
        "id=0x%08lx\traw=0x%08lx\tpriority=%s\tvoice_ms=%llu\tdb_duration_ms=%.0f"
        "\tvoice_minus_db_ms=%.0f\tpaused_during=%u\treason=%s\n",
        static_cast<unsigned long>(id), static_cast<unsigned long>(rawId), priority,
        static_cast<unsigned long long>(elapsedMs), databaseDuration * 1000.0,
        static_cast<double>(elapsedMs) - databaseDuration * 1000.0, pausedDuring, reason);
    std::fflush(captureFile);
}
void captureQueueDrops(uint64_t atMs, uint32_t count) {
    if (!captureEnabled()) return;
    std::lock_guard<std::mutex> lock(logMutex);
    if (!captureFile) return;
    capturePrefix("queue_dropped", atMs);
    std::fprintf(captureFile, "count=%lu\n", static_cast<unsigned long>(count));
    std::fflush(captureFile);
}
}
