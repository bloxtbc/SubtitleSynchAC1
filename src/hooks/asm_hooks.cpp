#include "hooks/asm_hooks.h"
#include "hooks/audio_pattern.h"
#include "audio_system.h"
#include "diagnostics.h"
#include "pattern_scan.h"
#include "playback_clock.h"
#include <Windows.h>
#include <MinHook.h>
#include <xbyak/xbyak.h>
#include <atomic>
#include <memory>

namespace {
std::unique_ptr<Xbyak::CodeGenerator> audioStub, leafStub;
bool ReadGameAudio(uintptr_t address, void* output, size_t size) {
    SIZE_T copied = 0;
    return address && ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(address),
        output, size, &copied) && copied == size;
}
// Manager seen by the root hook; the leaf site runs later in the same Play call.
std::atomic<uintptr_t> lastManager{0};
void __cdecl PushAudioEvent(uint32_t rawId, uintptr_t manager, uint32_t handle) noexcept {
    // Capture events during pause as well. The render thread can safely check
    // them against the loaded subtitle database, retain actual dialogue, and
    // discard menu/UI sounds without losing a line that starts at the boundary.
    if (manager) lastManager.store(manager, std::memory_order_relaxed);
    const auto audio = resolveAudio(rawId, manager, handle, &ReadGameAudio);
    g_AudioQueue.push({audio.id, GetCurrentThreadId(), GetTickCount64(), rawId, audio.resolution});
}
uintptr_t micHead = 0;
bool leafInstalled = false;
void __cdecl PushLeafAudio(uintptr_t handleSlot, uintptr_t instanceSlot, uint32_t result) noexcept {
    // The play routine returns 0 when no voice started.
    if (!result) return;
    uint32_t handle = 0, leafEvent = 0, instance = 0;
    if (!ReadGameAudio(handleSlot, &handle, sizeof handle)) return;
    const auto audio = resolveLeafAudio(lastManager.load(std::memory_order_relaxed), handle,
        leafEvent, &ReadGameAudio);
    if (!audio.id) return;
    // Still under the engine's sound lock: copy the instance values now.
    VoiceLevel level;
    if (ReadGameAudio(instanceSlot, &instance, sizeof instance) && instance)
        level = readVoiceLevel(instance, micHead, &ReadGameAudio);
    AudioEvent event{audio.id, GetCurrentThreadId(), GetTickCount64(), leafEvent, audio.resolution};
    event.levelKnown = level.known;
    event.positional = level.positional;
    event.distance = level.distance;
    event.levelDb = level.levelDb;
    event.emitter = level.emitter;
    event.instance = instance;
    g_AudioQueue.push(event);
}
}

bool applyASMPatches() {
    // Pause tracking is installed separately, preserving the game's original
    // 0x108 displacement through MinHook relocation.
    const auto address = PatternScan::Find(AudioHook::Pattern);
    if (!address) {
        Diagnostics::error("audio hook requires exactly one executable-section match; subtitles disabled");
        return false;
    }
    if (!AudioHook::Install(address + AudioHook::CallOffset)) return false;
    // Optional: without it only direct events resolve, as before.
    const auto leaf = PatternScan::Find(AudioHook::LeafPattern);
    uintptr_t micHeadSlot = 0;
    if (const auto mic = PatternScan::Find(AudioHook::MicPattern))
        micHeadSlot = *reinterpret_cast<const uint32_t*>(mic + AudioHook::MicHeadOffset);
    else
        Diagnostics::log("microphone_probe unavailable; secondary distance stays unknown");
    if (!leaf || !AudioHook::InstallLeaf(leaf + AudioHook::LeafReturnOffset, micHeadSlot))
        Diagnostics::log("leaf_hook unavailable; switch/multi events stay unresolved");
    return true;
}

namespace {
// Detour one instruction: save all state, run emitArgs + call target,
// restore, then continue into the MinHook-relocated original instructions.
template <class EmitArgs>
bool installCallDetour(uintptr_t address, const void* target, EmitArgs emitArgs,
                       std::unique_ptr<Xbyak::CodeGenerator>& keep, const char* name) {
    try {
        auto code = std::make_unique<Xbyak::CodeGenerator>(1024);
        void* trampoline = nullptr;
        auto status = MH_CreateHook(reinterpret_cast<void*>(address),
            const_cast<uint8_t*>(code->getCode()), &trampoline);
        if (status != MH_OK) {
            Diagnostics::log("hook_create name=%s status=%d", name, static_cast<int>(status));
            Diagnostics::error("audio hook creation failed");
            return false;
        }
        try {
            auto& c = *code;
            c.pushad();
            c.pushfd();
            c.mov(c.ebp, c.esp);
            c.and_(c.esp, -16);
            c.sub(c.esp, 528);
            // FXSAVE [ESP+16] (not exposed by the pinned Xbyak version).
            const uint8_t save[] = {0x0F, 0xAE, 0x44, 0x24, 0x10};
            c.db(save, sizeof(save));
            c.cld();
            emitArgs(c);
            c.mov(c.eax, reinterpret_cast<uintptr_t>(target));
            c.call(c.eax);
            c.fxrstor(c.ptr[c.esp + 16]);
            c.mov(c.esp, c.ebp);
            c.popfd();
            c.popad();
            // MinHook relocates the original CALL; its EAX and side effects
            // must reach the game's following instructions unchanged.
            c.jmp(trampoline);
            c.ready();
            FlushInstructionCache(GetCurrentProcess(), c.getCode(), c.getSize());
        } catch (...) {
            MH_RemoveHook(reinterpret_cast<void*>(address));
            throw;
        }
        status = MH_EnableHook(reinterpret_cast<void*>(address));
        if (status != MH_OK) {
            MH_RemoveHook(reinterpret_cast<void*>(address));
            Diagnostics::error("audio hook enable failed");
            return false;
        }
        keep = std::move(code);
        Diagnostics::log("hook_enabled name=%s address=%p trampoline=%p", name,
            reinterpret_cast<void*>(address), trampoline);
        return true;
    } catch (const std::exception& e) {
        Diagnostics::error(e.what());
        return false;
    }
}
}

bool AudioHook::Install(uintptr_t address) {
    return installCallDetour(address, reinterpret_cast<const void*>(&PushAudioEvent),
        [](Xbyak::CodeGenerator& c) {
            c.mov(c.dword[c.esp], c.esi);
            // At this exact CALL site EAX is the resource manager, and the
            // scoped event handle is at original ESP+0x18 (PUSHAD/PUSHFD: +36).
            c.mov(c.dword[c.esp + 4], c.eax);
            c.mov(c.edx, c.dword[c.ebp + 0x3c]);
            c.mov(c.dword[c.esp + 8], c.edx);
        }, audioStub, "audio");
}

bool AudioHook::InstallLeaf(uintptr_t address, uintptr_t micHeadSlot) {
    micHead = micHeadSlot;
    // Runs after the play CALL returned, before its arguments are popped.
    // Original ESP = EBP + 36 (PUSHAD/PUSHFD).
    leafInstalled = installCallDetour(address, reinterpret_cast<const void*>(&PushLeafAudio),
        [](Xbyak::CodeGenerator& c) {
            c.mov(c.dword[c.esp + 8], c.eax);
            c.mov(c.edx, c.dword[c.ebp + 36 + 0x0c]);
            c.mov(c.dword[c.esp], c.edx);
            c.mov(c.edx, c.dword[c.ebp + 36 + 0x24]);
            c.mov(c.dword[c.esp + 4], c.edx);
        }, leafStub, "leaf");
    if (leafInstalled) Diagnostics::log("microphone_head slot=%p", reinterpret_cast<void*>(micHeadSlot));
    return leafInstalled;
}

bool AudioHook::leafActive() { return leafInstalled; }

VoiceLevel AudioHook::readVoiceLevelNow(uintptr_t instance) {
    return readVoiceLevel(instance, micHead, &ReadGameAudio);
}
