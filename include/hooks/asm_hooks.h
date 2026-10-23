#pragma once
#include <cstdint>
#include "audio_resolver.h"
bool applyASMPatches();
namespace AudioHook {
bool Install(uintptr_t address);
// address: the instruction right after the leaf play CALL; micHeadSlot: the
// engine's microphone list head (0 = distance unknown).
bool InstallLeaf(uintptr_t address, uintptr_t micHeadSlot);
bool leafActive();
// Current listener distance and level of a playing voice instance (from the
// leaf hook). Safe on any thread: reads go through ReadProcessMemory.
VoiceLevel readVoiceLevelNow(uintptr_t instance);
}
namespace PauseHook { bool Install(uintptr_t address, unsigned playing); bool InstallGameHooks(); }
