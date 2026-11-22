#ifndef FILEMONITOR_FILEMONITORWINDOWS_H
#define FILEMONITOR_FILEMONITORWINDOWS_H

#ifdef _WIN32

#include "FileMonitor.h"
#include "EmailNotifier.h"

#include <windows.h>
#include <atomic>
#include <memory>
#include <string>
#include <thread>
#include <vector>

class FileMonitorWindows final : public IFileMonitor
{
private:
    struct WatchEntry
    {
        std::wstring path;
        HANDLE directoryHandle = INVALID_HANDLE_VALUE;
        HANDLE eventHandle = nullptr;
        OVERLAPPED overlapped{};
        std::thread monitorThread;
    };

    std::string m_path;
    std::unique_ptr<WatchEntry> m_watch;

    std::atomic<bool> m_running{false};

    std::unique_ptr<EmailNotifier> m_emailNotifier;

    void monitorDirectory(WatchEntry* watch);

public:
    FileMonitorWindows() = default;
    ~FileMonitorWindows() override;

    bool addWatch(const std::string& path) override;
    bool removeWatch(const std::string& path) override;

    bool configureEmailNotifications(
        const std::string& senderEmail,
        const std::string& appPassword,
        const std::vector<std::string>& recipients
    ) override;

    bool isRunning() const override;

    void start() override;
    void stop() override;
};

#endif // _WIN32

#endif // FILEMONITOR_FILEMONITORWINDOWS_H