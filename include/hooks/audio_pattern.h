#pragma once
#include <string_view>
namespace AudioHook {
// Include the result use and argument cleanup: the old CALL-only pattern
// also matched an unrelated routine earlier in the DX10 image.
inline constexpr std::string_view Pattern =
    "E8 ?? ?? ?? ?? 8B 48 08 6A 01 C1 E6 04 8B 34 0E 6A 01 56 E8 ?? ?? ?? ?? 8B F0 8B 44 24 18 83 C4 0C 85 C0 C6 44 24 18 00 74 14";
inline constexpr unsigned CallOffset = 19;
// Leaf voice start: the only CALL into the per-voice play routine inside the
// event-type dispatcher (DX10 0xc4db76). The hook sits on the ADD ESP after it
// (0xc4db7b): EAX = result, [ESP+0x0C] = &leaf event handle (arg 4),
// [ESP+0x24] = Instance** (arg 10).
inline constexpr std::string_view LeafPattern = "D9 1C 24 56 53 57 51 E8 ?? ?? ?? ?? 83 C4 2C E9";
inline constexpr unsigned LeafReturnOffset = 12;
// GetMic0Pos (DX10 0xc840e0): MOV ECX,[head]; the absolute head address is at +2.
inline constexpr std::string_view MicPattern =
    "8B 0D ?? ?? ?? ?? 8B 11 8B 44 24 04 89 10 8B 51 04 8B 49 08 89 50 04 89 48 08 C3";
inline constexpr unsigned MicHeadOffset = 2;
}
