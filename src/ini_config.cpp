#include <Windows.h>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

#include "ini_config.h"
#include "ini.h"
#include "subtitle_settings.h"


std::string getIniPath()
{
    HMODULE module = nullptr;

    GetModuleHandleExA(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
        reinterpret_cast<LPCSTR>(&getIniPath),
        &module);

    char path[MAX_PATH];
    GetModuleFileNameA(module, path, MAX_PATH);

    std::filesystem::path p(path);

    return (p.parent_path() / "SubtitleSynchAC1.ini").string();
}

SubtitleConfig& SubtitleConfig::instance()
{
    static SubtitleConfig cfg;
    return cfg;
}

namespace {
float clampFloat(float value, float low, float high, float fallback)
{
    if (!(value == value)) return fallback;  // NaN
    return value < low ? low : value > high ? high : value;
}

std::string number(float value)
{
    char text[32];
    std::snprintf(text, sizeof text, "%.4g", value);
    return text;
}
}

bool SubtitleConfig::load()
{
    // Layout details are fixed. They match the values the packaged INI used
    // before v5.20; the old PaddingY=-25 was always drawn as 0.
    g_subtitleSettings.autoPosition = true;
    g_subtitleSettings.autoScale = true;
    g_subtitleSettings.referenceHeight = 1080.0f;
    g_subtitleSettings.fontSize = 30.0f;
    g_subtitleSettings.padding = ImVec2(8.0f, 0.0f);
    g_subtitleSettings.resumeDelayMs = 500;
    g_subtitleSettings.saveIndicatorLift = 90.0f;
    g_subtitleSettings.saveIndicatorDurationMs = 3300;
    g_subtitleSettings.backgroundColor.x = 0.0f;
    g_subtitleSettings.backgroundColor.y = 0.0f;
    g_subtitleSettings.backgroundColor.z = 0.0f;
    try
    {
        const std::string& path = getIniPath();
        inih::INIReader ini(path);

        g_subtitleSettings.showSubtitles = ini.Get<bool>("Subtitle", "ShowSubtitles", true);
        g_subtitleSettings.showSoundCaptions = ini.Get<bool>("Subtitle", "ShowSoundCaptions", true);
        g_subtitleSettings.scale = clampFloat(ini.Get<float>("Subtitle", "SubtitleFontScale", 1.0f), 0.5f, 3.0f, 1.0f);
        g_subtitleSettings.maxWidthPercent = clampFloat(ini.Get<float>("Subtitle", "MaxWidthPercent", 65.0f), 20.0f, 100.0f, 65.0f);
        g_subtitleSettings.bottomMargin = clampFloat(ini.Get<float>("Subtitle", "BottomMargin", 100.0f), 0.0f, 1000.0f, 100.0f);
        g_subtitleSettings.tailExtensionMs = ini.Get<int>("Subtitle", "TailExtensionMs", 1500);
        if (g_subtitleSettings.tailExtensionMs < 0) g_subtitleSettings.tailExtensionMs = 0;
        if (g_subtitleSettings.tailExtensionMs > 10000) g_subtitleSettings.tailExtensionMs = 10000;

        g_subtitleSettings.textColor.x = ini.Get<float>("Subtitle", "SubtitleColorR", 1.0f);
        g_subtitleSettings.textColor.y = ini.Get<float>("Subtitle", "SubtitleColorG", 1.0f);
        g_subtitleSettings.textColor.z = ini.Get<float>("Subtitle", "SubtitleColorB", 1.0f);
        g_subtitleSettings.textColor.w = ini.Get<float>("Subtitle", "SubtitleColorA", 1.0f);
        g_subtitleSettings.secondaryTextColor.x = ini.Get<float>("Subtitle", "SecondaryColorR", 0.72f);
        g_subtitleSettings.secondaryTextColor.y = ini.Get<float>("Subtitle", "SecondaryColorG", 0.72f);
        g_subtitleSettings.secondaryTextColor.z = ini.Get<float>("Subtitle", "SecondaryColorB", 0.72f);
        g_subtitleSettings.secondaryTextColor.w = ini.Get<float>("Subtitle", "SecondaryColorA", 1.0f);
        g_subtitleSettings.backgroundColor.w = ini.Get<float>("Subtitle", "BackgroundColorA", 0.5f);

        return true;
    }
    catch (...)
    {
        return false;
    }
}

bool SubtitleConfig::save() const
{
    const std::string path = getIniPath();
    // Keep every other section as it is; [Subtitle] gets only the current keys.
    std::string others;
    std::string startOnLaunch = "0";
    try
    {
        inih::INIReader ini(path);
        for (const auto& section : ini.Sections()) {
            if (section == "Subtitle") continue;
            if (section == "Diagnostics") {
                startOnLaunch = ini.Get<std::string>("Diagnostics", "StartOnLaunch", std::string("0"));
                continue;
            }
            others += "[" + section + "]\n";
            for (const auto& key : ini.Keys(section))
                others += key + "=" + ini.Get<std::string>(section, key) + "\n";
        }
    }
    catch (...)
    {
        // Missing or unreadable file: write the defaults for the other sections.
    }

    const auto& s = g_subtitleSettings;
    std::string text;
    text += "[Subtitle]\n";
    text += "; 1 = show subtitles, 0 = hide all subtitles.\n";
    text += "ShowSubtitles=" + std::string(s.showSubtitles ? "1" : "0") + "\n";
    text += "; Show bracketed sound descriptions such as [laughs] (1 = on, 0 = off).\n";
    text += "ShowSoundCaptions=" + std::string(s.showSoundCaptions ? "1" : "0") + "\n";
    text += "; Subtitle size multiplier (0.5 - 3).\n";
    text += "SubtitleFontScale=" + number(s.scale) + "\n";
    text += "; Largest subtitle width in percent of the screen width (20 - 100).\n";
    text += "MaxWidthPercent=" + number(s.maxWidthPercent) + "\n";
    text += "; Distance of the subtitle from the bottom of the screen, in 1080p pixels.\n";
    text += "BottomMargin=" + number(s.bottomMargin) + "\n";
    text += "; The subtitle stays this many ms after the line ends (0 - 10000).\n";
    text += "TailExtensionMs=" + std::to_string(s.tailExtensionMs) + "\n";
    text += "; Main subtitle colour (0 - 1).\n";
    text += "SubtitleColorR=" + number(s.textColor.x) + "\n";
    text += "SubtitleColorG=" + number(s.textColor.y) + "\n";
    text += "SubtitleColorB=" + number(s.textColor.z) + "\n";
    text += "SubtitleColorA=" + number(s.textColor.w) + "\n";
    text += "; Ambient subtitle colour (citizens, guards, preachers), greyer than the main subtitle.\n";
    text += "SecondaryColorR=" + number(s.secondaryTextColor.x) + "\n";
    text += "SecondaryColorG=" + number(s.secondaryTextColor.y) + "\n";
    text += "SecondaryColorB=" + number(s.secondaryTextColor.z) + "\n";
    text += "SecondaryColorA=" + number(s.secondaryTextColor.w) + "\n";
    text += "; Background opacity behind the text (0 = none, 1 = solid black).\n";
    text += "BackgroundColorA=" + number(s.backgroundColor.w) + "\n";
    // The diagnostic log is a tester-only setting: keep it only where it is on.
    if (startOnLaunch == "1")
        text += "[Diagnostics]\nStartOnLaunch=1\n";
    text += others;

    const std::string temp = path + ".tmp";
    {
        std::ofstream out(temp, std::ios::binary | std::ios::trunc);
        if (!out) return false;
        out << text;
        if (!out.flush()) return false;
    }
    if (!MoveFileExA(temp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        DeleteFileA(temp.c_str());
        return false;
    }
    return true;
}
