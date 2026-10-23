#pragma once

#include <atomic>
#include <cstdint>
#include <string_view>

// Match the actual interaction text passed to the game's font renderer.
// A scene's red pixels and the title screen's Continue prompt are irrelevant.
bool isInteractionPrompt(std::wstring_view text);

class InteractionPromptTracker {
public:
    static constexpr uint64_t HoldMs = 250;
    void observe(std::wstring_view text, uint64_t now);
    bool visible(uint64_t now) const;
private:
    std::atomic<uint64_t> m_lastSeen{0};
    std::atomic<bool> m_seen{false};
};
