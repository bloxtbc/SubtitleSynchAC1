#include "playback_clock.h"
#include "subtitle_runtime.h"
#include "hooks/asm_hooks.h"
#include "pause_manager_probe.h"
#include "pattern_scan.h"
#include <Windows.h>
#include <MinHook.h>
#include <xbyak/xbyak.h>
#include <iostream>
#include <stdexcept>
#include <sstream>
#include <vector>
#include <cstring>

namespace {
std::vector<uint8_t> patternBytes(std::string_view pattern) {
    std::istringstream tokens{std::string(pattern)};
    std::vector<uint8_t> result;
    std::string token;
    while (tokens >> token)
        result.push_back(token == "??" ? 0 : static_cast<uint8_t>(std::stoul(token, nullptr, 16)));
    return result;
}
}

void check(bool condition, const char* reason) { if (!condition) throw std::runtime_error(reason); }
int main() {
    try {
        const uint8_t pauseManagerBytes[] = {
            0x90, 0xA1, 0x70, 0x19, 0xA1, 0x01, 0x83, 0xEC, 0x0C, 0x85, 0xC0,
            0x0F, 0x85, 0xB8, 0x00, 0x00, 0x00, 0xA1, 0x8C, 0x5A, 0x91, 0x01,
            0x8B, 0x0D, 0x44, 0x37, 0xA1, 0x01, 0x8B, 0x09, 0x8B, 0x11, 0xF7,
            0xD8, 0x1B, 0xC0, 0x25, 0x20, 0x26, 0x68, 0x01, 0x56, 0x50, 0x90
        };
        const auto pauseManagerMatches = PatternScan::FindOffsets(pauseManagerBytes,
            sizeof(pauseManagerBytes), PauseManagerProbe::Pattern);
        check(pauseManagerMatches.size() == 1 && pauseManagerMatches[0] == 1,
            "PauseManager signature resolves one exact initialization sequence");

        // Exercise the production read-only probe against a mapped PE fixture,
        // with independent game-time and menu state (as in Animus tutorials).
        std::vector<uint8_t> image(0x1400);
        auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(image.data());
        dos->e_magic = IMAGE_DOS_SIGNATURE;
        dos->e_lfanew = 0x80;
        auto* nt = reinterpret_cast<IMAGE_NT_HEADERS*>(image.data() + 0x80);
        nt->Signature = IMAGE_NT_SIGNATURE;
        nt->FileHeader.NumberOfSections = 1;
        nt->FileHeader.SizeOfOptionalHeader = sizeof(IMAGE_OPTIONAL_HEADER);
        nt->OptionalHeader.SizeOfImage = static_cast<DWORD>(image.size());
        auto* section = IMAGE_FIRST_SECTION(nt);
        section->VirtualAddress = 0x400;
        section->Misc.VirtualSize = 0x800;
        section->Characteristics = IMAGE_SCN_MEM_EXECUTE;
        const auto countCode = patternBytes(PauseManagerProbe::Pattern);
        const auto menuCode = patternBytes(PauseManagerProbe::MenuPattern);
        const auto stateCode = patternBytes(PauseManagerProbe::MenuStatePattern);
        std::memcpy(image.data() + 0x400, countCode.data(), countCode.size());
        std::memcpy(image.data() + 0x500, menuCode.data(), menuCode.size());
        std::memcpy(image.data() + 0x600, stateCode.data(), stateCode.size());
        uint32_t countObject[16]{};
        uint8_t menuObject[0x80]{};
        uintptr_t countPointer = reinterpret_cast<uintptr_t>(countObject);
        uintptr_t menuPointer = reinterpret_cast<uintptr_t>(menuObject);
        const uintptr_t countSlot = reinterpret_cast<uintptr_t>(&countPointer);
        const uintptr_t menuSlot = reinterpret_cast<uintptr_t>(&menuPointer);
        std::memcpy(image.data() + 0x401, &countSlot, sizeof(countSlot));
        for (size_t offset : {size_t(2), size_t(30), size_t(41)})
            std::memcpy(image.data() + 0x500 + offset, &menuSlot, sizeof(menuSlot));
        PauseManagerProbe probe;
        check(probe.initialize(image.data()), "both menu signatures enable the real probe");
        PauseManagerSnapshot snapshot;
        countObject[0x24 / 4] = countObject[0x28 / 4] = 1;
        check(probe.sample(snapshot) && snapshot.countsAvailable && !snapshot.menuOpen,
            "tutorial game-time freeze is not a menu pause");
        PlaybackClock tutorialClock;
        SubtitleRuntime tutorialLine;
        MenuPauseTracker tutorialMenu;
        const auto tutorialZero = PlaybackClock::Clock::time_point{};
        using TutorialMs = std::chrono::milliseconds;
        tutorialLine.start({{"Animus explains", 2}, {"next explanation", 0}}, TutorialMs(5000), tutorialZero);
        check(tutorialMenu.update(snapshot.menuOpen, TutorialMs(500), tutorialZero) == EscapePauseAction::None,
            "tutorial freeze creates neither playback pause nor resume delay");
        tutorialLine.update(tutorialClock.state(tutorialZero + TutorialMs(2000)).now);
        check(tutorialLine.currentText() == "next explanation",
            "Animus segments advance while game time is frozen");
        menuObject[0x5c] = 1;
        countObject[0x24 / 4] = countObject[0x28 / 4] = 2;
        check(probe.sample(snapshot) && snapshot.menuOpen &&
            tutorialMenu.update(snapshot.menuOpen, TutorialMs(500), tutorialZero + TutorialMs(2500)) == EscapePauseAction::Pause,
            "opening an actual menu during a tutorial still pauses subtitles");
        tutorialClock.setPaused(true, tutorialZero + TutorialMs(2500));
        tutorialLine.update(tutorialClock.state(tutorialZero + TutorialMs(60000)).now);
        check(tutorialLine.active(), "menu retains the tutorial subtitle for a long pause");
        menuObject[0x5c] = 0; // Resume by mouse, Escape or controller B uses the same state.
        countObject[0x24 / 4] = countObject[0x28 / 4] = 1;
        check(probe.sample(snapshot) && !snapshot.menuOpen && snapshot.activeCount == 1 &&
            tutorialMenu.update(snapshot.menuOpen, TutorialMs(500), tutorialZero + TutorialMs(60000)) == EscapePauseAction::None && tutorialMenu.pending(),
            "menu exit resumes even though the scripted tutorial freeze remains");
        check(tutorialMenu.update(snapshot.menuOpen, TutorialMs(500), tutorialZero + TutorialMs(60499)) == EscapePauseAction::None,
            "tutorial menu exit preserves the working resume delay");
        check(tutorialMenu.update(snapshot.menuOpen, TutorialMs(500), tutorialZero + TutorialMs(60500)) == EscapePauseAction::Resume,
            "tutorial menu exit resumes at the common delay deadline");
        tutorialClock.setPaused(false, tutorialZero + TutorialMs(60500));
        tutorialLine.update(tutorialClock.state(tutorialZero + TutorialMs(62999)).now);
        check(tutorialLine.active(), "tutorial subtitle retains its exact remaining time after the menu");
        tutorialLine.update(tutorialClock.state(tutorialZero + TutorialMs(63000)).now);
        check(!tutorialLine.active(), "tutorial subtitle expires while scripted freeze still remains");
        countPointer = 0;
        check(probe.sample(snapshot) && !snapshot.countsAvailable && !snapshot.menuOpen,
            "missing general PauseManager does not disable a valid menu sample");
        menuPointer = 0;
        check(probe.sample(snapshot) && !snapshot.menuOpen, "uninitialized menu manager means no menu");
        menuPointer = 1;
        check(!probe.sample(snapshot), "invalid menu object is read safely");
        menuPointer = reinterpret_cast<uintptr_t>(menuObject);
        menuObject[0x5c] = 2;
        check(!probe.sample(snapshot), "invalid menu-state value is rejected");
        menuObject[0x5c] = 0;
        std::memcpy(image.data() + 0x700, image.data() + 0x500, menuCode.size());
        check(!probe.initialize(image.data()), "duplicate menu singleton signature is rejected");
        std::memset(image.data() + 0x700, 0, menuCode.size());
        std::memcpy(image.data() + 0x700, stateCode.data(), stateCode.size());
        check(!probe.initialize(image.data()), "duplicate menu-state signature is rejected");
        std::memset(image.data() + 0x700, 0, stateCode.size());
        std::memcpy(image.data() + 0x500 + 30, &countSlot, sizeof(countSlot));
        check(!probe.initialize(image.data()), "different singleton operands are rejected");

        EscapePauseTracker escape;
        check(escape.update(false, false) == EscapePauseAction::None, "released Escape has no edge");
        check(escape.update(true, false) == EscapePauseAction::Pause, "first Escape press pauses");
        check(escape.update(true, true) == EscapePauseAction::None, "holding Escape does not repeat");
        check(escape.update(false, true) == EscapePauseAction::None, "opening-key release stays paused");
        check(escape.update(true, true) == EscapePauseAction::None, "closing press defers resume");
        check(escape.closingOnRelease(), "manager exit cannot resume while Escape is held");
        check(escape.update(true, true) == EscapePauseAction::None, "held closing key stays deferred");
        check(escape.update(false, true) == EscapePauseAction::Resume, "closing-key release resumes");
        check(!escape.closingOnRelease(), "closing gate clears on release");

        ResumeDelayGate resumeGate;
        const auto gateZero = ResumeDelayGate::Clock::time_point{};
        check(!resumeGate.request(std::chrono::milliseconds(500), gateZero), "configured delay defers release resume");
        check(resumeGate.pending(), "deferred resume remains pending");
        check(!resumeGate.update(gateZero + std::chrono::milliseconds(499)), "delay keeps subtitle frozen");
        check(resumeGate.update(gateZero + std::chrono::milliseconds(500)), "deadline releases subtitle clock");
        check(!resumeGate.pending(), "completed resume clears the gate");
        check(resumeGate.request(std::chrono::milliseconds(0), gateZero), "zero delay resumes immediately");
        check(!resumeGate.pending(), "immediate resume never leaves playback stuck");
        check(!resumeGate.request(std::chrono::milliseconds(500), gateZero), "gate can defer another pause cycle");
        resumeGate.cancel();
        check(!resumeGate.pending(), "new pause cancels a stale resume request");

        MenuPauseTracker menu;
        using Ms = std::chrono::milliseconds;
        check(menu.update(false, Ms(500), gateZero) == EscapePauseAction::None,
            "initial gameplay must not create a resume delay");
        check(menu.update(true, Ms(500), gateZero + Ms(100)) == EscapePauseAction::Pause,
            "mouse or controller menu entry freezes playback");
        check(menu.update(true, Ms(500), gateZero + Ms(200)) == EscapePauseAction::None,
            "nested menu navigation must not resume playback");
        check(menu.update(false, Ms(500), gateZero + Ms(300)) == EscapePauseAction::None && menu.pending(),
            "controller Resume waits for dialogue using the configured delay");
        check(menu.update(false, Ms(500), gateZero + Ms(799)) == EscapePauseAction::None,
            "all input paths retain the same remaining delay");
        check(menu.update(true, Ms(500), gateZero + Ms(800)) == EscapePauseAction::Pause && !menu.pending(),
            "reopening at the deadline cancels a stale resume");
        check(menu.update(false, Ms(500), gateZero + Ms(1000)) == EscapePauseAction::None,
            "new menu exit starts a fresh delay");
        check(menu.update(false, Ms(500), gateZero + Ms(1500)) == EscapePauseAction::Resume,
            "resume succeeds without an Escape key release");
        check(menu.update(false, Ms(500), gateZero + Ms(1600)) == EscapePauseAction::None,
            "resume occurs only once");
        check(menu.update(true, Ms(0), gateZero + Ms(1700)) == EscapePauseAction::Pause &&
            menu.update(false, Ms(0), gateZero + Ms(1800)) == EscapePauseAction::Resume,
            "zero-delay configuration also handles controller Resume");

        PlaybackClock clock;
        SubtitleRuntime runtime;
        const auto zero = PlaybackClock::Clock::time_point{};
        using namespace std::chrono;
        runtime.start({{"first", 2}, {"second", 0}}, seconds(5), clock.state(zero).now);
        clock.setPaused(true, zero+seconds(1));
        check(clock.paused(), "fast callback state reports pause");
        check(!playbackSubtitleVisible(runtime.active(), clock.paused()), "active subtitle hidden in menu");
        check(!clock.setPaused(true, zero+seconds(20)), "repeated pause must not move the pause boundary");
        runtime.update(clock.state(zero+seconds(100)).now);
        check(runtime.currentText()=="first", "long pause keeps the current segment");
        clock.setPaused(false, zero+seconds(101));
        check(!clock.paused(), "fast callback state reports resume");
        check(playbackSubtitleVisible(runtime.active(), clock.paused()), "same active subtitle returns after menu");
        runtime.update(clock.state(zero+milliseconds(101999)).now);
        check(runtime.currentText()=="first", "remaining segment time preserved");
        runtime.update(clock.state(zero+seconds(102)).now);
        check(runtime.currentText()=="second", "segment resumes at exact deadline");
        clock.setPaused(true,zero+seconds(103)); clock.setPaused(false,zero+seconds(153));
        runtime.update(clock.state(zero+seconds(154)).now);
        check(runtime.active(), "second pause extends total deadline");
        runtime.update(clock.state(zero+seconds(155)).now);
        check(!runtime.active(), "total duration expires after active time only");

        check(MH_Initialize()==MH_OK,"initialize MinHook");
        uint8_t target[16]{};
        uint32_t state[80]{}; state[0x108/4]=1; state[108/4]=0;
        Xbyak::CodeGenerator pause, resume;
        for (unsigned playing=0;playing<2;++playing) {
            auto& c=playing?resume:pause;
            c.push(c.edi); c.push(c.esi);
            c.mov(c.edi,reinterpret_cast<uintptr_t>(target));
            c.mov(c.esi,reinterpret_cast<uintptr_t>(state));
            const auto site=reinterpret_cast<uintptr_t>(c.getCurr());
            c.mov(c.byte[c.edi+8],playing); c.cmp(c.dword[c.esi+0x108],0);
            c.setne(c.al); c.movzx(c.eax,c.al); c.pop(c.esi); c.pop(c.edi); c.ret(); c.ready();
            check(PauseHook::Install(site,playing),"install actual pause/resume detour");
            check(c.getCode<unsigned(*)()>()()==1,"preserve original 0x108 comparison and flags");
            check(target[8]==playing,"preserve original game state write");
            check(g_playbackClock.state().paused==(playing==0),"observe real state transition");
        }
        MH_DisableHook(MH_ALL_HOOKS); MH_Uninitialize();
        std::cout<<"PASS: release-gated resume, repeated pauses, deadline preservation, native x86 state and flags\n";
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n'; return 1;}
}
