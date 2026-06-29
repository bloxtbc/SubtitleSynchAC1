#include <hooks/asm_hooks.h>
#include <audio_system.h>
#include <xbyak/xbyak.h>
#include <cstddef>
#include <memory>

#include "patcher.h"
#include "globals.h"

bool gamePaused;

void __stdcall PausedFalse() { 
    gamePaused = false;
}

void __stdcall PausedTrue() { 
    gamePaused = true;
}

void PushAudioEvent(uintptr_t evt) {
    g_AudioQueue.push(evt);
}

template <typename Func>
MidHook MakeMidHook(uintptr_t address, size_t overwriteSize, Func&& builder)
{
    MidHook hook{};
    hook.address = address;
    hook.size = overwriteSize;
    hook.returnAddress = address + overwriteSize;

    if (overwriteSize < 5) // jmp + address needs 5 bytes
        return hook;

    auto code = std::make_unique<Xbyak::CodeGenerator>();

    MidHook tempHook{};
    tempHook.address = address;
    tempHook.size = overwriteSize;
    tempHook.returnAddress = address + overwriteSize;

    builder(*code, tempHook);

    uintptr_t target = (uintptr_t)code->getCode();
    uintptr_t rel = target - (address + 5);

    uint8_t jmp[5];
    jmp[0] = 0xE9;
    *(uint32_t*)&jmp[1] = (uint32_t)rel;
    // 0xE9 (jmp) to 0x00000000 (address 4bytes)

    PatchBytes(address, jmp, 5);

    if (overwriteSize > 5)
        Nop(address + 5, overwriteSize - 5);

    hook.code = std::move(code);
    return hook;
}

MidHook Install(const HookDef& def)
{
    return MakeMidHook(def.address, def.size, def.builder);
}

MidHook unpauseHook;
MidHook pauseHook;
MidHook dareHook;

void applyASMPatches()
{
    //dx9: 0x00C77B69
    //dx10: 0x00E4BB29
    unpauseHook = Install({
        ResolveAddr(0x00C77B69, 0x00E4BB29),
        11,
        [](Xbyak::CodeGenerator& c, MidHook& hook)
        {
            using namespace Xbyak;

            c.db(0x60); // pushad
            c.db(0x9C); // pushfd

            c.mov(c.eax, (uintptr_t)&PausedTrue);
            c.call(c.eax);

            c.db(0x9D); // popfd
            c.db(0x61); // popad

            c.mov(c.byte[c.edi + 8], 1);
            c.cmp(c.dword[c.esi + 108], 0);

            c.mov(c.eax, hook.returnAddress);
            c.jmp(c.eax);
        }
    });

    //dx9: 0x00C774C5
    //dx10: 0x00E4B485
    pauseHook = Install({
        ResolveAddr(0x00C774C5, 0x00E4B485),
        11,
        [](Xbyak::CodeGenerator& c, MidHook& hook)
        {
            using namespace Xbyak;

            c.db(0x60); // pushad
            c.db(0x9C); // pushfd

            c.mov(c.eax, (uintptr_t)&PausedFalse);
            c.call(c.eax);

            c.db(0x9D); // popfd
            c.db(0x61); // popad

            c.mov(c.byte[c.edi + 8], 0);
            c.cmp(c.dword[c.esi + 108], 0);

            c.mov(c.eax, hook.returnAddress);
            c.jmp(c.eax);
        }
    });

    //dx9: 0x007E8555
    //dx10: 0x00C56625
    dareHook = Install({
        ResolveAddr(0x007E8555, 0x00C56625),
        5,
        [](Xbyak::CodeGenerator& c, MidHook& hook)
        {
            using namespace Xbyak;

            c.db(0x60); // pushad
            c.db(0x9C); // pushfd

            c.push(c.esi);

            c.mov(c.eax, (uintptr_t)&PushAudioEvent);
            c.call(c.eax);

            c.add(c.esp, 4);

            c.db(0x9D); // popfd
            c.db(0x61); // popad

            c.mov(c.eax, hook.returnAddress);
            c.jmp(c.eax);
        }
    });
}