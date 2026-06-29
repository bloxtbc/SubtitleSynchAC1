#pragma once

#include <cstdint>
#include <Windows.h>

#include "subtitle_runtime.h"
#include "audio_system.h"
#include "overlay_ui.h"
#include "ini_config.h"

class ModRuntime {
public:
    void init();
    void update();

    void onPause(bool paused);
    void onFocus(bool focused);

    bool handleVoiceline(uint32_t id);

    AudioQueue m_audio;
    SubtitleEngine m_engine;
    SubtitleOverlay m_overlay;
    SubtitleRuntime m_runtime;
    HWND m_window;

    bool m_paused = false;
};