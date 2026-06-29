#pragma once

#include <imgui.h>

#include "subtitle_settings.h"

class SubtitleConfig
{
public:
    static SubtitleConfig& instance();

    bool load();
    bool save() const;

    SubtitleConfig() = default;
};