#include "hooks/asm_hooks.h"
#include "playback_clock.h"
#include "pattern_scan.h"
#include "diagnostics.h"
#include "audio_system.h"
#include <Windows.h>
#include <MinHook.h>
#include <xbyak/xbyak.h>
#include <memory>

namespace {
std::unique_ptr<Xbyak::CodeGenerator> stubs[2];
void __cdecl observePause(unsigned playing) noexcept {
    const bool paused = playing == 0;
    if (!g_playbackClock.setPaused(paused)) return;
    // Clear events captured around both transition boundaries. The current
    // subtitle remains in SubtitleRuntime and its logical deadline is frozen.
    const auto discarded = g_AudioQueue.discardAll();
    Diagnostics::log("playback_pause paused=%u queue_discarded=%zu", paused, discarded);
}
}

bool PauseHook::Install(uintptr_t address, unsigned playing) {
    if (playing > 1) return false;
    auto code = std::make_unique<Xbyak::CodeGenerator>(1024);
    void* trampoline = nullptr;
    if (MH_CreateHook(reinterpret_cast<void*>(address), const_cast<uint8_t*>(code->getCode()), &trampoline) != MH_OK)
        return false;
    try {
        auto& c = *code;
        c.pushad(); c.pushfd();
        c.mov(c.ebp, c.esp); c.and_(c.esp, -16); c.sub(c.esp, 528);
        const uint8_t save[] = {0x0f,0xae,0x44,0x24,0x10};
        c.db(save, sizeof(save)); c.cld();
        c.mov(c.dword[c.esp], playing);
        c.mov(c.eax, reinterpret_cast<uintptr_t>(&observePause)); c.call(c.eax);
        c.fxrstor(c.ptr[c.esp+16]);
        c.mov(c.esp,c.ebp); c.popfd(); c.popad();
        // Replay the original MOV and CMP [ESI+0x108] with their flags intact.
        c.jmp(trampoline); c.ready();
        FlushInstructionCache(GetCurrentProcess(), c.getCode(), c.getSize());
        if (MH_EnableHook(reinterpret_cast<void*>(address)) != MH_OK) {
            MH_RemoveHook(reinterpret_cast<void*>(address)); return false;
        }
        stubs[playing] = std::move(code);
        return true;
    } catch (...) {
        MH_RemoveHook(reinterpret_cast<void*>(address)); return false;
    }
}

bool PauseHook::InstallGameHooks() {
    // Shared call/result handling on both sides, not a keypress heuristic.
    const auto pause = PatternScan::Find("E8 ?? ?? ?? ?? 8B F8 C6 47 08 00 83 BE 08 01 00 00 00 0F 95 C0 88 47 09 E8 ?? ?? ?? ?? 50 8B CF");
    const auto resume = PatternScan::Find("E8 ?? ?? ?? ?? 8B F8 C6 47 08 01 83 BE 08 01 00 00 00 0F 95 C0 88 47 09 E8 ?? ?? ?? ?? 50 8B CF");
    if (!pause || !resume) {
        Diagnostics::error("pause/resume patterns are not unique; pause tracking unavailable"); return false;
    }
    if (!Install(pause + 7, 0)) return false;
    if (!Install(resume + 7, 1)) {
        MH_DisableHook(reinterpret_cast<void*>(pause + 7));
        MH_RemoveHook(reinterpret_cast<void*>(pause + 7));
        stubs[0].reset();
        g_playbackClock.setPaused(false);
        return false;
    }
    Diagnostics::log("pause_hooks enabled pause=%p resume=%p", reinterpret_cast<void*>(pause+7), reinterpret_cast<void*>(resume+7));
    return true;
}
