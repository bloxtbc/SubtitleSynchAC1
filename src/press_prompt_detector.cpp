#include "press_prompt_detector.h"

#include <string>

bool isInteractionPrompt(std::wstring_view text) {
    if (text.size() < 24 || text.size() > 256) return false;
    std::wstring normalized;
    normalized.reserve(text.size());
    for (wchar_t c : text) {
        if (c >= L'a' && c <= L'z') c -= L'a' - L'A';
        if (c == L'\t' || c == L'\r' || c == L'\n') c = L' ';
        if (c == L' ' && !normalized.empty() && normalized.back() == L' ') continue;
        normalized.push_back(c);
    }
    return normalized.find(L"PRESS ANY BUTTON TO INTERACT") != std::wstring::npos ||
        normalized.find(L"PRESS ANY KEY TO INTERACT") != std::wstring::npos;
}

void InteractionPromptTracker::observe(std::wstring_view text, uint64_t now) {
    if (!isInteractionPrompt(text)) return;
    m_lastSeen.store(now, std::memory_order_relaxed);
    m_seen.store(true, std::memory_order_release);
}

bool InteractionPromptTracker::visible(uint64_t now) const {
    if (!m_seen.load(std::memory_order_acquire)) return false;
    const auto seen = m_lastSeen.load(std::memory_order_relaxed);
    return now >= seen && now - seen <= HoldMs;
}
