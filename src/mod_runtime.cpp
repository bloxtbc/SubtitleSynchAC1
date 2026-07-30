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

namespace
{
    // Scoped x87/SSE control word guard.
    //
    // D3D9 puts the x87 FPU into single precision unless the device is created
    // with D3DCREATE_FPU_PRESERVE, which makes std::stod parse durations
    // incorrectly. The guard is needed - but only around the code that actually
    // parses doubles, not around the whole per-frame update.
    //
    // Restore order matters: on x86 _controlfp_s writes MXCSR as well, so MXCSR
    // is restored last.
    struct ScopedFpuGuard
    {
        unsigned int mxcsr;
        unsigned int cw;

        ScopedFpuGuard() : mxcsr(_mm_getcsr()), cw(0)
        {
            _controlfp_s(&cw, 0, 0);
            _mm_setcsr((mxcsr & ~_MM_ROUND_MASK) | _MM_ROUND_NEAREST);
            _controlfp_s(nullptr, _RC_NEAR, _MCW_RC);
        }

        ~ScopedFpuGuard()
        {
            _controlfp_s(nullptr, cw, _MCW_RC);
            _mm_setcsr(mxcsr);
        }
    };
}

void ModRuntime::update(void) {

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
}


bool ModRuntime::handleVoiceline(uint32_t id)
{
    // extractDuration() -> std::stod and the duration<double> conversion below
    // are the only double parsing in the hot path. Runs on audio events only.
    ScopedFpuGuard fpu;

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
