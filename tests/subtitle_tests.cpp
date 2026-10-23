#include "subtitle_core.h"
#include "subtitle_runtime.h"
#include "audio_system.h"
#include "pattern_scan.h"
#include "ambient_voices.h"
#include <fstream>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <cstring>

uint32_t memory[256]{};
// Live distances for the ambient tracker tests, by fake instance address.
float liveDistance[4]{};
uint32_t liveEmitter[4]{};
VoiceLevel readLive(uintptr_t instance) {
    VoiceLevel level;
    if (instance < 1 || instance > 3) return level;
    level.emitter = liveEmitter[instance];
    level.known = level.positional = true;
    level.distance = liveDistance[instance];
    return level;
}
bool readFixture(uintptr_t address, void* output, size_t size) {
    if (address < 0x1000 || address + size > 0x1000 + sizeof(memory)) return false;
    std::memcpy(output, reinterpret_cast<const char*>(memory) + address - 0x1000, size);
    return true;
}

void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
int main() {
    try {
        // Observed Ubisoft DX9 reference: event 10a80305 -> audio 20a802f9,
        // NOT the old high-nibble substitution 20a80305.
        memory[0] = 3; memory[1] = 3; memory[2] = 0x1040;
        memory[20] = 0x10a80305; memory[21] = 0x1100; // table handle 1
        memory[24] = 0x20a802f9; // table handle 2
        memory[64] = 0x00a80305; memory[65] = 1; memory[66] = 2;
        auto resolved = resolveAudio(0x10a80305, 0x1000, 1, readFixture);
        check(resolved.id == 0x20a802f9 && resolved.resolution == Resolution::DirectReference, "observed event-to-audio reference");
        memory[65] = 4;
        check(resolveAudio(0x10a80305, 0x1000, 1, readFixture).resolution == Resolution::Unresolved, "do not guess composite branches");
        memory[65] = 1; memory[66] = 3;
        check(!resolveAudio(0x10a80305, 0x1000, 1, readFixture).id, "out of bounds handle");
        memory[66] = 2;
        check(!resolveAudio(0x10a80306, 0x1000, 1, readFixture).id, "stale root ID");
        check(!resolveAudio(0x10a80305, 0x2000, 1, readFixture).id, "unreadable manager");
        check(resolveAudio(0x20a802f9, 0, 0, nullptr).id == 0x20a802f9, "exact audio ID needs no conversion");
        {
            const float citizen[4] = {1, -1, 1, 20}, defaults[4] = {-2, -2, -2, -2};
            check(voiceCurveDb(0.5f, citizen) == 0.0f, "inside minimum distance is full level");
            check(std::fabs(voiceCurveDb(10, citizen) + 5.58f) < 0.05f, "linear fade halfway");
            check(voiceCurveDb(20, citizen) == -96.0f && voiceCurveDb(100, citizen) == -96.0f, "silent beyond max distance");
            check(std::fabs(voiceCurveDb(1, defaults) + 6.02f) < 0.05f, "engine default inverse-distance curve");
            check(secondaryRank(0x20970aa4) > secondaryRank(0x20980202) &&
                  secondaryRank(0x20980202) > secondaryRank(0x20a00043), "victim > guard > preacher");
            SecondaryCandidate guard{0x20980202, true, -3}, victim{0x20970aa4, true, -9},
                nearGuard{0x20980203, true, -1}, farGuard{0x20980204, true, -8}, unknown{0x20980205};
            check(shouldReplaceSecondary(guard, true, victim), "victim interrupts guard");
            check(!shouldReplaceSecondary(victim, true, guard), "guard cannot interrupt victim");
            check(shouldReplaceSecondary(victim, false, guard), "after the audio only the tail remains");
            check(shouldReplaceSecondary(guard, true, nearGuard) && !shouldReplaceSecondary(guard, true, farGuard),
                  "nearer speaker of the same kind wins");
            check(shouldReplaceSecondary(guard, true, unknown), "unknown level keeps old behaviour");
        }
        check(removeSoundCaptions("[laughs]").empty(), "caption-only segment becomes empty");
        check(removeSoundCaptions("[cries out] No!") == "No!", "caption before speech is removed");
        check(removeSoundCaptions("No! [cries out] Let go!") == "No! Let go!", "inner caption leaves one space");
        {
            using clk = std::chrono::steady_clock;
            check(secondaryMaxDistance(0x20a00043) == 20.0f && secondaryMaxDistance(0x20970aa4) == 15.0f,
                  "preachers show from 20 m, citizens from 15 m");
            const auto t0 = clk::now();
            auto voice = [&](uint32_t id, uintptr_t instance, float distance) {
                AmbientVoice v;
                v.id = id; v.instance = instance; v.emitter = 0x100 + static_cast<uint32_t>(instance);
                v.start = t0; v.audioSeconds = 10.0;
                v.levelKnown = v.positional = true; v.distance = distance;
                liveEmitter[instance] = v.emitter; liveDistance[instance] = distance;
                return v;
            };
            AmbientVoices tracker;
            tracker.add(voice(0x20a00043, 1, 60.0f));  // preacher out of range
            check(!tracker.choose(nullptr), "a far preacher is not shown");
            liveDistance[1] = 15.0f;
            tracker.update(t0 + std::chrono::seconds(1), readLive);
            const AmbientVoice* shown = tracker.choose(nullptr);
            check(shown && shown->id == 0x20a00043, "walking near a speaking preacher shows him mid-line");
            liveDistance[1] = 21.5f;
            tracker.update(t0 + std::chrono::seconds(2), readLive);
            check(tracker.choose(nullptr) != nullptr, "a small step over the limit does not flicker");
            liveDistance[1] = 23.0f;
            tracker.update(t0 + std::chrono::seconds(3), readLive);
            check(!tracker.choose(nullptr), "leaving the range hides the line");
            liveDistance[1] = 21.0f;
            tracker.update(t0 + std::chrono::seconds(4), readLive);
            check(!tracker.choose(nullptr), "coming back needs the real limit");
            liveDistance[1] = 19.0f;
            tracker.update(t0 + std::chrono::seconds(5), readLive);
            check(tracker.choose(nullptr) != nullptr, "coming back in range shows it again");

            tracker.add(voice(0x20980202, 2, 10.0f));  // guard, nearer than the preacher
            shown = tracker.choose(nullptr);
            check(shown && shown->id == 0x20980202, "a guard outranks a preacher");
            tracker.add(voice(0x20970aa4, 3, 12.0f));  // citizen to rescue
            shown = tracker.choose(tracker.find(0x20980202, 2));
            check(shown && shown->id == 0x20970aa4, "the citizen to rescue wins over guards");

            AmbientVoices guards;
            guards.add(voice(0x20980202, 1, 10.0f));
            guards.add(voice(0x20980203, 2, 8.0f));
            const AmbientVoice* current = guards.find(0x20980202, 1);
            check(guards.choose(current) == current, "a slightly nearer guard does not take over");
            liveDistance[2] = 6.0f;
            guards.update(t0 + std::chrono::seconds(1), readLive);
            current = guards.find(0x20980202, 1);
            shown = guards.choose(current);
            check(shown && shown->id == 0x20980203, "a clearly nearer guard takes over");
            check(guards.choose(nullptr)->id == 0x20980203, "without a current line the nearest wins");
            liveEmitter[2] = 0x999;
            guards.update(t0 + std::chrono::seconds(2), readLive);
            check(!guards.find(0x20980203, 2), "a reused voice instance ends the line");
            guards.update(t0 + std::chrono::seconds(11), readLive);
            check(guards.size() == 0, "voices end with their audio");

            AmbientVoices speaker;
            speaker.add(voice(0x20980202, 1, 5.0f));
            AmbientVoice next = voice(0x20980210, 2, 5.0f);
            next.emitter = 0x101;  // same speaker, new line
            speaker.add(next);
            check(speaker.size() == 1 && speaker.find(0x20980210, 2), "a speaker's new line replaces the old one");

            AmbientVoices flat;
            AmbientVoice twoD = voice(0x20980202, 1, -1.0f);
            twoD.positional = false;
            flat.add(twoD);
            check(flat.choose(nullptr) != nullptr, "2D voices are always in range");
            AmbientVoice unknown = voice(0x20980203, 0, -1.0f);
            unknown.levelKnown = unknown.positional = false;
            unknown.emitter = 0xffffffffu;
            AmbientVoices blind;
            blind.add(unknown);
            blind.update(t0 + std::chrono::seconds(1), readLive);
            check(blind.choose(nullptr) != nullptr, "unknown level keeps the old always-show rule");

            SubtitleRuntime joined;
            joined.start({{"A", 2.0}, {"B", 2.0}, {"C", 0.0}},
                std::chrono::duration_cast<SubtitleRuntime::clock::duration>(std::chrono::duration<double>(10.0)),
                t0 - std::chrono::seconds(3));
            joined.update(t0);
            check(joined.currentText() == "B", "joining mid-line starts at the segment playing now");
        }
        check(removeSoundCaptions("First line\n[gasps]") == "First line", "explicit line break survives");
        check(removeSoundCaptions("No captions here.") == "No captions here.", "plain text unchanged");
        uint32_t leafEvent = 0;
        auto leaf = resolveLeafAudio(0x1000, 1, leafEvent, readFixture);
        check(leaf.id == 0x20a802f9 && leafEvent == 0x10a80305 && leaf.resolution == Resolution::LeafVoice,
              "leaf voice start reports the chosen resource");
        check(!resolveLeafAudio(0x1000, 2, leafEvent, readFixture).id && !leafEvent, "resource handle is not a leaf event");
        memory[65] = 11;
        check(!resolveLeafAudio(0x1000, 1, leafEvent, readFixture).id, "switch event is not a leaf");
        memory[65] = 1;
        check(!resolveLeafAudio(0, 1, leafEvent, readFixture).id, "no manager seen yet");
        const char* fixture = "lookup_fixture.json";
        const char* secondaryFixture = "lookup_secondary_fixture.json";
        { std::ofstream f(fixture); f << R"({"0x10a80001":"exact<duration=2>","0x20a80001":"other<duration=2>","0x20a80002":"first<wait=1>second<duration=2>"})"; }
        { std::ofstream f(secondaryFixture); f << R"({"0x20a80001":"duplicate secondary<duration=4>","0x20a80003":"ambient<duration=4>"})"; }
        SubtitleEngine engine;
        check(engine.load(fixture), "load fixture");
        check(engine.loadSecondary(secondaryFixture), "load secondary fixture");
        check(engine.getRaw(0x10a80001) == "exact<duration=2>", "exact ID must win over transformed ID");
        check(engine.getRaw(0x20a80001) == "other<duration=2>" &&
              engine.getPriority(0x20a80001) == SubtitlePriority::Primary,
              "primary database wins duplicate IDs");
        check(engine.getRaw(0x20a80003) == "ambient<duration=4>" &&
              engine.getPriority(0x20a80003) == SubtitlePriority::Secondary,
              "secondary database supplies ambient subtitle");
        check(!shouldDisplaySubtitle(SubtitlePriority::Primary, SubtitlePriority::Secondary),
              "secondary subtitle cannot cover active mission subtitle");
        check(shouldDisplaySubtitle(SubtitlePriority::Secondary, SubtitlePriority::Primary),
              "mission subtitle replaces active secondary subtitle");
        check(shouldDisplaySubtitle(SubtitlePriority::Secondary, SubtitlePriority::Secondary),
              "new secondary speech replaces older secondary speech");
        check(engine.getRaw(0x30a80001).empty(), "unknown resource type must not alias voice");
        check(engine.getRaw(0).empty(), "unknown zero");
        SubtitleRuntime runtime;
        using C = SubtitleRuntime::clock;
        const auto t = C::time_point{};
        check(std::abs(applySubtitleTailExtension(2.0, 1500) - 3.5) < 0.0001,
              "configured tail extends the database duration");
        check(std::abs(applySubtitleTailExtension(2.0, -500) - 2.0) < 0.0001,
              "invalid negative tail cannot shorten a subtitle");
        const auto segments = engine.getSegments(0x20a80002);
        runtime.start(segments, std::chrono::seconds(2), t);
        auto runtimeEvent = runtime.update(t + std::chrono::milliseconds(1100));
        check(runtimeEvent.kind == SubtitleUpdateKind::SegmentChanged &&
              runtimeEvent.previousIndex == 0 && runtimeEvent.currentIndex == 1,
              "segment transition reports its indices");
        check(runtime.currentText() == "second", "advance segment");
        runtimeEvent = runtime.update(t + std::chrono::seconds(2));
        check(runtimeEvent.kind == SubtitleUpdateKind::DurationExpired,
              "duration expiry reports why the subtitle ended");
        check(!runtime.active(), "total duration caps last segment");
        runtime.start(segments, std::chrono::seconds(2), t);
        runtime.update(t + std::chrono::seconds(20));
        check(!runtime.active(), "delayed render expires subtitle");
        runtime.start(segments, std::chrono::seconds(2), t);
        runtime.start(segments, std::chrono::seconds(2), t + std::chrono::seconds(1));
        runtime.update(t + std::chrono::milliseconds(1500));
        check(runtime.currentText() == "first", "repeated playback restarts");
        runtime.start(segments, std::chrono::milliseconds(3500), t);
        runtime.update(t + std::chrono::milliseconds(3400));
        check(runtime.active(), "tail extension keeps the final subtitle visible");
        runtime.start(segments, std::chrono::milliseconds(3500), t + std::chrono::milliseconds(3400));
        check(runtime.currentText() == "first", "a new subtitle still replaces an extended one immediately");
        AudioQueue queue;
        check(queue.push({0, 1, 1}) && queue.push({42, 2, 2}) && queue.push({42, 2, 3}), "enqueue repeated and zero IDs");
        AudioEvent event;
        check(queue.pop(event) && event.id == 0, "zero is an event, not empty sentinel");
        check(queue.pop(event) && event.timestampMs == 2, "first playback");
        check(queue.pop(event) && event.timestampMs == 3, "repeated playback preserved");
        check(!queue.pop(event), "empty queue");
        check(queue.push({11,0,0}) && queue.push({12,0,0}) && queue.discardAll() == 2,
              "pause transition discards queued events");
        check(!queue.pop(event), "discard leaves queue empty");
        for (size_t i=0; i<AudioQueue::Capacity; ++i) check(queue.push({42,0,i}), "fill queue");
        check(!queue.push({42,0,0}) && queue.takeDropped() == 1, "overflow counted");
        while (queue.pop(event)) {}
        // Concurrent producers: accepted + explicitly dropped must account for all events.
        std::thread a([&] { for (int i=0;i<10000;++i) queue.push({1,1,0}); });
        std::thread b([&] { for (int i=0;i<10000;++i) queue.push({2,2,0}); });
        a.join(); b.join();
        size_t accepted=0;
        while (queue.pop(event)) { ++accepted; check(event.id == event.threadId, "event fields stay together"); }
        check(accepted + queue.takeDropped() == 20000, "concurrent event accounting");
        uint8_t bytes[] = {0xE8,1,2,0xE8,1,3};
        check(PatternScan::FindOffsets(bytes, sizeof bytes, "E8 01 ??").size() == 2, "ambiguous pattern detected");
        check(PatternScan::FindOffsets(bytes, sizeof bytes, "E8 01 03").size() == 1, "unique pattern");
        check(PatternScan::FindOffsets(bytes, 1, "E8 01 03").empty(), "short buffer no unsigned underflow");
        check(PatternScan::FindOffsets(bytes, sizeof bytes, "").empty(), "empty pattern rejected");
        std::remove(fixture);
        std::remove(secondaryFixture);
        std::cout << "PASS: lookup, timing, replay, queue concurrency/overflow, pattern matching\n";
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
