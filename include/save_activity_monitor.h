#pragma once

#include <Windows.h>
#include <string>

class SaveActivityMonitor {
public:
    ~SaveActivityMonitor();

    bool initialize();
    bool update(int indicatorDurationMs);
    bool watching() const { return m_changeNotification != INVALID_HANDLE_VALUE; }
    const std::wstring& directory() const { return m_directory; }

private:
    void close();

    HANDLE m_changeNotification = INVALID_HANDLE_VALUE;
    std::wstring m_directory;
    ULONGLONG m_lastActivityMs = 0;
};
