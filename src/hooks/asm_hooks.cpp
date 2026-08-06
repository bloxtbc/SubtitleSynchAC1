#include <hooks/asm_hooks.h>
#include <audio_system.h>
#include <xbyak/xbyak.h>
#include <cstddef>
#include <cstring>
#include <memory>

#include "patcher.h"
#include "globals.h"

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
    hook.overwrittenBytes.resize(overwriteSize);
    std::memcpy(hook.overwrittenBytes.data(), reinterpret_cast<const void*>(address), overwriteSize);

    if (overwriteSize < 5) // jmp + address needs 5 bytes
        return hook;

    auto code = std::make_unique<Xbyak::CodeGenerator>();

    builder(*code, hook);

    uintptr_t target = reinterpret_cast<uintptr_t>(code->getCode());
    uintptr_t rel = target - (address + 5);

    uint8_t jmp[5];
    jmp[0] = 0xE9;
    *(uint32_t*)&jmp[1] = static_cast<uint32_t>(rel);
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

            c.mov(c.byte[c.edi + 8], 1);
            c.cmp(c.dword[c.esi + 108], 0);
            c.jmp((const void*)hook.returnAddress);
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

            c.mov(c.byte[c.edi + 8], 0);
            c.cmp(c.dword[c.esi + 108], 0);
            c.jmp((const void*)hook.returnAddress);
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

            c.jmp((const void*)hook.returnAddress);
        }
    });
}