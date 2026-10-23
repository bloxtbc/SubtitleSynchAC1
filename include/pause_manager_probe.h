#pragma once

#include <cstdint>
#include <string_view>

struct PauseManagerSnapshot {
    uintptr_t object = 0;
    uint32_t activeCount = 0;
    uint32_t committedCount = 0;
    bool countsAvailable = false;
    uintptr_t menuObject = 0;
    bool menuOpen = false;
};

class PauseManagerProbe {
public:
    // Matches the supported DX9/DX10 PauseManager initialization. The first MOV
    // contains the relocated address of the singleton pointer slot.
    inline static constexpr std::string_view Pattern =
        "A1 ?? ?? ?? ?? 83 EC 0C 85 C0 0F 85 B8 00 00 00 "
        "A1 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 8B 09 8B 11 F7 D8 1B C0 25 ?? ?? ?? ?? 56 50";

    // AssassinMenuManager singleton registration and its PrePauseEvent state
    // handler. Scripted FreezeTimeClip/tutorial requests do not set this flag.
    inline static constexpr std::string_view MenuPattern =
        "83 3D ?? ?? ?? ?? 00 75 29 68 7A B1 D7 F8 68 36 08 00 00 "
        "E8 ?? ?? ?? ?? 83 C4 08 8B C8 A3 ?? ?? ?? ?? E8 ?? ?? ?? ?? "
        "8B 0D ?? ?? ?? ?? E9";
    inline static constexpr std::string_view MenuStatePattern =
        "C6 46 5C 01 5E C2 04 00 C6 46 5C 00 E8 ?? ?? ?? ?? "
        "6A 00 6A 00 6A 01 8B C8 E8 ?? ?? ?? ?? 5E C2 04 00";

    bool initialize(void* module = nullptr);
    bool sample(PauseManagerSnapshot& snapshot) const;
    uintptr_t pointerSlot() const { return m_pointerSlot; }
    uintptr_t menuPointerSlot() const { return m_menuPointerSlot; }

private:
    uintptr_t m_pointerSlot = 0;
    uintptr_t m_menuPointerSlot = 0;
};
