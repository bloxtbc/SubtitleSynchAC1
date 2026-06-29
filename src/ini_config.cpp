#include <Windows.h>
#include <filesystem>

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

bool SubtitleConfig::load()
{
    try
    {
        const std::string& path = getIniPath();
        inih::INIReader ini(path);

        g_subtitleSettings.autoPosition = ini.Get<bool>("Subtitle", "AutoPosition", true);

        g_subtitleSettings.position.x = ini.Get<float>("Subtitle", "SubtitlePosX", 960.0f);
        g_subtitleSettings.position.y = ini.Get<float>("Subtitle", "SubtitlePosY", 980.0f);

        g_subtitleSettings.padding.x = ini.Get<float>("Subtitle", "PaddingX", 8);
        g_subtitleSettings.padding.y = ini.Get<float>("Subtitle", "PaddingY", -25);

        g_subtitleSettings.scale = ini.Get<float>("Subtitle", "SubtitleFontScale", 1.0f);

        g_subtitleSettings.textColor.x = ini.Get<float>("Subtitle", "SubtitleColorR", 1.0f);
        g_subtitleSettings.textColor.y = ini.Get<float>("Subtitle", "SubtitleColorG", 1.0f);
        g_subtitleSettings.textColor.z = ini.Get<float>("Subtitle", "SubtitleColorB", 1.0f);
        g_subtitleSettings.textColor.w = ini.Get<float>("Subtitle", "SubtitleColorA", 1.0f);

        g_subtitleSettings.backgroundColor.x = ini.Get<float>("Subtitle", "BackgroundColorR", 0.0f);
        g_subtitleSettings.backgroundColor.y = ini.Get<float>("Subtitle", "BackgroundColorG", 0.0f);
        g_subtitleSettings.backgroundColor.z = ini.Get<float>("Subtitle", "BackgroundColorB", 0.0f);
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
    try
    {
        const std::string& path = getIniPath();
        inih::INIReader ini;

        ini.InsertEntry("Subtitle", "AutoPosition", g_subtitleSettings.autoPosition);

        ini.InsertEntry("Subtitle", "SubtitlePosX", g_subtitleSettings.position.x);
        ini.InsertEntry("Subtitle", "SubtitlePosY", g_subtitleSettings.position.y);

        ini.InsertEntry("Subtitle", "PaddingX", g_subtitleSettings.padding.x);
        ini.InsertEntry("Subtitle", "PaddingY", g_subtitleSettings.padding.y);

        ini.InsertEntry("Subtitle", "SubtitleFontScale", g_subtitleSettings.scale);

        ini.InsertEntry("Subtitle", "SubtitleColorR", g_subtitleSettings.textColor.x);
        ini.InsertEntry("Subtitle", "SubtitleColorG", g_subtitleSettings.textColor.y);
        ini.InsertEntry("Subtitle", "SubtitleColorB", g_subtitleSettings.textColor.z);
        ini.InsertEntry("Subtitle", "SubtitleColorA", g_subtitleSettings.textColor.w);

        ini.InsertEntry("Subtitle", "BackgroundColorR", g_subtitleSettings.backgroundColor.x);
        ini.InsertEntry("Subtitle", "BackgroundColorG", g_subtitleSettings.backgroundColor.y);
        ini.InsertEntry("Subtitle", "BackgroundColorB", g_subtitleSettings.backgroundColor.z);
        ini.InsertEntry("Subtitle", "BackgroundColorA", g_subtitleSettings.backgroundColor.w);

        inih::INIWriter::write(path, ini, true);

        return true;
    }
    catch (...)
    {
        return false;
    }
}