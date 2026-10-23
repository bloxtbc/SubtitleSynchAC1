#include "pattern_scan.h"
#include "diagnostics.h"
#include <Windows.h>
#include <algorithm>

uintptr_t PatternScan::Find(void* module, std::string_view pattern) {
    if (!module) return 0;
    auto* image = static_cast<uint8_t*>(module);
    auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(image);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return 0;
    auto* nt = reinterpret_cast<IMAGE_NT_HEADERS*>(image + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return 0;
    auto* section = IMAGE_FIRST_SECTION(nt);
    size_t count = 0;
    uintptr_t candidate = 0;
    for (unsigned i = 0; i < nt->FileHeader.NumberOfSections; ++i) {
        const auto& s = section[i];
        if (!(s.Characteristics & IMAGE_SCN_MEM_EXECUTE)) continue;
        if (s.VirtualAddress >= nt->OptionalHeader.SizeOfImage) continue;
        const size_t size = std::min(s.Misc.VirtualSize, nt->OptionalHeader.SizeOfImage - s.VirtualAddress);
        for (auto offset : FindOffsets(image + s.VirtualAddress, size, pattern)) {
            candidate = reinterpret_cast<uintptr_t>(image + s.VirtualAddress + offset);
            ++count;
            Diagnostics::log("hook_candidate address=%p rva=%08lx", reinterpret_cast<void*>(candidate),
                static_cast<unsigned long>(s.VirtualAddress + offset));
        }
    }
    Diagnostics::log("hook_scan matches=%zu", count);
    return count == 1 ? candidate : 0;
}
uintptr_t PatternScan::Find(std::string_view pattern) { return Find(GetModuleHandleA(nullptr), pattern); }
