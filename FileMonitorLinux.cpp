#include "FileMonitorLinux.h"

#include <iostream>
#include <stdexcept>
#include <filesystem>
#include <unordered_map>
#include <poll.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>

FileMonitorLinux::FileMonitorLinux()
{
    m_fileDescriptor = inotify_init1(IN_NONBLOCK);

    if (m_fileDescriptor < 0)
    {
        throw std::runtime_error(
            "Failed to initialize inotify instance.");
    }
}

FileMonitorLinux::~FileMonitorLinux()
{
    stop();

    if (m_fileDescriptor >= 0)
    {
        close(m_fileDescriptor);
        m_fileDescriptor = -1;
    }
}

bool FileMonitorLinux::addWatch(const std::string& path)
{
    if (m_running || path.empty())
        return false;

    // Only one directory path is supported.
    m_path = path;

    return true;
}

bool FileMonitorLinux::removeWatch(const std::string& path)
{
    if (m_running || m_path.empty())
        return false;

    // An empty path means remove the currently configured path.
    if (!path.empty() && path != m_path)
        return false;

    m_path.clear();

    return true;
}

bool FileMonitorLinux::configureEmailNotifications(
    const std::string& senderEmail,
    const std::string& appPassword,
    const std::vector<std::string>& recipients)
{
    if (m_running)
        return false;

    if (senderEmail.empty() ||
        appPassword.empty() ||
        recipients.empty() ||
        recipients.size() > 3)
    {
        return false;
    }

    m_emailNotifier = std::make_unique<EmailNotifier>(
        senderEmail,
        appPassword,
        recipients
    );

    return true;
}

bool FileMonitorLinux::isRunning() const
{
    return m_running.load();
}

void FileMonitorLinux::monitorLoop()
{
    std::unordered_map<uint32_t, std::string> pendingRenames;

    const uint32_t watchMask =
        IN_CREATE |
        IN_DELETE |
        IN_CLOSE_WRITE |
        IN_MOVED_FROM |
        IN_MOVED_TO |
        IN_DELETE_SELF |
        IN_MOVE_SELF;

    m_watchDescriptor = inotify_add_watch(
        m_fileDescriptor,
        m_path.c_str(),
        watchMask
    );

    if (m_watchDescriptor < 0)
    {
        std::cerr << "Error: Failed to monitor path: "
                  << m_path << '\n';

        m_running = false;
        return;
    }

    std::cout << "File Monitor Monitoring\n\n";

    struct pollfd pfd{};
    pfd.fd = m_fileDescriptor;
    pfd.events = POLLIN;

    auto sendEmailNotification =
        [this](const std::string& eventType,
               const std::string& filePath,
               const std::string& eventDetail)
    {
        if (!m_emailNotifier)
            return;

        const std::string subject =
            "File Monitor Alert : " + eventType;

        const std::string message =
            "File Monitor Detected an Event\n\n"
            "Event Type : " + eventType + "\n" +
            "File Path : " + filePath + "\n" +
            eventDetail + "\n";

        m_emailNotifier->sendNotification(subject, message);
    };

    auto printEvent =
        [](const std::string& eventType,
           const std::string& filePath)
    {
        std::cout << eventType << ": "
                  << filePath << '\n';
    };

    while (!m_stopRequested)
    {
        const int pollResult = poll(&pfd, 1, 250);

        if (pollResult < 0)
        {
            if (errno == EINTR)
                continue;

            std::cerr << "Error: File monitoring poll failed.\n";
            break;
        }

        if (pollResult == 0)
            continue;

        if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL))
        {
            std::cerr << "Error: File monitoring descriptor failure.\n";
            break;
        }

        if (!(pfd.revents & POLLIN))
            continue;

        char buffer[EVENT_BUFF_LENGTH];

        const ssize_t length = read(
            m_fileDescriptor,
            buffer,
            sizeof(buffer)
        );

        if (length < 0)
        {
            if (errno == EAGAIN || errno == EINTR)
                continue;

            std::cerr << "Error: Failed to read file events.\n";
            break;
        }

        std::size_t position = 0;

        while (position < static_cast<std::size_t>(length))
        {
            const auto* event =
                reinterpret_cast<const struct inotify_event*>(
                    buffer + position
                );

            if (event->mask & IN_IGNORED)
            {
                position += sizeof(struct inotify_event) + event->len;
                continue;
            }

            if (event->mask & (IN_DELETE_SELF | IN_MOVE_SELF))
            {
                std::cerr << "Warning: The monitored directory "
                          << "was deleted or moved.\n";

                m_stopRequested = true;

                position += sizeof(struct inotify_event) + event->len;
                continue;
            }

            if (event->len > 0)
            {
                const std::string fileName(event->name);

                // Ignore common editor backup files.
                if (!fileName.empty() && fileName.back() != '~')
                {
                    const std::string fullPath =
                        m_path + "/" + fileName;

                    // File rename: remember the old name.
                    if (event->mask & IN_MOVED_FROM)
                    {
                        pendingRenames[event->cookie] = fullPath;
                    }

                    // File rename: match the new name using the cookie.
                    else if (event->mask & IN_MOVED_TO)
                    {
                        const auto renameIt =
                            pendingRenames.find(event->cookie);

                        if (renameIt != pendingRenames.end())
                        {
                            const std::string oldPath =
                                renameIt->second;

                            const std::string eventType =
                                "File Renamed";

                            const std::string eventDetail =
                                "Renamed File : " +
                                oldPath + " -> " + fullPath;

                            printEvent(
                                eventType,
                                oldPath + " -> " + fullPath
                            );

                            sendEmailNotification(
                                eventType,
                                fullPath,
                                eventDetail
                            );

                            pendingRenames.erase(renameIt);
                        }
                        else
                        {
                            // File moved into the monitored directory.
                            const std::string eventType =
                                "File Created";

                            printEvent(eventType, fullPath);

                            sendEmailNotification(
                                eventType,
                                fullPath,
                                "Created File : " + fileName
                            );
                        }
                    }

                    else if (event->mask & IN_CREATE)
                    {
                        const std::string eventType =
                            "File Created";

                        printEvent(eventType, fullPath);

                        sendEmailNotification(
                            eventType,
                            fullPath,
                            "Created File : " + fileName
                        );
                    }

                    else if (event->mask & IN_CLOSE_WRITE)
                    {
                        const std::string eventType =
                            "File Altered";

                        printEvent(eventType, fullPath);

                        sendEmailNotification(
                            eventType,
                            fullPath,
                            "Altered File : " + fileName
                        );
                    }

                    else if (event->mask & IN_DELETE)
                    {
                        const std::string eventType =
                            "File Deleted";

                        printEvent(eventType, fullPath);

                        sendEmailNotification(
                            eventType,
                            fullPath,
                            "Deleted File : " + fileName
                        );
                    }

                    else if (event->mask & IN_MOVED_FROM)
                    {
                        // A move out of the monitored directory.
                        const std::string eventType =
                            "File Deleted";

                        printEvent(eventType, fullPath);

                        sendEmailNotification(
                            eventType,
                            fullPath,
                            "Deleted File : " + fileName
                        );

                        pendingRenames.erase(event->cookie);
                    }
                }
            }

            position += sizeof(struct inotify_event) + event->len;
        }
    }

    if (m_watchDescriptor >= 0)
    {
        inotify_rm_watch(m_fileDescriptor, m_watchDescriptor);
        m_watchDescriptor = -1;
    }

    pendingRenames.clear();
    m_running = false;
}

void FileMonitorLinux::start()
{
    if (m_running)
    {
        std::cout << "File Monitor is already running.\n";
        return;
    }

    if (m_path.empty())
    {
        std::cerr
            << "Error: Please add a path before starting File Monitor.\n";
        return;
    }

    if (!std::filesystem::exists(m_path) ||
        !std::filesystem::is_directory(m_path))
    {
        std::cerr
            << "Error: The specified path is not a valid directory.\n";
        return;
    }

    m_stopRequested = false;
    m_running = true;

    try
    {
        m_monitorThread =
            std::thread(&FileMonitorLinux::monitorLoop, this);
    }
    catch (const std::exception& error)
    {
        m_running = false;

        std::cerr << "Error: Unable to start File Monitor: "
                  << error.what() << '\n';
    }
}

void FileMonitorLinux::stop()
{
    m_stopRequested = true;

    if (m_monitorThread.joinable())
    {
        m_monitorThread.join();
    }

    m_running = false;
}