#include "FileMonitorWindows.h"

#ifdef _WIN32

#include <algorithm>
#include <cstring>
#include <iostream>
#include <stdexcept>

namespace
{
    std::wstring toWideString(const std::string& value)
    {
        if (value.empty())
            return {};

        const int size = MultiByteToWideChar(
            CP_UTF8, 0, value.c_str(), -1, nullptr, 0);

        if (size <= 0)
            throw std::runtime_error(
                "Invalid Windows path encoding.");

        std::wstring result(size, L'\0');

        MultiByteToWideChar(
            CP_UTF8, 0, value.c_str(), -1,
            result.data(), size);

        result.pop_back();

        return result;
    }

    std::string toUtf8String(const std::wstring& value)
    {
        if (value.empty())
            return {};

        const int size = WideCharToMultiByte(
            CP_UTF8, 0, value.c_str(), -1,
            nullptr, 0, nullptr, nullptr);

        if (size <= 0)
            return {};

        std::string result(size, '\0');

        WideCharToMultiByte(
            CP_UTF8, 0, value.c_str(), -1,
            result.data(), size, nullptr, nullptr);

        result.pop_back();

        return result;
    }
}

FileMonitorWindows::~FileMonitorWindows()
{
    stop();
}

bool FileMonitorWindows::addWatch(const std::string& path)
{
    if (m_running)
    {
        std::cerr
            << "Stop File Monitor before adding a path.\n";
        return false;
    }

    if (path.empty())
        return false;

    // Replace the previously configured path.
    m_path = path;

    return true;
}

bool FileMonitorWindows::removeWatch(const std::string& path)
{
    if (m_running)
    {
        std::cerr
            << "Stop File Monitor before removing the path.\n";
        return false;
    }

    if (m_path.empty())
        return false;

    // An empty argument removes the current path.
    if (!path.empty() && path != m_path)
        return false;

    m_path.clear();

    return true;
}

bool FileMonitorWindows::configureEmailNotifications(
    const std::string& senderEmail,
    const std::string& appPassword,
    const std::vector<std::string>& recipients)
{
    if (m_running)
    {
        std::cerr
            << "Stop File Monitor before configuring email.\n";
        return false;
    }

    if (senderEmail.empty() ||
        appPassword.empty() ||
        recipients.empty() ||
        recipients.size() > 3)
    {
        m_emailNotifier.reset();
        return false;
    }

    try
    {
        m_emailNotifier = std::make_unique<EmailNotifier>(
            senderEmail,
            appPassword,
            recipients);

        return true;
    }
    catch (const std::exception& ex)
    {
        m_emailNotifier.reset();

        std::cerr << "Email configuration failed: "
                  << ex.what() << '\n';

        return false;
    }
}

bool FileMonitorWindows::isRunning() const
{
    return m_running.load();
}

void FileMonitorWindows::start()
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

    auto watch = std::make_unique<WatchEntry>();

    try
    {
        watch->path = toWideString(m_path);
    }
    catch (const std::exception& ex)
    {
        std::cerr << "Error: " << ex.what() << '\n';
        return;
    }

    watch->directoryHandle = CreateFileW(
        watch->path.c_str(),
        FILE_LIST_DIRECTORY,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
        nullptr);

    if (watch->directoryHandle == INVALID_HANDLE_VALUE)
    {
        std::cerr << "Error: Unable to access directory: "
                  << m_path
                  << " (Windows error: "
                  << GetLastError() << ")\n";
        return;
    }

    watch->eventHandle = CreateEventW(
        nullptr, TRUE, FALSE, nullptr);

    if (!watch->eventHandle)
    {
        std::cerr << "Error: Unable to create monitoring event.\n";

        CloseHandle(watch->directoryHandle);
        return;
    }

    m_running = true;

    try
    {
        watch->monitorThread = std::thread(
            &FileMonitorWindows::monitorDirectory,
            this,
            watch.get());
    }
    catch (const std::exception& ex)
    {
        m_running = false;

        CloseHandle(watch->eventHandle);
        CloseHandle(watch->directoryHandle);

        std::cerr << "Unable to start monitoring thread: "
                  << ex.what() << '\n';
        return;
    }

    m_watch = std::move(watch);

    std::cout << "\nFile Monitor Monitoring\n\n";
}

void FileMonitorWindows::monitorDirectory(WatchEntry* watch)
{
    BYTE buffer[64 * 1024];

    std::wstring oldFileName;

    while (m_running)
    {
        std::memset(
            &watch->overlapped, 0,
            sizeof(watch->overlapped));

        ResetEvent(watch->eventHandle);
        watch->overlapped.hEvent = watch->eventHandle;

        const BOOL result = ReadDirectoryChangesW(
            watch->directoryHandle,
            buffer,
            sizeof(buffer),
            FALSE,
            FILE_NOTIFY_CHANGE_FILE_NAME |
                FILE_NOTIFY_CHANGE_DIR_NAME |
                FILE_NOTIFY_CHANGE_LAST_WRITE |
                FILE_NOTIFY_CHANGE_SIZE |
                FILE_NOTIFY_CHANGE_CREATION,
            nullptr,
            &watch->overlapped,
            nullptr);

        if (!result && GetLastError() != ERROR_IO_PENDING)
        {
            if (m_running)
            {
                std::cerr << "Directory monitoring error: "
                          << GetLastError() << '\n';
            }

            break;
        }

        const DWORD waitResult = WaitForSingleObject(
            watch->eventHandle, INFINITE);

        if (waitResult != WAIT_OBJECT_0)
            break;

        DWORD bytesTransferred = 0;

        if (!GetOverlappedResult(
                watch->directoryHandle,
                &watch->overlapped,
                &bytesTransferred,
                FALSE))
        {
            if (!m_running)
                break;

            std::cerr << "Unable to retrieve directory changes: "
                      << GetLastError() << '\n';
            break;
        }

        if (!m_running || bytesTransferred == 0)
            continue;

        DWORD offset = 0;

        while (offset < bytesTransferred)
        {
            auto* notification =
                reinterpret_cast<FILE_NOTIFY_INFORMATION*>(
                    buffer + offset);

            const std::wstring fileName(
                notification->FileName,
                notification->FileNameLength / sizeof(WCHAR));

            const std::string fileNameUtf8 =
                toUtf8String(fileName);

            const std::string filePath =
                toUtf8String(watch->path + L"\\" + fileName);

            std::string eventType;
            std::string eventDetail;
            std::string logPath = filePath;

            switch (notification->Action)
            {
                case FILE_ACTION_ADDED:
                    eventType = "File Created";
                    eventDetail =
                        "Created File : " + fileNameUtf8;
                    break;

                case FILE_ACTION_REMOVED:
                    eventType = "File Deleted";
                    eventDetail =
                        "Deleted File : " + fileNameUtf8;
                    break;

                case FILE_ACTION_MODIFIED:
                    eventType = "File Altered";
                    eventDetail =
                        "Altered File : " + fileNameUtf8;
                    break;

                case FILE_ACTION_RENAMED_OLD_NAME:
                    oldFileName = fileName;
                    break;

                case FILE_ACTION_RENAMED_NEW_NAME:
                {
                    if (!oldFileName.empty())
                    {
                        const std::string oldNameUtf8 =
                            toUtf8String(oldFileName);

                        const std::string oldFullPath =
                            toUtf8String(watch->path + L"\\" + oldFileName);

                        eventType = "File Renamed";

                        eventDetail =
                            "Renamed File : " +
                            oldNameUtf8 + " -> " +
                            fileNameUtf8;

                        logPath = oldFullPath + " -> " + filePath;

                        oldFileName.clear();
                    }
                    break;
                }

                default:
                    break;
            }

            if (!eventType.empty())
            {
                // Console log: one line for each detected event.
                std::cout << eventType
                          << ": " << logPath << '\n';

                // Email notification is optional.
                if (m_emailNotifier)
                {
                    const std::string subject =
                        "File Monitor Alert : " + eventType;

                    const std::string message =
                        "File Monitor Detected an Event\n\n"
                        "Event Type : " + eventType + "\n" +
                        "File Path : " + filePath + "\n" +
                        eventDetail + "\n";

                    m_emailNotifier->sendNotification(
                        subject,
                        message);
                }
            }

            if (notification->NextEntryOffset == 0)
                break;

            offset += notification->NextEntryOffset;
        }
    }
}

void FileMonitorWindows::stop()
{
    if (!m_running.exchange(false))
        return;

    if (m_watch)
    {
        if (m_watch->directoryHandle != INVALID_HANDLE_VALUE)
        {
            CancelIoEx(
                m_watch->directoryHandle,
                &m_watch->overlapped);
        }

        if (m_watch->eventHandle)
            SetEvent(m_watch->eventHandle);

        if (m_watch->monitorThread.joinable())
            m_watch->monitorThread.join();

        if (m_watch->eventHandle)
            CloseHandle(m_watch->eventHandle);

        if (m_watch->directoryHandle != INVALID_HANDLE_VALUE)
            CloseHandle(m_watch->directoryHandle);

        m_watch.reset();
    }

    std::cout << "\nFile Monitor stopped.\n";
}

#endif // _WIN32