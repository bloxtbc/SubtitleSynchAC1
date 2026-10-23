#pragma once

#include <cstdint>
#include <string_view>

namespace PressPromptHook {
// The string wrapper resolves MSVC's small-string storage, then dispatches
// a nonempty UTF-16 span to font virtual +0x20. Executable sections only.
inline constexpr std::string_view Pattern =
    "8B 44 24 04 83 78 18 08 72 05 8B 50 04 EB 03 8D 50 04 "
    "0F BF 44 24 2C 8B 4C 24 08 56 57 0F BF 7C 24 38 2B F8 "
    "57 0F BF 7C 24 34 8B 31 89 7C 24 38";
bool initialize();
bool install(uintptr_t address);
bool visible(uint64_t now);
uintptr_t address();
}
