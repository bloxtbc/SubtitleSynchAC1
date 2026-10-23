#include "diagnostics.h"

#include <Windows.h>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <thread>

void check(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
std::string read(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

int main() {
    char temporary[MAX_PATH]{};
    GetTempPathA(MAX_PATH, temporary);
    const auto directory = std::filesystem::path(temporary) /
        ("SubtitleSynchAC1-recording-test-" + std::to_string(GetCurrentProcessId()));
    try {
        std::filesystem::create_directories(directory);
        const auto log = directory / "SubtitleSynchAC1.log";
        const auto capture = directory / "SubtitleSynchAC1.capture.tsv";
        // Users' old testing INIs must no longer start recording implicitly.
        std::ofstream(directory / "SubtitleSynchAC1.ini") <<
            "[Diagnostics]\nEnabled=1\n[Capture]\nEnabled=1\n";
        Diagnostics::init(directory.string());
        check(!Diagnostics::recording(), "legacy INI defaults to manual recording");
        Diagnostics::log("must-not-create-a-file");
        check(!std::filesystem::exists(log) && !std::filesystem::exists(capture),
            "disabled recording creates no files");
        check(Diagnostics::setRecording(true), "enable both logs without restarting");
        Diagnostics::captureMarker(GetTickCount64(), 1, 0x20abcdef, "primary", 1.2, "retained marker");
        std::atomic<bool> running{true};
        std::thread producer([&] {
            while (running.load()) {
                Diagnostics::log("thread-event");
                Diagnostics::captureAudio(GetTickCount64(), 1, 1, GetCurrentThreadId(), 1, "missing");
            }
        });
        for (unsigned cycle = 0; cycle < 20; ++cycle) {
            Diagnostics::setRecording(false);
            check(Diagnostics::setRecording(true), "recording can be reenabled during audio callbacks");
        }
        running.store(false); producer.join();
        Diagnostics::setRecording(false);
        check(!Diagnostics::enabled() && !Diagnostics::captureEnabled(), "both streams stop together");
        const auto oldLog = read(log), oldCapture = read(capture);
        check(oldCapture.find("retained marker") != std::string::npos,
            "toggle appends and preserves earlier F2 data");
        check(oldCapture.find("session_end") != std::string::npos &&
            oldLog.find("recording_stop") != std::string::npos, "disable flushes session endings");
        Diagnostics::log("disabled-event");
        Diagnostics::captureAudio(GetTickCount64(), 2, 2, 1, 1, "missing");
        check(oldLog == read(log) && oldCapture == read(capture), "no writes after disabling");
        check(Diagnostics::setRecording(true), "recording can restart");
        Diagnostics::captureAudio(GetTickCount64(), 0x20abcdef, 0x10abcdef, 1, 1, "missing");
        Diagnostics::captureDecision(GetTickCount64(), 0x20abcdef, 0x10abcdef, "missing-subtitle", 0x20b40022);
        Diagnostics::captureQueueDrops(GetTickCount64(), 3);
        Diagnostics::setRecording(false);
        const auto restarted = read(capture);
        check(restarted.find(oldCapture) == 0 &&
            restarted.find("reason=missing-subtitle") != std::string::npos &&
            restarted.find("count=3") != std::string::npos,
            "a restarted session appends missing sounds and loss diagnostics");
        std::ofstream(directory / "SubtitleSynchAC1.ini") << "[Diagnostics]\nStartOnLaunch=1\n";
        WritePrivateProfileStringA(nullptr, nullptr, nullptr,
            (directory / "SubtitleSynchAC1.ini").string().c_str());
        Diagnostics::init(directory.string());
        check(Diagnostics::recording(), "explicit StartOnLaunch opt-in starts recording");
        Diagnostics::setRecording(false);
        std::filesystem::remove(directory / "SubtitleSynchAC1.ini");
        std::filesystem::remove(log);
        std::filesystem::remove(capture);
        std::filesystem::remove(directory);
        std::cout << "PASS: manual defaults, runtime toggles, concurrent callbacks, append and explicit opt-in\n";
    } catch (const std::exception& e) {
        Diagnostics::setRecording(false);
        std::cerr << e.what() << '\n';
        return 1;
    }
}
