#include <xmmintrin.h>
#include <filesystem>

#include "mod_runtime.h"
#include "hooks/asm_hooks.h"
#include "globals.h"

void ModRuntime::init(void) {
    applyASMPatches();

    std::string baseDir = std::filesystem::path(dllPath).parent_path().string();
    std::string jsonPath = baseDir + "\\subtitles.json";

    m_engine.load(jsonPath);
    m_overlay.init();
}

void ModRuntime::update(void) {
    //some float bullshit for msvc :(
    unsigned int mxcsr_saved = _mm_getcsr();
    unsigned int cw_saved = 0;
    _controlfp_s(&cw_saved, 0, 0);

    _mm_setcsr((mxcsr_saved & ~_MM_ROUND_MASK) | _MM_ROUND_NEAREST);
    _controlfp_s(nullptr, _RC_NEAR, _MCW_RC);

    static uint32_t last = 0;

    while (true)
    {
        uint32_t id = g_AudioQueue.pop();
        
        if (id == 0)
            break;
        
        if (id == last)
            continue;

        last = id;
        handleVoiceline(id);
    }

    m_runtime.update();

    bool visible = m_runtime.active();
    std::string text = m_runtime.currentText();
    if (m_overlay.isDebugWindowVisible()) {
        visible = m_overlay.isDebugVisible();
        text = m_overlay.debugText();
    }

    m_overlay.setVisible(visible);
    m_overlay.setText(text);

    m_overlay.render(m_window);
    m_runtime.update();

    _mm_setcsr(mxcsr_saved);
    _controlfp_s(nullptr, cw_saved, _MCW_RC);
}


bool ModRuntime::handleVoiceline(uint32_t id)
{
    auto segments = m_engine.getSegments(id);

    if (segments.empty()) {
        auto raw = m_engine.getRaw(id);
        return false;
    }

    auto raw = m_engine.getRaw(id);
    auto duration = SubtitleEngine::extractDuration(raw);

    m_runtime.start(
        segments,
        std::chrono::duration_cast<SubtitleRuntime::clock::duration>(
            std::chrono::duration<double>(duration)
        )
    );

    m_overlay.setSegments(segments);
    m_overlay.setVisible(true);

    return true;
}