#include "hooks/asm_hooks.h"
#include "audio_system.h"
#include "playback_clock.h"
#include <Windows.h>
#include <MinHook.h>
#include <xbyak/xbyak.h>
#include <cmath>
#include <cstring>
#include <iostream>

int main() {
    // Real x86 execution: CALL relocation, original side effect/result, repeated
    // captures, and caller's ESI survive the actual production detour.
    unsigned calls = 0;
    uint32_t action[] = {0x00a80305, 1, 2};
    uint32_t table[12]{};
    table[4] = 0x10a80305; table[5] = reinterpret_cast<uintptr_t>(action);
    table[8] = 0x20a802f9;
    uint32_t manager[] = {3, 3, reinterpret_cast<uintptr_t>(table)};
    Xbyak::CodeGenerator callee;
    callee.mov(callee.eax, reinterpret_cast<uintptr_t>(&calls));
    callee.inc(callee.dword[callee.eax]);
    callee.mov(callee.eax, 0x12345678);
    callee.ret();
    callee.ready();
    Xbyak::CodeGenerator caller;
    caller.push(caller.esi);
    // Match the production call site's scoped-handle slot at ESP+0x18.
    caller.sub(caller.esp, 16);
    caller.mov(caller.dword[caller.esp + 12], 1);
    caller.mov(caller.esi, 0x10a80305);
    caller.mov(caller.eax, reinterpret_cast<uintptr_t>(manager));
    caller.push(1); caller.push(1); caller.push(caller.esi);
    auto site = reinterpret_cast<uintptr_t>(caller.getCurr());
    caller.call(callee.getCode());
    caller.add(caller.esp, 12);
    caller.cmp(caller.esi, 0x10a80305);
    caller.je("good");
    caller.xor_(caller.eax, caller.eax);
    caller.L("good");
    caller.add(caller.esp, 16);
    caller.pop(caller.esi);
    caller.ret();
    caller.ready();
    auto run = caller.getCode<unsigned(*)()>();
    if (run() != 0x12345678 || calls != 1) return 1;
    if (MH_Initialize() != MH_OK || !AudioHook::Install(site)) return 2;
    AudioEvent event;
    g_playbackClock.setPaused(true);
    if (run() != 0x12345678 || calls != 2 || !g_AudioQueue.pop(event) ||
        event.id != 0x20a802f9 || event.rawId != 0x10a80305) return 3;
    g_playbackClock.setPaused(false);
    if (run() != 0x12345678 || run() != 0x12345678 || calls != 4) return 4;
    for (int i=0; i<2; ++i)
        if (!g_AudioQueue.pop(event) || event.id != 0x20a802f9 || event.rawId != 0x10a80305 ||
            event.resolution != Resolution::DirectReference || !event.threadId) return 5;
    if (g_AudioQueue.pop(event)) return 6;
    // Leaf voice-start site: the hook sits on the ADD ESP right after the play
    // CALL; arg 4 = &leaf handle, arg 10 = Instance** (11 cdecl arguments).
    uint32_t leafHandle = 1;
    alignas(16) uint8_t instance[0x140]{};
    uint32_t instancePtr = reinterpret_cast<uintptr_t>(instance);
    auto put = [&](size_t offset, const void* value, size_t size) { std::memcpy(instance + offset, value, size); };
    const uint32_t emitterId = 0x42, flags3d = 4;
    const float emitterPos[3] = {10, 0, 0}, curve[4] = {1, -1, 1, 20};
    put(8, &emitterId, 4); put(0xa0, &flags3d, 4); put(0x88, emitterPos, 12); put(0x118, curve, 16);
    alignas(16) uint8_t mic[0x44]{};  // listener at the origin, data present
    uint32_t micHeadSlot = reinterpret_cast<uintptr_t>(mic);
    Xbyak::CodeGenerator leafCaller;
    leafCaller.push(leafCaller.esi);
    leafCaller.mov(leafCaller.esi, reinterpret_cast<uintptr_t>(&leafHandle));
    leafCaller.push(0);                                              // arg 11
    leafCaller.push(reinterpret_cast<uintptr_t>(&instancePtr));      // arg 10
    for (int i = 0; i < 5; ++i) leafCaller.push(0);                  // args 9..5
    leafCaller.push(leafCaller.esi);                                 // arg 4
    leafCaller.push(0); leafCaller.push(0); leafCaller.push(0);      // args 3..1
    leafCaller.call(callee.getCode());
    auto leafSite = reinterpret_cast<uintptr_t>(leafCaller.getCurr());
    leafCaller.add(leafCaller.esp, 44);
    leafCaller.jmp("leafcheck", Xbyak::CodeGenerator::T_NEAR);
    leafCaller.db(0xcc);
    leafCaller.L("leafcheck");
    leafCaller.cmp(leafCaller.esi, reinterpret_cast<uintptr_t>(&leafHandle));
    leafCaller.je("leafgood");
    leafCaller.xor_(leafCaller.eax, leafCaller.eax);
    leafCaller.L("leafgood");
    leafCaller.pop(leafCaller.esi);
    leafCaller.ret();
    leafCaller.ready();
    auto runLeaf = leafCaller.getCode<unsigned(*)()>();
    if (!AudioHook::InstallLeaf(leafSite, reinterpret_cast<uintptr_t>(&micHeadSlot)) ||
        !AudioHook::leafActive()) return 8;
    // Every voice start is reported, with the emitter's level for the listener.
    if (runLeaf() != 0x12345678 || calls != 5 || !g_AudioQueue.pop(event) ||
        event.id != 0x20a802f9 || event.rawId != 0x10a80305 ||
        event.resolution != Resolution::LeafVoice || !event.levelKnown || !event.positional ||
        std::fabs(event.distance - 10) > 0.01f || std::fabs(event.levelDb + 5.58f) > 0.05f ||
        event.emitter != 0x42 || g_AudioQueue.pop(event)) return 9;
    const uint32_t flags2d = 1;
    put(0xa0, &flags2d, 4);
    if (runLeaf() != 0x12345678 || !g_AudioQueue.pop(event) || !event.levelKnown ||
        event.positional || event.levelDb != 0.0f) return 10;
    instancePtr = 0;  // started without an instance: level unknown, still reported
    if (runLeaf() != 0x12345678 || !g_AudioQueue.pop(event) || event.levelKnown) return 11;
    leafHandle = 2; // a resource handle is not a leaf event
    if (runLeaf() != 0x12345678 || g_AudioQueue.pop(event)) return 12;
    MH_DisableHook(reinterpret_cast<void*>(site));
    MH_DisableHook(reinterpret_cast<void*>(leafSite));
    MH_Uninitialize();
    if (run() != 0x12345678 || calls != 9) return 7;
    std::cout << "PASS: native x86 detour preserves CALL/result/ESI, captures pause-boundary dialogue, repeats events and leaf voice starts\n";
}
