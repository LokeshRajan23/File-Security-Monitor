#ifndef FILEMONITOR_FILEMONITOR_H
#define FILEMONITOR_FILEMONITOR_H

#include <memory>
#include <string>
#include <vector>

class IFileMonitor
{
public:
    virtual ~IFileMonitor() = default;

    virtual bool addWatch(const std::string& path) = 0;
    virtual bool removeWatch(const std::string& path) = 0;

    virtual bool configureEmailNotifications(
        const std::string& senderEmail,
        const std::string& appPassword,
        const std::vector<std::string>& recipients
    ) = 0;

    virtual bool isRunning() const = 0;

    virtual void start() = 0;
    virtual void stop() = 0;
};

std::unique_ptr<IFileMonitor> createFileMonitor();

#endif // FILEMONITOR_FILEMONITOR_H