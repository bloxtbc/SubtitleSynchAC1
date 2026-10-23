#include "save_activity_monitor.h"

#include <algorithm>
#include <filesystem>
#include <vector>

SaveActivityMonitor::~SaveActivityMonitor()
{
    close();
}

bool SaveActivityMonitor::initialize()
{
    close();

    DWORD required = GetEnvironmentVariableW(L"APPDATA", nullptr, 0);
    if (required == 0)
        return false;

    std::vector<wchar_t> appData(required);
    if (GetEnvironmentVariableW(L"APPDATA", appData.data(), required) == 0)
        return false;

    const std::filesystem::path saveDirectory = std::filesystem::path(appData.data()) /
        L"Ubisoft" / L"Assassin's Creed" / L"Saved Games";
    const DWORD attributes = GetFileAttributesW(saveDirectory.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES || !(attributes & FILE_ATTRIBUTE_DIRECTORY))
        return false;

    m_changeNotification = FindFirstChangeNotificationW(
        saveDirectory.c_str(), FALSE,
        FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_SIZE | FILE_NOTIFY_CHANGE_LAST_WRITE);
    if (m_changeNotification == INVALID_HANDLE_VALUE)
        return false;

    m_directory = saveDirectory.wstring();
    m_lastActivityMs = 0;
    return true;
}

bool SaveActivityMonitor::update(int indicatorDurationMs)
{
    if (m_changeNotification == INVALID_HANDLE_VALUE)
        return false;

    const ULONGLONG now = GetTickCount64();
    const DWORD waitResult = WaitForSingleObject(m_changeNotification, 0);
    if (waitResult == WAIT_OBJECT_0) {
        m_lastActivityMs = now;
        if (!FindNextChangeNotification(m_changeNotification)) {
            close();
            return false;
        }
    } else if (waitResult == WAIT_FAILED) {
        close();
        return false;
    }

    const ULONGLONG duration = static_cast<ULONGLONG>(
        std::clamp(indicatorDurationMs, 0, 10000));
    return duration != 0 && m_lastActivityMs != 0 &&
        now >= m_lastActivityMs && now - m_lastActivityMs < duration;
}

void SaveActivityMonitor::close()
{
    if (m_changeNotification != INVALID_HANDLE_VALUE) {
        FindCloseChangeNotification(m_changeNotification);
        m_changeNotification = INVALID_HANDLE_VALUE;
    }
    m_directory.clear();
    m_lastActivityMs = 0;
}
