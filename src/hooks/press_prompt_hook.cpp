#include "hooks/press_prompt_hook.h"
#include "press_prompt_detector.h"
#include "pattern_scan.h"
#include "diagnostics.h"

#include <Windows.h>
#include <MinHook.h>
#include <xbyak/xbyak.h>
#include <memory>

namespace {
InteractionPromptTracker tracker;
std::unique_ptr<Xbyak::CodeGenerator> stub;
uintptr_t installedAddress = 0;

bool readMemory(uintptr_t address, void* output, size_t size) {
    SIZE_T copied = 0;
    return address && ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<const void*>(address),
        output, size, &copied) && copied == size;
}

void __cdecl observeRenderedText(uintptr_t stringObject, int32_t start, int32_t end) noexcept {
    // AC1's MSVC UTF-16 string: proxy +0, inline storage/pointer +4,
    // length +0x14, capacity +0x18. Never scan beyond its bounded length.
    if (start < 0 || end <= start) return;
    uint32_t words[7]{};
    if (!readMemory(stringObject, words, sizeof(words))) return;
    const auto length = words[5], capacity = words[6];
    if (length < 24 || length > 256 || capacity < length) return;
    const uintptr_t chars = capacity >= 8 ? words[1] : stringObject + 4;
    wchar_t text[256]{};
    if (!readMemory(chars, text, length * sizeof(wchar_t))) return;
    try {
        tracker.observe(std::wstring_view(text, length), GetTickCount64());
    } catch (...) { /* Observation must never interrupt the game's draw call. */ }
}
}

bool PressPromptHook::initialize() {
    const auto target = PatternScan::Find(Pattern);
    if (!target) {
        Diagnostics::error("interaction prompt observer unavailable: expected one font draw signature; automatic prompt lift disabled");
        return false;
    }
    return install(target);
}

bool PressPromptHook::install(uintptr_t target) {
    if (installedAddress) return installedAddress == target;
    try {
        auto code = std::make_unique<Xbyak::CodeGenerator>(1024);
        void* trampoline = nullptr;
        auto status = MH_CreateHook(reinterpret_cast<void*>(target),
            const_cast<uint8_t*>(code->getCode()), &trampoline);
        if (status != MH_OK) {
            Diagnostics::log("interaction_prompt_hook create_status=%d", static_cast<int>(status));
            return false;
        }
        try {
            auto& c = *code;
            c.pushad(); c.pushfd();
            c.mov(c.ebp, c.esp);
            c.and_(c.esp, -16);
            c.sub(c.esp, 528);
            const uint8_t save[] = {0x0F, 0xAE, 0x44, 0x24, 0x10};
            c.db(save, sizeof(save));
            c.cld();
            // Entry ESP+4 is the first cdecl argument. PUSHAD/PUSHFD add 36.
            c.mov(c.eax, c.dword[c.ebp + 40]);
            c.mov(c.dword[c.esp], c.eax);
            // The original wrapper draws [start, end), both signed 16-bit
            // indices in cdecl slots 11/12. Empty spans are not visible UI.
            c.movsx(c.eax, c.word[c.ebp + 80]);
            c.mov(c.dword[c.esp + 4], c.eax);
            c.movsx(c.eax, c.word[c.ebp + 84]);
            c.mov(c.dword[c.esp + 8], c.eax);
            c.mov(c.eax, reinterpret_cast<uintptr_t>(&observeRenderedText));
            c.call(c.eax);
            c.fxrstor(c.ptr[c.esp + 16]);
            c.mov(c.esp, c.ebp);
            c.popfd(); c.popad();
            c.jmp(trampoline);
            c.ready();
            FlushInstructionCache(GetCurrentProcess(), c.getCode(), c.getSize());
        } catch (...) {
            MH_RemoveHook(reinterpret_cast<void*>(target));
            throw;
        }
        status = MH_EnableHook(reinterpret_cast<void*>(target));
        if (status != MH_OK) {
            MH_RemoveHook(reinterpret_cast<void*>(target));
            return false;
        }
        stub = std::move(code);
        installedAddress = target;
        Diagnostics::log("interaction_prompt_hook enabled address=%p source=rendered-ui-text",
            reinterpret_cast<void*>(target));
        return true;
    } catch (const std::exception& e) {
        Diagnostics::error(e.what());
        return false;
    }
}

bool PressPromptHook::visible(uint64_t now) { return installedAddress && tracker.visible(now); }
uintptr_t PressPromptHook::address() { return installedAddress; }
