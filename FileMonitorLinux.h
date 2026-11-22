#ifndef FILEMONITOR_FILEMONITORLINUX_H
#define FILEMONITOR_FILEMONITORLINUX_H

#include "FileMonitor.h"
#include "EmailNotifier.h"

#include <memory>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <sys/inotify.h>

class FileMonitorLinux final : public IFileMonitor
{
public:
    FileMonitorLinux();
    ~FileMonitorLinux() override;

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

private:
    void monitorLoop();

    static constexpr int EVENT_BUFF_LENGTH =
        1024 * (sizeof(struct inotify_event) + 16);

    std::string m_path;

    int m_fileDescriptor{-1};
    int m_watchDescriptor{-1};

    std::thread m_monitorThread;

    std::atomic<bool> m_stopRequested{false};
    std::atomic<bool> m_running{false};

    std::unique_ptr<EmailNotifier> m_emailNotifier;
};

#endif // FILEMONITOR_FILEMONITORLINUX_H