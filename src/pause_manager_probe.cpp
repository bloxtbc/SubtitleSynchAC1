#include "pause_manager_probe.h"

#include "diagnostics.h"
#include "pattern_scan.h"

#include <Windows.h>
#include <cstring>

namespace {
bool readMemory(uintptr_t address, void* output, size_t size) {
    SIZE_T copied = 0;
    return address && ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<const void*>(address),
        output, size, &copied) && copied == size;
}
}

bool PauseManagerProbe::initialize(void* module) {
    m_pointerSlot = 0;
    m_menuPointerSlot = 0;
    if (!module) module = GetModuleHandleA(nullptr);
    static_assert(sizeof(uintptr_t) == 4, "AC1 probe requires the x86 build");
    // General game-time pause counts remain useful diagnostics, but cannot
    // distinguish a tutorial freeze (speech continues) from an actual menu.
    const auto instruction = PatternScan::Find(module, Pattern);
    if (instruction)
        std::memcpy(&m_pointerSlot, reinterpret_cast<const void*>(instruction + 1), sizeof(m_pointerSlot));

    const auto menu = PatternScan::Find(module, MenuPattern);
    const auto state = PatternScan::Find(module, MenuStatePattern);
    if (!menu || !state) {
        Diagnostics::log("menu_pause_probe unavailable reason=signature");
        return false;
    }
    uintptr_t checkedSlot = 0, storedSlot = 0, loadedSlot = 0;
    std::memcpy(&checkedSlot, reinterpret_cast<const void*>(menu + 2), sizeof(checkedSlot));
    std::memcpy(&storedSlot, reinterpret_cast<const void*>(menu + 30), sizeof(storedSlot));
    std::memcpy(&loadedSlot, reinterpret_cast<const void*>(menu + 41), sizeof(loadedSlot));
    if (!checkedSlot || checkedSlot != storedSlot || checkedSlot != loadedSlot) {
        Diagnostics::log("menu_pause_probe unavailable reason=inconsistent-pointer-slots");
        return false;
    }
    m_menuPointerSlot = checkedSlot;

    Diagnostics::log("menu_pause_probe enabled instruction=%p state_handler=%p menu_pointer_slot=%p pause_pointer_slot=%p",
        reinterpret_cast<void*>(menu), reinterpret_cast<void*>(state),
        reinterpret_cast<void*>(m_menuPointerSlot), reinterpret_cast<void*>(m_pointerSlot));
    return true;
}

bool PauseManagerProbe::sample(PauseManagerSnapshot& snapshot) const {
    snapshot = {};
    if (!readMemory(m_menuPointerSlot, &snapshot.menuObject, sizeof(snapshot.menuObject)))
        return false;
    if (snapshot.menuObject) {
        uint8_t open = 0;
        if (!readMemory(snapshot.menuObject + 0x5c, &open, sizeof(open)) || open > 1)
            return false;
        snapshot.menuOpen = open != 0;
    }
    // The menu manager can exist before PauseManager is initialized. A missing
    // diagnostic count must not select the keyboard fallback or hide dialogue.
    if (readMemory(m_pointerSlot, &snapshot.object, sizeof(snapshot.object)) && snapshot.object) {
        uint32_t counts[2]{};
        if (readMemory(snapshot.object + 0x24, counts, sizeof(counts))) {
            snapshot.activeCount = counts[0];
            snapshot.committedCount = counts[1];
            snapshot.countsAvailable = true;
        }
    }
    return true;
}
