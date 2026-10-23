#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
namespace Diagnostics {
void init(const std::string& directory);
bool enabled();
bool captureEnabled();
bool recording();
bool setRecording(bool active);
uint64_t recordingSession();
void log(const char* format, ...);
void error(const char* message);
void captureAudio(uint64_t eventMs, uint32_t id, uint32_t rawId, uint32_t threadId,
                  uint32_t resolution, const char* mapping);
void captureSubtitleStart(uint64_t eventMs, uint64_t displayMs, uint32_t id,
                          const char* priority, double databaseDuration,
                          double effectiveDuration, const std::string& text);
void captureSubtitleEnd(uint64_t startMs, uint64_t endMs, uint32_t id,
                        const char* priority, double playbackElapsed,
                        double databaseDuration, double effectiveDuration,
                        const char* reason, uint32_t replacementId);
void captureSegment(const char* action, uint64_t atMs, uint32_t id, size_t index,
                    double playbackElapsed, const std::string& text);
void captureSuppressed(uint64_t atMs, uint32_t id, const char* priority,
                       uint32_t activeId, const char* activePriority);
void captureMarker(uint64_t atMs, uint32_t sequence, uint32_t activeId, const char* activePriority,
                   double playbackElapsed, const std::string& text);
void captureDisplay(uint64_t atMs, bool visible, uint32_t activeId, bool paused,
                    const std::string& text);
void captureDecision(uint64_t atMs, uint32_t id, uint32_t rawId,
                     const char* reason, uint32_t activeId);
// Real play length of a voice instance, measured until the engine frees it.
void captureVoiceEnd(uint64_t atMs, uint32_t id, uint32_t rawId, const char* priority,
                     uint64_t elapsedMs, double databaseDuration, bool pausedDuring, const char* reason);
void captureQueueDrops(uint64_t atMs, uint32_t count);
}
